#include "modules/ApiAssertions.hpp"
#include "editor/json/Json.hpp"
#include <charconv>
#include <cmath>
#include <optional>
#include <algorithm>
namespace lucid::modules {
namespace {
constexpr std::size_t maxSpec=256U*1024U, maxAssertions=64, maxExpected=4096, maxPointer=1024, maxBody=4U*1024U*1024U, maxJsonBody=1024U*1024U;
struct Decimal { std::string digits; long exponent=0; bool negative=false; };
std::optional<Decimal> decimal(std::string_view s) {
    if(s.empty() || s.size()>maxExpected) return std::nullopt;
    Decimal result; std::size_t i=0; if(s[i]=='-') {result.negative=true; if(++i==s.size()) return std::nullopt;}
    const auto integerStart=i;
    while(i<s.size() && s[i]>='0' && s[i]<='9') result.digits+=s[i++];
    if(i==integerStart || (i-integerStart>1 && s[integerStart]=='0')) return std::nullopt;
    long fraction=0;
    if(i<s.size() && s[i]=='.') { ++i; const auto start=i; while(i<s.size() && s[i]>='0' && s[i]<='9') {result.digits+=s[i++]; ++fraction;} if(i==start) return std::nullopt; }
    long exponent=0;
    if(i<s.size() && (s[i]=='e' || s[i]=='E')) {
        ++i; bool negative=false; if(i<s.size() && (s[i]=='+' || s[i]=='-')) {negative=s[i]=='-'; ++i;}
        const auto start=i;
        while(i<s.size() && s[i]>='0' && s[i]<='9') { if(exponent>1000000) return std::nullopt; exponent=exponent*10+(s[i++]-'0'); }
        if(i==start || exponent>1000000) return std::nullopt;
        if(negative) exponent=-exponent;
    }
    if(i!=s.size()) return std::nullopt;
    const auto first=result.digits.find_first_not_of('0');
    if(first==std::string::npos) return Decimal{"0",0,false};
    result.digits.erase(0,first); result.exponent=exponent-fraction;
    while(result.digits.size()>1 && result.digits.back()=='0') {result.digits.pop_back(); ++result.exponent;}
    return result;
}
std::string_view numberAt(std::string_view source,std::size_t offset) {
    if(offset>=source.size()) return {};
    auto end=offset;
    while(end<source.size() && ((source[end]>='0' && source[end]<='9') || source[end]=='-' || source[end]=='+' || source[end]=='.' || source[end]=='e' || source[end]=='E')) ++end;
    return source.substr(offset,end-offset);
}
bool headerNameValid(std::string_view s) {
    if(s.empty() || s.size()>256) return false;
    for(unsigned char c:s) if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || std::string_view("!#$%&'*+-.^_`|~").find(c)!=std::string_view::npos)) return false;
    return true;
}
bool headerValueValid(std::string_view s) {
    return s.size()<=maxExpected && std::none_of(s.begin(),s.end(),[](unsigned char c){return (c<32 && c!='\t') || c==127;});
}
char asciiLower(char c) { return c>='A' && c<='Z' ? static_cast<char>(c-'A'+'a') : c; }
bool equalHeaderName(std::string_view a,std::string_view b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](char x,char y){return asciiLower(x)==asciiLower(y);});
}
bool headerMatches(const ApiResponse& response,std::string_view name,std::string_view expected) {
    // libcurl can report interim response blocks. Only the last complete status
    // block is eligible; duplicate headers are ambiguous and never pass.
    std::string_view headers=response.headers, block; std::size_t pos=0; bool foundBlock=false;
    while(pos<headers.size()) {
        auto end=headers.find('\n',pos); if(end==headers.npos) return false;
        auto line=headers.substr(pos,end-pos); if(!line.empty() && line.back()=='\r') line.remove_suffix(1);
        if(!line.starts_with("HTTP/")) return false;
        auto space=line.find(' '); if(space==line.npos || space+4>line.size()) return false;
        long status=0; auto parsed=std::from_chars(line.data()+space+1,line.data()+space+4,status);
        if(parsed.ec!=std::errc{} || parsed.ptr!=line.data()+space+4 || (space+4<line.size() && line[space+4]!=' ')) return false;
        pos=end+1; const auto start=pos; bool complete=false;
        while(pos<headers.size()) {
            end=headers.find('\n',pos); if(end==headers.npos) return false;
            line=headers.substr(pos,end-pos); if(!line.empty() && line.back()=='\r') line.remove_suffix(1);
            if(line.empty()) { block=headers.substr(start,pos-start); pos=end+1; complete=true; break; }
            if(line.find(':')==line.npos || line.front()==' ' || line.front()=='\t') return false;
            pos=end+1;
        }
        if(!complete) return false;
        foundBlock=status==response.status;
    }
    if(!foundBlock) return false;
    bool found=false,matched=false; pos=0;
    while(pos<block.size()) {
        auto end=block.find('\n',pos); if(end==block.npos) return false;
        auto line=block.substr(pos,end-pos); if(!line.empty() && line.back()=='\r') line.remove_suffix(1);
        auto colon=line.find(':'); auto key=line.substr(0,colon); if(!headerNameValid(key)) return false;
        auto value=line.substr(colon+1); while(!value.empty() && (value.front()==' ' || value.front()=='\t')) value.remove_prefix(1);
        while(!value.empty() && (value.back()==' ' || value.back()=='\t')) value.remove_suffix(1);
        if(equalHeaderName(key,name)) { if(found) return false; found=true; matched=value==expected; }
        pos=end+1;
    }
    return found && matched;
}
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
        const editor::JsonValue *type=nullptr,*value=nullptr,*pointer=nullptr,*name=nullptr;
        for(const auto& field:entry.children) {
            if(field.key=="type" && !type) type=&field;
            else if(field.key=="value" && !value) value=&field;
            else if(field.key=="pointer" && !pointer) pointer=&field;
            else if(field.key=="name" && !name) name=&field;
            else return fail("Unknown or duplicate assertion field");
        }
        if(!type || type->type!=editor::JsonType::String || !value) return fail("Assertion type and value are required");
        if(name && type->stringValue!="header_equals") return fail("Unexpected assertion field");
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
        } else if(type->stringValue=="json_bool_equals" || type->stringValue=="json_null") {
            const bool isBool=type->stringValue=="json_bool_equals";
            if(!pointer || pointer->type!=editor::JsonType::String || !pointerValid(pointer->stringValue) ||
               value->type!=(isBool ? editor::JsonType::Bool : editor::JsonType::Null)) return fail("Invalid JSON assertion");
            a.kind=isBool ? ApiAssertionKind::JsonBoolEquals : ApiAssertionKind::JsonNull;
            a.pointer=pointer->stringValue;
            if(isBool) a.expected=value->boolValue ? "true" : "false";
        } else if(type->stringValue=="json_number_equals") {
            if(!pointer || pointer->type!=editor::JsonType::String || !pointerValid(pointer->stringValue) || value->type!=editor::JsonType::Number) return fail("Invalid JSON number assertion");
            const auto raw=numberAt(json,value->byteOffset); if(!decimal(raw)) return fail("Invalid JSON number assertion");
            a.kind=ApiAssertionKind::JsonNumberEquals; a.pointer=pointer->stringValue; a.expected=raw;
        } else if(type->stringValue=="header_equals") {
            if(pointer || !name || name->type!=editor::JsonType::String || !headerNameValid(name->stringValue) || value->type!=editor::JsonType::String || !headerValueValid(value->stringValue)) return fail("Invalid header assertion");
            a.kind=ApiAssertionKind::HeaderEquals; a.pointer=name->stringValue; a.expected=value->stringValue;
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
        case ApiAssertionKind::JsonBoolEquals: if((a.expected!="true" && a.expected!="false") || !pointerValid(a.pointer)) return fail("Invalid assertion set"); needsJson=true; break;
        case ApiAssertionKind::JsonNull: if(!a.expected.empty() || !pointerValid(a.pointer)) return fail("Invalid assertion set"); needsJson=true; break;
        case ApiAssertionKind::JsonNumberEquals: if(!pointerValid(a.pointer) || !decimal(a.expected)) return fail("Invalid assertion set"); needsJson=true; break;
        case ApiAssertionKind::HeaderEquals: if(!headerNameValid(a.pointer) || !headerValueValid(a.expected)) return fail("Invalid assertion set"); break;
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
        case ApiAssertionKind::HeaderEquals: result.passed=headerMatches(response,a.pointer,a.expected); result.detail=result.passed?"Header matched":"Header did not match or was unavailable"; break;
        case ApiAssertionKind::JsonStringEquals: {
            const auto* value=root ? follow(*root,a.pointer) : nullptr;
            result.passed=value && value->type==editor::JsonType::String && value->stringValue==a.expected;
            result.detail=result.passed?"JSON string matched":"JSON string did not match or was unavailable"; break;
        }
        case ApiAssertionKind::JsonBoolEquals: {
            const auto* value=root ? follow(*root,a.pointer) : nullptr;
            result.passed=value && value->type==editor::JsonType::Bool && value->boolValue==(a.expected=="true");
            result.detail=result.passed?"JSON boolean matched":"JSON boolean did not match or was unavailable"; break;
        }
        case ApiAssertionKind::JsonNull: {
            const auto* value=root ? follow(*root,a.pointer) : nullptr;
            result.passed=value && value->type==editor::JsonType::Null;
            result.detail=result.passed?"JSON null matched":"JSON null did not match or was unavailable"; break;
        }
        case ApiAssertionKind::JsonNumberEquals: {
            const auto* value=root ? follow(*root,a.pointer) : nullptr;
            auto expected=decimal(a.expected); auto actual=value && value->type==editor::JsonType::Number ? decimal(numberAt(response.body,value->byteOffset)) : std::nullopt;
            result.passed=expected && actual && expected->digits==actual->digits && expected->exponent==actual->exponent && expected->negative==actual->negative;
            result.detail=result.passed?"JSON number matched":"JSON number did not match or was unavailable"; break;
        }
        }
        report.results.push_back(std::move(result));
    }
    return report;
}
}
