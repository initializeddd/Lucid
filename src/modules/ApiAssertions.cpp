#include "modules/ApiAssertions.hpp"
#include "editor/json/Json.hpp"
#include <charconv>
#include <cmath>
#include <optional>
namespace lucid::modules {
namespace {
constexpr std::size_t maxSpec=256U*1024U, maxAssertions=64, maxExpected=4096, maxPointer=1024, maxBody=4U*1024U*1024U, maxJsonBody=1024U*1024U;
bool pointerValid(std::string_view p) {
    if (p.size()>maxPointer || (!p.empty() && p[0]!='/')) return false;
    for(std::size_t i=0;i<p.size();++i) if(p[i]=='~' && (++i==p.size() || (p[i]!='0' && p[i]!='1'))) return false;
    return p.find('\0')==p.npos;
}
const editor::JsonValue* follow(const editor::JsonValue& root, std::string_view pointer) {
    const auto* value=&root;
    for(std::size_t start=0;start<pointer.size();) {
        const auto end=pointer.find('/',start+1); const auto encoded=pointer.substr(start+1,end==pointer.npos ? pointer.npos : end-start-1);
        std::string token; token.reserve(encoded.size());
        for(std::size_t i=0;i<encoded.size();++i) { if(encoded[i]=='~') { token+=encoded[++i]=='0' ? '~' : '/'; } else token+=encoded[i]; }
        if(value->type==editor::JsonType::Object) {
            const editor::JsonValue* found=nullptr;
            for(const auto& child:value->children) if(child.key==token) { if(found) return nullptr; found=&child; }
            value=found;
        } else if(value->type==editor::JsonType::Array) {
            if(token.empty() || (token.size()>1 && token[0]=='0')) return nullptr;
            std::size_t index=0; const auto parsed=std::from_chars(token.data(),token.data()+token.size(),index);
            if(parsed.ec!=std::errc{} || parsed.ptr!=token.data()+token.size() || index>=value->children.size()) return nullptr;
            value=&value->children[index];
        } else return nullptr;
        if(!value) return nullptr;
        if(end==pointer.npos) break; start=end;
    }
    return value;
}
}
bool parseApiAssertions(std::string_view json,std::vector<ApiAssertion>& assertions,std::string& error) {
    assertions.clear();error.clear();
    const auto fail=[&](const char* message){error=message;return false;};
    if(json.size()>maxSpec) return fail("Assertion specification exceeds size limit");
    editor::JsonError parseError; const auto root=editor::parseJson(json,parseError);
    if(!root || root->type!=editor::JsonType::Object || root->children.size()!=1 || root->children[0].key!="assertions" || root->children[0].type!=editor::JsonType::Array || root->children[0].children.empty() || root->children[0].children.size()>maxAssertions) return fail("Expected a bounded assertions array");
    std::vector<ApiAssertion> parsed;
    for(const auto& entry:root->children[0].children) {
        if(entry.type!=editor::JsonType::Object) return fail("Invalid assertion");
        const editor::JsonValue *type=nullptr,*value=nullptr,*pointer=nullptr;
        for(const auto& field:entry.children) {
            if(field.key=="type" && !type) type=&field;
            else if(field.key=="value" && !value) value=&field;
            else if(field.key=="pointer" && !pointer) pointer=&field;
            else return fail("Unknown or duplicate assertion field");
        }
        if(!type || type->type!=editor::JsonType::String || !value) return fail("Assertion type and value are required");
        ApiAssertion a{};
        if(type->stringValue=="status") {
            if(pointer || value->type!=editor::JsonType::Number || !std::isfinite(value->numberValue) || value->numberValue<100 || value->numberValue>599 || std::floor(value->numberValue)!=value->numberValue) return fail("Invalid status assertion");
            a.kind=ApiAssertionKind::Status; a.status=static_cast<long>(value->numberValue);
        } else if(type->stringValue=="body_contains") {
            if(pointer || value->type!=editor::JsonType::String || value->stringValue.empty() || value->stringValue.size()>maxExpected) return fail("Invalid body assertion");
            a.kind=ApiAssertionKind::BodyContains; a.expected=value->stringValue;
        } else if(type->stringValue=="json_string_equals") {
            if(!pointer || pointer->type!=editor::JsonType::String || !pointerValid(pointer->stringValue) || value->type!=editor::JsonType::String || value->stringValue.size()>maxExpected) return fail("Invalid JSON string assertion");
            a.kind=ApiAssertionKind::JsonStringEquals; a.pointer=pointer->stringValue; a.expected=value->stringValue;
        } else return fail("Unsupported assertion type");
        parsed.push_back(std::move(a));
    }
    assertions=std::move(parsed);return true;
}
ApiAssertionReport evaluateApiAssertions(const ApiResponse& response,const std::vector<ApiAssertion>& assertions,const std::atomic_bool* cancel) {
    ApiAssertionReport report; const auto fail=[&](const char* message){report.results.clear();report.error=message;return report;};
    if(cancel && cancel->load()) return fail("Assertion evaluation cancelled");
    if(!response.error.empty() || response.status<100 || response.status>599 || response.body.size()>maxBody || response.headers.size()>65536) return fail("A complete bounded HTTP response is required");
    if(assertions.empty() || assertions.size()>maxAssertions) return fail("A bounded assertion set is required");
    bool needsJson=false;
    for(const auto& a:assertions) {
        switch(a.kind) {
        case ApiAssertionKind::Status: if(a.status<100 || a.status>599 || !a.expected.empty() || !a.pointer.empty()) return fail("Invalid assertion set"); break;
        case ApiAssertionKind::BodyContains: if(a.expected.empty() || a.expected.size()>maxExpected || !a.pointer.empty()) return fail("Invalid assertion set"); break;
        case ApiAssertionKind::JsonStringEquals: if(a.expected.size()>maxExpected || !pointerValid(a.pointer)) return fail("Invalid assertion set"); needsJson=true; break;
        default: return fail("Invalid assertion set");
        }
    }
    std::optional<editor::JsonValue> root;
    if(needsJson && response.body.size()<=maxJsonBody) { editor::JsonError error; root=editor::parseJson(response.body,error); }
    for(const auto& a:assertions) {
        if(cancel && cancel->load()) return fail("Assertion evaluation cancelled");
        ApiAssertionResult result{a.kind,false,{}};
        switch(a.kind) {
        case ApiAssertionKind::Status: result.passed=response.status==a.status; result.detail=result.passed?"Status matched":"Status did not match"; break;
        case ApiAssertionKind::BodyContains: result.passed=response.body.find(a.expected)!=std::string::npos; result.detail=result.passed?"Body contained expected text":"Body did not contain expected text"; break;
        case ApiAssertionKind::JsonStringEquals: {
            const auto* value=root ? follow(*root,a.pointer) : nullptr;
            result.passed=value && value->type==editor::JsonType::String && value->stringValue==a.expected;
            result.detail=result.passed?"JSON string matched":"JSON string did not match or was unavailable"; break;
        }
        }
        report.results.push_back(std::move(result));
    }
    return report;
}
}
