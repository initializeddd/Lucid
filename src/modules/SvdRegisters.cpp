#include "modules/SvdRegisters.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <unordered_set>
namespace lucid::modules {
namespace {
std::string_view trim(std::string_view s) { while(!s.empty() && (s.front()==' '||s.front()=='\n'||s.front()=='\r'||s.front()=='\t')) s.remove_prefix(1); while(!s.empty() && (s.back()==' '||s.back()=='\n'||s.back()=='\r'||s.back()=='\t')) s.remove_suffix(1); return s; }
bool number(std::string_view s, std::uint64_t& v) { s=trim(s); int base=10; if(s.starts_with("0x")||s.starts_with("0X")){s.remove_prefix(2);base=16;} if(s.empty()) return false; auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v,base); return e==std::errc{} && p==s.data()+s.size(); }
// Expand only a single inclusive decimal or ASCII-letter range. Comma-separated
// lists retain their existing literal semantics; unsupported ranges stay literal.
bool expandDimIndex(std::string& value,std::uint64_t count) {
    if(value.find(',')!=std::string::npos)return true;
    const auto dash=value.find('-');if(dash==std::string::npos || dash==0 || dash+1==value.size() || value.find('-',dash+1)!=std::string::npos)return true;
    const auto first=std::string_view(value).substr(0,dash),last=std::string_view(value).substr(dash+1);
    const auto digits=[](std::string_view v){return std::all_of(v.begin(),v.end(),[](unsigned char c){return c>='0'&&c<='9';});};
    std::string result;
    if(digits(first)&&digits(last)) {
        std::uint64_t start=0,end=0;auto [p1,e1]=std::from_chars(first.data(),first.data()+first.size(),start);auto [p2,e2]=std::from_chars(last.data(),last.data()+last.size(),end);
        if(e1!=std::errc{}||e2!=std::errc{}||p1!=first.data()+first.size()||p2!=last.data()+last.size()||end<start||end-start!=count-1)return false;
        const bool padded=(first.size()>1&&first.front()=='0')||(last.size()>1&&last.front()=='0');if(padded && first.size()!=last.size())return false;
        for(std::uint64_t i=0;i<count;++i){auto part=std::to_string(start+i);if(padded && part.size()<first.size())part.insert(0,first.size()-part.size(),'0');if(i)result+=',';result+=part;}
    } else if(first.size()==1 && last.size()==1 && ((first[0]>='A'&&first[0]<='Z'&&last[0]>='A'&&last[0]<='Z')||(first[0]>='a'&&first[0]<='z'&&last[0]>='a'&&last[0]<='z'))) {
        if(last[0]<first[0] || static_cast<std::uint64_t>(last[0]-first[0])!=count-1)return false;
        for(std::uint64_t i=0;i<count;++i){if(i)result+=',';result+=static_cast<char>(first[0]+i);}
    } else return true;
    value=std::move(result);return true;
}
bool decodeXml(std::string_view input,std::string& output) {
    output.clear();
    for (std::size_t i=0;i<input.size();) {
        if(input[i]!='&') {output.push_back(input[i++]);continue;}
        auto end=input.find(';',i+1);if(end==std::string_view::npos || end-i>16) return false;auto entity=input.substr(i+1,end-i-1);std::uint32_t c=0;
        if(entity=="amp") c='&';else if(entity=="lt") c='<';else if(entity=="gt") c='>';else if(entity=="quot") c='"';else if(entity=="apos") c='\'';
        else if(entity.starts_with("#")) {entity.remove_prefix(1);int base=10;if(entity.starts_with("x")){entity.remove_prefix(1);base=16;}if(entity.empty())return false;auto [p,e]=std::from_chars(entity.data(),entity.data()+entity.size(),c,base);if(e!=std::errc{} || p!=entity.data()+entity.size())return false;}
        else return false;
        if((c<32 && c!=9 && c!=10 && c!=13) || c>0x10ffff || (c>=0xd800 && c<=0xdfff) || c==0xfffe || c==0xffff) return false;
        if(c<0x80) output.push_back(static_cast<char>(c));else if(c<0x800){output.push_back(static_cast<char>(0xc0|(c>>6)));output.push_back(static_cast<char>(0x80|(c&63)));}else if(c<0x10000){output.push_back(static_cast<char>(0xe0|(c>>12)));output.push_back(static_cast<char>(0x80|((c>>6)&63)));output.push_back(static_cast<char>(0x80|(c&63)));}else {output.push_back(static_cast<char>(0xf0|(c>>18)));output.push_back(static_cast<char>(0x80|((c>>12)&63)));output.push_back(static_cast<char>(0x80|((c>>6)&63)));output.push_back(static_cast<char>(0x80|(c&63)));}
        i=end+1;
    }
    return true;
}
struct Node { std::string name, text; bool skip=false; };
struct Field { std::string name,access,description; std::optional<std::uint64_t> off,width,lsb,msb,dim,increment; std::string dimIndex; std::vector<SvdEnumValue> values; bool inherited=false, explicitEnums=false, boundsLsb=false, boundsOffset=false; };
struct EnumItem { std::string name; std::optional<std::uint64_t> value; bool isDefault=false, invalid=false; };
struct Defaults { std::optional<std::uint64_t> size,reset; std::string access; };
struct PeripheralSource { std::string name; std::uint64_t base; std::size_t start,end; bool complete; };
struct Reg { std::string name,access,dimIndex,description; std::optional<std::uint64_t> offset,size,reset,dim,increment; std::vector<SvdField> fields; bool skip=false; };
}
SvdRegisters parseSvdRegisters(std::string_view xml, std::size_t maxBytes, std::size_t maxRegisters) {
    SvdRegisters out; auto fail=[&](const char* message) { out.error=message; out.registers.clear(); return out; };
    maxBytes=std::min<std::size_t>(maxBytes,16*1024*1024); maxRegisters=std::min<std::size_t>(maxRegisters,100000);
    if (xml.size()>maxBytes) return fail("SVD exceeds byte limit");
    bool inPeripheral=false, currentPeripheralIncomplete=false;
    auto warn = [&](const char* message) { if(inPeripheral) currentPeripheralIncomplete=true; out.incomplete=true; if (std::find(out.warnings.begin(),out.warnings.end(),message)==out.warnings.end() && out.warnings.size()<16) out.warnings.emplace_back(message); };
    Defaults deviceDefaults, peripheralDefaults, clusterDefaults; bool deviceRegistersSeen=false, peripheralRegistersSeen=false;
    std::vector<PeripheralSource> peripheralSources; std::string peripheralDerivedFrom; std::size_t peripheralStart=0; bool peripheralOverrides=false; std::optional<std::uint64_t> peripheralDim,peripheralIncrement; std::string peripheralIndexes;
    std::vector<Node> stack; stack.reserve(64); std::string peripheral, clusterName; std::optional<std::uint64_t> base, clusterOffset, clusterDim, clusterIncrement; std::string clusterIndexes; std::size_t clusterStart=0; bool inCluster=false, clusterRegistersSeen=false, clusterSkip=false; Reg reg; Field field; EnumItem enumItem; std::string enumUsage; bool peripheralSkip=false, inReg=false, inField=false, inEnum=false, rootSeen=false; std::size_t pos=0,nodes=0,totalFields=0,emittedFields=0,derivedComparisons=0,descriptionBytes=0,emittedEnums=0,constructedEnums=0,constructedDescriptions=0;
    while(pos<xml.size()) {
        auto lt=xml.find('<',pos); if(lt==std::string_view::npos) { if(!trim(xml.substr(pos)).empty()) return fail("Text outside XML root"); break; }
        auto text=xml.substr(pos,lt-pos); std::string decoded; if(!decodeXml(text,decoded)) return fail("Invalid or unsupported XML entity");
        if(!stack.empty()) { if(stack.back().text.size()+decoded.size()>8192) return fail("SVD text exceeds limit"); stack.back().text.append(decoded); } else if(!trim(text).empty()) return fail("Text outside XML root");
        if(xml.substr(lt,4)=="<!--") { auto end=xml.find("-->",lt+4); if(end==std::string_view::npos) return fail("Invalid XML comment"); pos=end+3; continue; }
        if(xml.substr(lt,2)=="<?") { auto end=xml.find("?>",lt+2); if(end==std::string_view::npos) return fail("Invalid XML declaration"); pos=end+2; continue; }
        if(xml.substr(lt,2)=="<!") return fail("XML declarations and entities are unsupported");
        auto gt=xml.find('>',lt+1); if(gt==std::string_view::npos) return fail("Truncated XML tag"); auto tag=trim(xml.substr(lt+1,gt-lt-1)); pos=gt+1; if(tag.empty() || tag.find('<')!=std::string_view::npos) return fail("Invalid XML tag");
        bool closing=tag.front()=='/'; if(closing) {tag.remove_prefix(1);tag=trim(tag);} bool self=!closing && tag.back()=='/'; if(self){tag.remove_suffix(1);tag=trim(tag);}
        auto split=tag.find_first_of(" \t\r\n"); auto name=tag.substr(0,split); auto attrs=split==std::string_view::npos ? std::string_view{} : trim(tag.substr(split));
        if(name.empty() || !std::all_of(name.begin(),name.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c==':';}) || (closing && !attrs.empty())) return fail("Invalid XML tag name");
        if(!closing) {
            if(++nodes>200000 || stack.size()>=64) return fail("SVD structure exceeds limit");
            if(stack.empty()) {if(rootSeen || name!="device") return fail("Invalid SVD root");rootSeen=true;}
            const auto parentName = stack.empty() ? std::string_view{} : std::string_view(stack.back().name);
            if ((name=="peripheral" && parentName!="peripherals") || (name=="register" && parentName!="registers" && parentName!="cluster") || (name=="field" && parentName!="fields")) return fail("Invalid SVD structure");
            const bool parentSkip=!stack.empty() && stack.back().skip; const bool registerArrayProperty = inReg && !inField && parentName=="register" && (name=="dim" || name=="dimIndex" || name=="dimIncrement");
            const bool fieldArrayProperty = inField && parentName=="field" && (name=="dim" || name=="dimIndex" || name=="dimIncrement");
            const bool peripheralArrayProperty = inPeripheral && parentName=="peripheral" && (name=="dim" || name=="dimIndex" || name=="dimIncrement");
            const bool derived = attrs.find("derivedFrom")!=std::string_view::npos;
            const bool registerDerived = name=="register" && derived;
            const bool peripheralDerived = name=="peripheral" && derived;
            const bool fieldDerived = name=="field" && derived;
            const bool supportedCluster = name=="cluster" && !inCluster && parentName=="registers" && !derived;
            const bool clusterArrayProperty = inCluster && !inReg && parentName=="cluster" && (name=="dim" || name=="dimIndex" || name=="dimIncrement");
            const bool unsupported=(derived && !registerDerived && !peripheralDerived && !fieldDerived) || (name=="cluster" && !supportedCluster) || (!registerArrayProperty && !clusterArrayProperty && !fieldArrayProperty && !peripheralArrayProperty && (name=="dim" || name=="dimIndex" || name=="dimIncrement"));
            if(unsupported) {warn(name=="cluster" ? "Clusters are not expanded" : (name=="dim" || name=="dimIndex" || name=="dimIncrement") ? "Arrays are not expanded" : "derivedFrom definitions are not expanded"); if(name=="dim"||name=="dimIndex"||name=="dimIncrement") {if(inReg) reg.skip=true; else if(inCluster) {if(clusterRegistersSeen) return fail("Cluster array properties must precede registers"); clusterSkip=true;} else peripheralSkip=true;} }
            stack.push_back({std::string(name),{},parentSkip || unsupported});
            if(!parentSkip && name=="peripheral") {
                inPeripheral=true;currentPeripheralIncomplete=false;peripheralStart=out.registers.size();peripheralDerivedFrom.clear();peripheralOverrides=false;
                peripheral.clear();base.reset();peripheralDefaults={};peripheralRegistersSeen=false;peripheralSkip=unsupported;peripheralDim.reset();peripheralIncrement.reset();peripheralIndexes.clear();
                if(peripheralDerived) {auto a=trim(attrs);constexpr std::string_view key="derivedFrom";if(!a.starts_with(key))return fail("Unsupported peripheral derivedFrom attribute");a.remove_prefix(key.size());a=trim(a);if(a.empty()||a.front()!='=')return fail("Invalid peripheral derivedFrom attribute");a.remove_prefix(1);a=trim(a);if(a.size()<3||(a.front()!='\''&&a.front()!='"'))return fail("Invalid peripheral derivedFrom attribute");auto quote=a.front();a.remove_prefix(1);auto end=a.find(quote);if(end==std::string_view::npos||!trim(a.substr(end+1)).empty())return fail("Invalid peripheral derivedFrom attribute");auto ref=a.substr(0,end);if(ref.empty()||ref.size()>256||!std::all_of(ref.begin(),ref.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';}))return fail("Unsupported peripheral derivedFrom reference");peripheralDerivedFrom=std::string(ref);}
            }
            if(!parentSkip && !peripheralDerivedFrom.empty() && parentName=="peripheral" && (name=="registers" || name=="size" || name=="access" || name=="resetValue" || name=="resetMask" || name=="protection")) {peripheralOverrides=true;stack.back().skip=true;warn("Derived peripheral overrides are not expanded");}
            if (!parentSkip && supportedCluster) {inCluster=true;clusterDefaults={};clusterName.clear();clusterOffset.reset();clusterDim.reset();clusterIncrement.reset();clusterIndexes.clear();clusterStart=out.registers.size();clusterRegistersSeen=false;clusterSkip=false;}
            if(!parentSkip && name=="register") {
                if (inCluster && (clusterName.empty() || !clusterOffset)) return fail("Cluster name and offset must precede registers");
                if (inCluster) clusterRegistersSeen=true;
                reg={};reg.skip=unsupported || (inCluster && clusterSkip);inReg=true;deviceRegistersSeen=true;peripheralRegistersSeen=true;
                if (registerDerived) {
                    // Accept a single, literal local reference; never interpret XML entities or paths.
                    auto a=trim(attrs); constexpr std::string_view key="derivedFrom";
                    if (!a.starts_with(key)) return fail("Unsupported register derivedFrom attribute");
                    a.remove_prefix(key.size()); a=trim(a);
                    if (a.empty() || a.front()!='=') return fail("Invalid register derivedFrom attribute");
                    a.remove_prefix(1); a=trim(a);
                    if (a.size()<3 || (a.front()!='\'' && a.front()!='"')) return fail("Invalid register derivedFrom attribute");
                    const auto quote=a.front(); a.remove_prefix(1); auto end=a.find(quote); if (end==std::string_view::npos || !trim(a.substr(end+1)).empty()) return fail("Invalid register derivedFrom attribute");
                    const auto ref=a.substr(0,end); if (ref.empty() || ref.size()>256 || !std::all_of(ref.begin(),ref.end(),[](unsigned char c){return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_';})) return fail("Unsupported register derivedFrom reference");
                    const SvdRegister* source=nullptr; bool ambiguous=false;
                    for (const auto& candidate:out.registers) {
                        if (++derivedComparisons > 1000000) {ambiguous=true;warn("Register derivedFrom lookup limit reached");break;}
                        if (candidate.peripheral==peripheral && candidate.name==(inCluster ? clusterName+"."+std::string(ref) : std::string(ref))) {if(source) {ambiguous=true;break;} source=&candidate;}
                    }
                    if (!source || ambiguous || !base || (inCluster && (!clusterOffset || *clusterOffset > std::numeric_limits<std::uint64_t>::max()-*base)) || source->address < *base + (inCluster ? *clusterOffset : 0)) {reg.skip=true;warn("Register derivedFrom reference unavailable or ambiguous");}
                    else if (source->fields.size()>100000-emittedFields) {reg.skip=true;warn("Expanded field limit reached");}
                    else if ([&]{std::size_t count=0;for(const auto& f:source->fields)count+=f.values.size();return count>100000-emittedEnums;}()) {reg.skip=true;warn("Expanded enumerated value limit reached");}
                    else {reg.offset=source->address-*base-(inCluster ? *clusterOffset : 0); if(source->sizeBits) reg.size=source->sizeBits; reg.reset=source->resetValue; reg.access=source->access; reg.fields=source->fields; reg.description=source->description;}
                }
            }
            if(!parentSkip && name=="field") {
                field={};inField=true;
                if(fieldDerived){auto a=trim(attrs);constexpr std::string_view key="derivedFrom";if(!a.starts_with(key))return fail("Unsupported field derivedFrom attribute");a.remove_prefix(key.size());a=trim(a);if(a.empty()||a.front()!='=')return fail("Invalid field derivedFrom attribute");a.remove_prefix(1);a=trim(a);if(a.size()<3||(a.front()!='\''&&a.front()!='"'))return fail("Invalid field derivedFrom attribute");auto quote=a.front();a.remove_prefix(1);auto end=a.find(quote);if(end==std::string_view::npos||!trim(a.substr(end+1)).empty())return fail("Invalid field derivedFrom attribute");auto ref=a.substr(0,end);if(ref.empty()||ref.size()>256||!std::all_of(ref.begin(),ref.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';}))return fail("Unsupported field derivedFrom reference");
                    const SvdField* source=nullptr;bool ambiguous=false;for(const auto& candidate:reg.fields){if(++derivedComparisons>1000000){warn("Field derivedFrom lookup limit reached");ambiguous=true;break;}if(candidate.name==ref){if(source){ambiguous=true;break;}source=&candidate;}}
                    if(!source||ambiguous){warn("Field derivedFrom reference unavailable or ambiguous");stack.back().skip=true;}
                    else {field.inherited=true;field.access=source->access;field.description=source->description;field.off=source->offset;field.width=source->width;field.values=source->values;}
                }
            }
            if(!parentSkip && name=="enumeratedValues" && !stack.back().skip){enumUsage.clear();if(field.inherited&&!field.explicitEnums){field.values.clear();field.explicitEnums=true;}}
            if(!parentSkip && name=="enumeratedValue" && parentName=="enumeratedValues") {enumItem={};inEnum=true;}
            if(!self) continue;
        }
        if(stack.empty() || stack.back().name!=name) return fail("Mismatched XML tag");
        Node node=std::move(stack.back()); stack.pop_back(); auto value=trim(node.text); auto parent=stack.empty()?std::string_view{}:std::string_view(stack.back().name); std::uint64_t n=0;
        if(!node.skip) {
            if ((parent=="device" || parent=="peripheral" || (inCluster && parent=="cluster")) && (name=="size" || name=="resetValue" || name=="access")) {
                if ((parent=="device" && deviceRegistersSeen) || (parent=="peripheral" && peripheralRegistersSeen) || (parent=="cluster" && clusterRegistersSeen)) return fail("SVD defaults must precede registers");
                auto& defaults = parent=="device" ? deviceDefaults : parent=="peripheral" ? peripheralDefaults : clusterDefaults;
                if (name=="access") { if (value.empty() || !defaults.access.empty()) return fail("Invalid or duplicate SVD default access"); defaults.access=std::string(value); }
                else { if (!number(value,n) || (name=="size" && (n==0 || n>64))) return fail("Invalid SVD default value"); auto& target=name=="size" ? defaults.size : defaults.reset; if (target) return fail("Duplicate SVD default value"); target=n; }
            }
            if (inCluster && parent=="cluster" && (name=="dim" || name=="dimIncrement" || name=="dimIndex")) {
                if (clusterRegistersSeen) return fail("Cluster array properties must precede registers");
                if (name=="dimIndex") {if (!clusterIndexes.empty() || value.empty() || value.size()>4096) return fail("Invalid cluster dimIndex"); clusterIndexes=std::string(value);}
                else {if (!number(value,n)) return fail("Invalid cluster array value"); auto& target=name=="dim" ? clusterDim : clusterIncrement; if (target) return fail("Duplicate cluster array value"); target=n;}
            }
            if (inCluster && parent=="cluster" && (name=="resetMask" || name=="protection")) warn("Some cluster-level defaults are not inherited");
            if (inCluster && parent=="cluster" && (name=="name" || name=="addressOffset")) { if (clusterRegistersSeen) return fail("Cluster name and offset must precede registers"); if (name=="name") clusterName=std::string(value); else {if(!number(value,n)) return fail("Invalid cluster offset"); clusterOffset=n;} }
            if(parent=="peripheral" && (name=="dim" || name=="dimIncrement" || name=="dimIndex")) {
                if(peripheralRegistersSeen) return fail("Peripheral array properties must precede registers");
                if(name=="dimIndex") {if(!peripheralIndexes.empty() || value.empty() || value.size()>4096)return fail("Invalid peripheral dimIndex");peripheralIndexes=std::string(value);}
                else {if(!number(value,n))return fail("Invalid peripheral array value");auto& target=name=="dim"?peripheralDim:peripheralIncrement;if(target)return fail("Duplicate peripheral array value");target=n;}
            }
            if(parent=="peripheral") {if(name=="name") peripheral=std::string(value); else if(name=="baseAddress") {if(!number(value,n)) return fail("Invalid peripheral base address");base=n;}}
            if (inReg && !inField && parent=="register" && (name=="dim" || name=="dimIncrement" || name=="dimIndex")) {
                if (name=="dimIndex") { if (!reg.dimIndex.empty() || value.empty() || value.size()>4096) return fail("Invalid register dimIndex"); reg.dimIndex=std::string(value); }
                else { if (!number(value,n)) return fail("Invalid register array value"); auto& target=name=="dim" ? reg.dim : reg.increment; if (target) return fail("Duplicate register array value"); target=n; }
            }
            if(inReg && !inField && parent=="register") { if(name=="name") reg.name=std::string(value); else if(name=="access") reg.access=std::string(value); else if(name=="description") {if(value.size()<=512) reg.description=std::string(value);else {reg.description.clear();warn("Some descriptions exceed the display limit");}} else if(name=="addressOffset"||name=="size"||name=="resetValue") {if(!number(value,n)) return fail("Invalid register value"); if(name=="addressOffset") reg.offset=n; else if(name=="size") reg.size=n; else reg.reset=n;} }
            if(inField && parent=="enumeratedValues" && name=="usage") {if(value=="read" || value=="write" || value=="read-write") enumUsage=std::string(value); else {warn("Unsupported enumerated value usage");enumUsage="unsupported";}}
            if(inEnum && parent=="enumeratedValue") {
                if(name=="name") enumItem.name=std::string(value);
                else if(name=="value") {if(number(value,n)) enumItem.value=n;else {enumItem.invalid=true;warn("Unsupported enumerated value encoding");}}
                else if(name=="isDefault") {if(value=="true" || value=="1") enumItem.isDefault=true;else if(value!="false" && value!="0") {enumItem.invalid=true;warn("Invalid enumerated default");}}
            }
            if(name=="enumeratedValue" && inEnum) {
                if(!enumItem.invalid && !enumItem.name.empty() && enumItem.name.size()<=256 && (enumItem.value || enumItem.isDefault) && enumUsage!="unsupported") {if(field.values.size()<256) field.values.push_back({enumItem.name,enumUsage,enumItem.value,enumItem.isDefault});else warn("Field enumerated value limit reached");}
                else warn("Some enumerated values were omitted");
                inEnum=false;
            }
            if(inField && parent=="field" && (name=="dim" || name=="dimIncrement" || name=="dimIndex")) {
                if(name=="dimIndex") {if(!field.dimIndex.empty() || value.empty() || value.size()>4096) return fail("Invalid field dimIndex"); field.dimIndex=std::string(value);}
                else {if(!number(value,n)) return fail("Invalid field array value"); auto& target=name=="dim" ? field.dim : field.increment; if(target) return fail("Duplicate field array value"); target=n;}
            }
            if(inField && parent=="field") {if(name=="name") field.name=std::string(value); else if(name=="access") field.access=std::string(value); else if(name=="description") {if(value.size()<=512) field.description=std::string(value);else warn("Some descriptions exceed the display limit");} else if(name=="bitOffset"||name=="bitWidth"||name=="lsb"||name=="msb") {if(!number(value,n)) return fail("Invalid field value");if(name=="bitOffset"||name=="bitWidth"){if(field.boundsLsb)return fail("Mixed field bit range forms");field.boundsOffset=true;if(name=="bitOffset")field.off=n;else field.width=n;}else {if(field.boundsOffset)return fail("Mixed field bit range forms");if(!field.boundsLsb && field.inherited){field.off.reset();field.width.reset();}field.boundsLsb=true;if(name=="lsb")field.lsb=n;else field.msb=n;}}}
            if(name=="field" && inField) {
                if(!field.off && field.lsb) field.off=field.lsb;
                if(!field.width && field.lsb && field.msb && *field.msb<64 && *field.msb>=*field.lsb) field.width=*field.msb-*field.lsb+1;
                if(field.width && *field.width<64) {const auto limit=std::uint64_t{1}<<*field.width;auto end=std::remove_if(field.values.begin(),field.values.end(),[&](const auto& item){return item.value && *item.value>=limit;});if(end!=field.values.end()){warn("Enumerated value exceeds field width");field.values.erase(end,field.values.end());}}
                std::vector<std::string> indexes;
                const bool array=field.dim || field.increment || !field.dimIndex.empty();
                if(array) {
                    if(!field.dim || !field.increment || !*field.dim || !*field.increment || *field.dim>64 || field.name.find("%s")==std::string::npos || field.name.find("%s")!=field.name.rfind("%s")) return fail("Invalid field array definition");
                    if(!expandDimIndex(field.dimIndex,*field.dim)) return fail("Invalid field dimIndex range");
                    if(field.dimIndex.empty()) {for(std::uint64_t i=0;i<*field.dim;++i) indexes.push_back(std::to_string(i));}
                    else {std::string_view list=field.dimIndex; while(true) {auto comma=list.find(',');auto item=trim(list.substr(0,comma));if(item.empty() || item.size()>64 || !std::all_of(item.begin(),item.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';}) || std::find(indexes.begin(),indexes.end(),item)!=indexes.end()) return fail("Unsupported field dimIndex");indexes.emplace_back(item);if(comma==std::string_view::npos) break;list.remove_prefix(comma+1);if(indexes.size()>*field.dim) return fail("Field dimIndex count mismatch");}if(indexes.size()!=*field.dim) return fail("Field dimIndex count mismatch");}
                } else indexes.emplace_back();
                if(field.name.empty() || !field.off || !field.width || !*field.width || *field.off>=64 || *field.width>64-*field.off || (array && (*field.dim-1)>(63-*field.off)/ *field.increment) || (array && *field.width>64-(*field.off+(*field.dim-1)* *field.increment))) {warn("Some fields have unsupported or invalid bit ranges");}
                else for(const auto& index:indexes) {
                    if(++totalFields>100000) return fail("SVD field limit exceeded");
                    if(field.values.size()>100000-constructedEnums || field.description.size()>16U*1024U*1024U-constructedDescriptions) {warn("Expanded field metadata limit reached");break;}
                    constructedEnums+=field.values.size();constructedDescriptions+=field.description.size();auto fieldName=field.name;if(array) fieldName.replace(fieldName.find("%s"),2,index);
                    const auto offset=*field.off+(array ? static_cast<std::uint64_t>(&index-&indexes[0])* *field.increment : 0);
                    auto previous=std::find_if(reg.fields.begin(),reg.fields.end(),[&](const auto& item){return item.name==fieldName;});SvdField parsed{std::move(fieldName),field.access,static_cast<unsigned>(offset),static_cast<unsigned>(*field.width),field.description,field.values};if(previous==reg.fields.end()) reg.fields.push_back(std::move(parsed));else *previous=std::move(parsed);
                }
                inField=false;
            }
            if(name=="register" && inReg) {
                if (!reg.size) reg.size=inCluster && clusterDefaults.size ? clusterDefaults.size : peripheralDefaults.size ? peripheralDefaults.size : deviceDefaults.size;
                if (!reg.reset) reg.reset=inCluster && clusterDefaults.reset ? clusterDefaults.reset : peripheralDefaults.reset ? peripheralDefaults.reset : deviceDefaults.reset;
                if (reg.access.empty()) reg.access=inCluster && !clusterDefaults.access.empty() ? clusterDefaults.access : !peripheralDefaults.access.empty() ? peripheralDefaults.access : deviceDefaults.access;
                for (auto& item:reg.fields) if (item.access.empty()) item.access=reg.access;
                const auto clusterBase = inCluster ? clusterOffset.value_or(0) : 0;
                if(!reg.skip && !peripheralSkip && !peripheral.empty() && base && !reg.name.empty() && reg.offset && clusterBase<=std::numeric_limits<std::uint64_t>::max()-*base && *reg.offset<=std::numeric_limits<std::uint64_t>::max()-*base-clusterBase && (!reg.size || (*reg.size>0 && *reg.size<=64))) {
                    const auto first=*base+clusterBase+*reg.offset;
                    std::size_t descriptionCost=reg.description.size(), enumCost=0; for(const auto& f:reg.fields) {descriptionCost+=f.description.size();enumCost+=f.values.size();}
                    if (reg.dim || reg.increment || !reg.dimIndex.empty()) {
                        if (!reg.dim || !reg.increment || !*reg.dim || !*reg.increment || *reg.dim>10000 || reg.name.find("%s")==std::string::npos || reg.name.find("%s")!=reg.name.rfind("%s")) return fail("Invalid register array definition");
                        if (*reg.dim-1 > (std::numeric_limits<std::uint64_t>::max()-first)/ *reg.increment) return fail("Register array address overflow");
                        std::vector<std::string> indexes; std::unordered_set<std::string> seenIndexes;
                        if(!expandDimIndex(reg.dimIndex,*reg.dim)) return fail("Invalid register dimIndex range");
                    if (reg.dimIndex.empty()) { for (std::uint64_t i=0;i<*reg.dim;++i) indexes.push_back(std::to_string(i)); }
                        else {
                            std::string_view list=reg.dimIndex; while (true) { auto comma=list.find(','); auto part=trim(list.substr(0,comma)); if (part.empty() || part.size()>64 || !std::all_of(part.begin(),part.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';}) || !seenIndexes.emplace(part).second) return fail("Unsupported register dimIndex"); indexes.emplace_back(part); if (comma==std::string_view::npos) break; list.remove_prefix(comma+1); if (indexes.size()>*reg.dim) return fail("Register dimIndex count mismatch"); }
                            if (indexes.size()!=*reg.dim) return fail("Register dimIndex count mismatch");
                        }
                        for (std::size_t i=0;i<indexes.size();++i) { if (out.registers.size()>=maxRegisters) {warn("Register limit reached"); break;} if (reg.fields.size()>100000-emittedFields) {warn("Expanded field limit reached"); break;} if(descriptionCost>16U*1024U*1024U-descriptionBytes){warn("Expanded description limit reached");break;} if(enumCost>100000-emittedEnums){warn("Expanded enumerated value limit reached");break;}emittedEnums+=enumCost;descriptionBytes+=descriptionCost; emittedFields+=reg.fields.size(); auto registerName=reg.name; registerName.replace(registerName.find("%s"),2,indexes[i]); if(inCluster) registerName=clusterName+"."+registerName; out.registers.push_back({peripheral,std::move(registerName),reg.access,first+i* *reg.increment,static_cast<unsigned>(reg.size.value_or(0)),reg.reset,reg.fields,reg.description}); }
                    } else if(out.registers.size()>=maxRegisters) warn("Register limit reached");
                    else if(reg.fields.size()>100000-emittedFields) warn("Expanded field limit reached");
                    else if(descriptionCost>16U*1024U*1024U-descriptionBytes) warn("Expanded description limit reached");
                    else if(enumCost>100000-emittedEnums) warn("Expanded enumerated value limit reached");
                    else { emittedEnums+=enumCost;descriptionBytes+=descriptionCost; emittedFields+=reg.fields.size(); out.registers.push_back({peripheral,inCluster ? clusterName+"."+reg.name : reg.name,reg.access,first,static_cast<unsigned>(reg.size.value_or(0)),reg.reset,std::move(reg.fields),reg.description}); }
                } else warn("Some registers have missing or invalid metadata"); inReg=false;
            }
        } else { if(name=="field") inField=false; if(name=="register") inReg=false; if(name=="enumeratedValue") inEnum=false; }
        if(name=="peripheral" && !node.skip) {
            const bool peripheralArray=peripheralDim || peripheralIncrement || !peripheralIndexes.empty();
            if(peripheralArray && !peripheralDerivedFrom.empty()) {warn("Derived peripheral arrays are not expanded");peripheralSkip=true;}
            if(!peripheralDerivedFrom.empty() && !peripheralOverrides && !peripheralSkip) {
                const PeripheralSource* source=nullptr;bool ambiguous=false;
                for(const auto& item:peripheralSources){if(++derivedComparisons>1000000){warn("Peripheral derivedFrom lookup limit reached");ambiguous=true;break;}if(item.name==peripheralDerivedFrom){if(source){ambiguous=true;break;}source=&item;}}
                if(!source||ambiguous||!source->complete||peripheral.empty()||!base){warn("Peripheral derivedFrom reference unavailable, incomplete or ambiguous");}
                else {
                    const auto start=source->start,end=source->end;const auto sourceBase=source->base;
                    bool valid=true;for(std::size_t i=start;i<end;++i){const auto& item=out.registers[i];if(item.address<sourceBase||item.address-sourceBase>std::numeric_limits<std::uint64_t>::max()-*base){valid=false;break;}}
                    if(!valid)warn("Derived peripheral address overflow");
                    else for(std::size_t i=start;i<end;++i){const auto& item=out.registers[i];if(out.registers.size()>=maxRegisters){warn("Register limit reached");break;}if(item.fields.size()>100000-emittedFields){warn("Expanded field limit reached");break;}std::size_t descriptions=item.description.size(),enums=0;for(const auto& f:item.fields){descriptions+=f.description.size();enums+=f.values.size();}if(descriptions>16U*1024U*1024U-descriptionBytes){warn("Expanded description limit reached");break;}if(enums>100000-emittedEnums){warn("Expanded enumerated value limit reached");break;}auto copy=item;copy.peripheral=peripheral;copy.address=*base+(item.address-sourceBase);emittedFields+=item.fields.size();descriptionBytes+=descriptions;emittedEnums+=enums;out.registers.push_back(std::move(copy));}
                }
            }
            if(peripheralArray && peripheralDerivedFrom.empty() && !peripheralSkip) {
                if(!peripheralDim || !peripheralIncrement || !*peripheralDim || !*peripheralIncrement || *peripheralDim>256 || !base || peripheral.find("%s")==std::string::npos || peripheral.find("%s")!=peripheral.rfind("%s"))return fail("Invalid peripheral array definition");
                std::vector<std::string> indexes;
                if(!expandDimIndex(peripheralIndexes,*peripheralDim)) return fail("Invalid peripheral dimIndex range");
                    if(peripheralIndexes.empty()){for(std::uint64_t i=0;i<*peripheralDim;++i)indexes.push_back(std::to_string(i));}
                else {std::string_view list=peripheralIndexes;while(true){auto comma=list.find(',');auto item=trim(list.substr(0,comma));if(item.empty()||item.size()>64||!std::all_of(item.begin(),item.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';})||std::find(indexes.begin(),indexes.end(),item)!=indexes.end())return fail("Unsupported peripheral dimIndex");indexes.emplace_back(item);if(comma==std::string_view::npos)break;list.remove_prefix(comma+1);if(indexes.size()>*peripheralDim)return fail("Peripheral dimIndex count mismatch");}if(indexes.size()!=*peripheralDim)return fail("Peripheral dimIndex count mismatch");}
                const auto templateEnd=out.registers.size();const auto marker=peripheral.find("%s");
                if((*peripheralDim-1)>(std::numeric_limits<std::uint64_t>::max()-*base)/ *peripheralIncrement)return fail("Peripheral array address overflow");
                for(std::size_t i=peripheralStart;i<templateEnd;++i)if((*peripheralDim-1)>(std::numeric_limits<std::uint64_t>::max()-out.registers[i].address)/ *peripheralIncrement)return fail("Peripheral array address overflow");
                for(std::size_t index=0;index<indexes.size();++index){auto instance=peripheral;instance.replace(marker,2,indexes[index]);const auto instanceStart=index?out.registers.size():peripheralStart;
                    if(index==0){for(std::size_t i=peripheralStart;i<templateEnd;++i)out.registers[i].peripheral=instance;}
                    else for(std::size_t i=peripheralStart;i<templateEnd;++i){const auto& item=out.registers[i];if(out.registers.size()>=maxRegisters){warn("Register limit reached");break;}if(item.fields.size()>100000-emittedFields){warn("Expanded field limit reached");break;}std::size_t descriptions=item.description.size(),enums=0;for(const auto& f:item.fields){descriptions+=f.description.size();enums+=f.values.size();}if(descriptions>16U*1024U*1024U-descriptionBytes){warn("Expanded description limit reached");break;}if(enums>100000-emittedEnums){warn("Expanded enumerated value limit reached");break;}auto copy=item;copy.peripheral=instance;copy.address+=index* *peripheralIncrement;emittedFields+=item.fields.size();descriptionBytes+=descriptions;emittedEnums+=enums;out.registers.push_back(std::move(copy));}
                    peripheralSources.push_back({instance,*base+index* *peripheralIncrement,instanceStart,out.registers.size(),!currentPeripheralIncomplete});
                    if(currentPeripheralIncomplete)break;
                }
            } else if(!peripheral.empty()&&base)peripheralSources.push_back({peripheral,*base,peripheralStart,out.registers.size(),!currentPeripheralIncomplete&&!peripheralSkip});
            inPeripheral=false;
        }
        if (name=="cluster" && !node.skip) {
            if (clusterDim || clusterIncrement || !clusterIndexes.empty()) {
                if (!clusterDim || !clusterIncrement || !*clusterDim || !*clusterIncrement || *clusterDim>10000 || clusterName.find("%s")==std::string::npos || clusterName.find("%s")!=clusterName.rfind("%s")) return fail("Invalid cluster array definition");
                std::vector<std::string> indexes; std::unordered_set<std::string> seenIndexes;
                if(!expandDimIndex(clusterIndexes,*clusterDim)) return fail("Invalid cluster dimIndex range");
                    if (clusterIndexes.empty()) {for (std::uint64_t i=0;i<*clusterDim;++i) indexes.push_back(std::to_string(i));}
                else {std::string_view list=clusterIndexes; while(true) {auto comma=list.find(','); auto item=trim(list.substr(0,comma)); if(item.empty() || item.size()>64 || !std::all_of(item.begin(),item.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';}) || !seenIndexes.emplace(item).second) return fail("Unsupported cluster dimIndex"); indexes.emplace_back(item); if(comma==std::string_view::npos) break; list.remove_prefix(comma+1); if(indexes.size()>*clusterDim) return fail("Cluster dimIndex count mismatch");} if(indexes.size()!=*clusterDim) return fail("Cluster dimIndex count mismatch");}
                std::vector<SvdRegister> templates(out.registers.begin()+static_cast<std::ptrdiff_t>(clusterStart),out.registers.end());
                const auto marker=clusterName.find("%s");
                for (const auto& item:templates) if (*clusterDim-1 > (std::numeric_limits<std::uint64_t>::max()-item.address)/ *clusterIncrement) return fail("Cluster array address overflow");
                for (std::size_t i=0;i<templates.size();++i) out.registers[clusterStart+i].name.replace(marker,2,indexes[0]);
                for (std::size_t index=1;index<indexes.size();++index) {
                    for (const auto& item:templates) {
                        if(out.registers.size()>=maxRegisters) {warn("Register limit reached");break;}
                        if(item.fields.size()>100000-emittedFields) {warn("Expanded field limit reached");break;}
                        std::size_t descriptionCost=item.description.size(),enumCost=0;for(const auto& f:item.fields){descriptionCost+=f.description.size();enumCost+=f.values.size();}if(descriptionCost>16U*1024U*1024U-descriptionBytes){warn("Expanded description limit reached");break;}if(enumCost>100000-emittedEnums){warn("Expanded enumerated value limit reached");break;}emittedEnums+=enumCost;descriptionBytes+=descriptionCost;
                        auto copy=item; copy.name.replace(marker,2,indexes[index]); copy.address+=index* *clusterIncrement; emittedFields+=copy.fields.size(); out.registers.push_back(std::move(copy));
                    }
                    if(out.registers.size()>=maxRegisters || emittedFields>=100000) {if(index+1<indexes.size()) warn("Cluster array expansion limited");break;}
                }
            }
            inCluster=false;clusterName.clear();clusterOffset.reset();
        }
    }
    if(!rootSeen || !stack.empty()) return fail("Incomplete SVD XML");
    return out;
}
}
