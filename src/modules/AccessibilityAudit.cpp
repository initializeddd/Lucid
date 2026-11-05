#include "modules/AccessibilityAudit.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
namespace lucid::modules {
namespace {
std::string lower(std::string_view s) { std::string out; for (unsigned char c:s) out.push_back(static_cast<char>(std::tolower(c))); return out; }
bool space(char c) { return c==' ' || c=='\t' || c=='\n' || c=='\r' || c=='\f'; }
struct Tag { std::string name; std::unordered_map<std::string,std::string> attrs; std::size_t line=0,column=0; bool closing=false, self=false, uncertain=false; };
std::string_view trim(std::string_view value) {
    while (!value.empty() && space(value.front())) value.remove_prefix(1);
    while (!value.empty() && space(value.back())) value.remove_suffix(1);
    return value;
}
// A conservative source-only check. Ancestors, CSS and runtime state cannot be
// resolved here, so callers describe the result as a possible focus problem.
bool possiblyTabbableAriaHidden(const Tag& t) {
    const auto aria = t.attrs.find("aria-hidden");
    if (aria == t.attrs.end() || lower(trim(aria->second)) != "true" ||
        t.attrs.count("hidden") || t.attrs.count("inert") || t.attrs.count("style")) return false;
    const bool formControl = t.name == "button" || t.name == "input" || t.name == "select" || t.name == "textarea";
    if (formControl && t.attrs.count("disabled")) return false;
    if (t.name == "input") {
        const auto type = t.attrs.find("type");
        if (type != t.attrs.end() && lower(trim(type->second)) == "hidden") return false;
    }
    const auto tabindex = t.attrs.find("tabindex");
    if (tabindex != t.attrs.end()) {
        auto value = trim(tabindex->second);
        if (!value.empty() && value.front() == '+') value.remove_prefix(1);
        if (!value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= '0' && c <= '9'; })) return true;
        if (!value.empty() && value.front() == '-') return false;
        // Invalid values are not reliable enough for a focus claim.
        return false;
    }
    return formControl || ((t.name == "a" || t.name == "area") && t.attrs.count("href"));
}
// The viewport declaration is a list of literal key/value pairs. We deliberately
// ignore expressions and duplicated keys, whose browser behavior can differ.
bool viewportMayRestrictZoom(const Tag& t) {
    if (t.name != "meta") return false;
    const auto name = t.attrs.find("name"), content = t.attrs.find("content");
    if (name == t.attrs.end() || lower(trim(name->second)) != "viewport" ||
        content == t.attrs.end() || content->second.find('&') != std::string::npos) return false;
    unsigned scalableCount = 0, maximumCount = 0;
    bool disablesScaling = false, tooSmallMaximum = false;
    std::string_view rest = content->second;
    while (!rest.empty()) {
        const auto separator = rest.find_first_of(",;");
        const auto entry = trim(rest.substr(0, separator));
        const auto equals = entry.find('=');
        if (equals != std::string_view::npos) {
            const auto key = lower(trim(entry.substr(0, equals)));
            const auto value = trim(entry.substr(equals + 1));
            if (key == "user-scalable") {
                ++scalableCount;
                disablesScaling = lower(value) == "no";
            } else if (key == "maximum-scale") {
                ++maximumCount;
                double maximum = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), maximum);
                tooSmallMaximum = !value.empty() && parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() &&
                                  std::isfinite(maximum) && maximum > 0 && maximum < 2;
            }
        }
        if (separator == std::string_view::npos) break;
        rest.remove_prefix(separator + 1);
    }
    return (scalableCount == 1 && disablesScaling) || (maximumCount == 1 && tooSmallMaximum);
}
}
AccessibilityAudit auditHtml(std::string_view html, std::size_t maxBytes, std::size_t maxTags, std::size_t maxFindings) {
    AccessibilityAudit out; out.limitation="Static subset only; CSS, JavaScript, rendered visibility, accessible-name computation and dynamic markup are not evaluated.";
    if (html.size()>maxBytes) { out.incomplete=true; html=html.substr(0,maxBytes); }
    std::vector<Tag> tags; std::size_t pos=0,line=1,col=1;
    auto advance=[&](std::size_t end) { while(pos<end) { if(html[pos++]=='\n') { ++line; col=1; } else ++col; } };
    while (pos<html.size()) {
        if (html[pos]!='<') { advance(pos+1); continue; }
        if (html.substr(pos,4)=="<!--") { auto end=html.find("-->",pos+4); if (end==std::string_view::npos) { out.incomplete=true; break; } advance(end+3); continue; }
        auto start=pos; auto tagLine=line, tagCol=col; std::size_t p=pos+1; bool closing=false;
        if (p<html.size() && html[p]=='/') { closing=true; ++p; }
        if (p>=html.size() || !std::isalpha(static_cast<unsigned char>(html[p]))) { advance(pos+1); continue; }
        auto nameStart=p; while(p<html.size() && (std::isalnum(static_cast<unsigned char>(html[p])) || html[p]=='-' || html[p]==':')) ++p;
        Tag t; t.name=lower(html.substr(nameStart,p-nameStart)); t.line=tagLine; t.column=tagCol; t.closing=closing;
        bool done=false;
        while (p<html.size()) {
            while(p<html.size() && space(html[p])) ++p;
            if (p>=html.size()) break;
            if (html[p]=='>') { ++p; done=true; break; }
            if (html[p]=='/' && p+1<html.size() && html[p+1]=='>') { p+=2; t.self=true; done=true; break; }
            auto a=p; while(p<html.size() && !space(html[p]) && html[p]!='=' && html[p]!='>' && html[p]!='/' && html[p]!='<' && html[p]!='\"' && html[p]!='\'') ++p;
            if (a==p) { t.uncertain=true; break; }
            auto key=lower(html.substr(a,p-a)); while(p<html.size() && space(html[p])) ++p; std::string value;
            if (p<html.size() && html[p]=='=') { ++p; while(p<html.size() && space(html[p])) ++p; if (p>=html.size()) break;
                if (html[p]=='\"' || html[p]=='\'') { char q=html[p++]; auto begin=p; while(p<html.size() && html[p]!=q) ++p; if(p==html.size()) break; value=std::string(html.substr(begin,p-begin)); ++p; }
                else { auto begin=p; while(p<html.size() && !space(html[p]) && html[p]!='>') ++p; value=std::string(html.substr(begin,p-begin)); }
            }
            if (!t.attrs.emplace(std::move(key),std::move(value)).second) t.uncertain=true;
        }
        if (!done) { out.incomplete=true; break; }
        if (tags.size()>=maxTags) { out.incomplete=true; break; }
        if (html.substr(start,p-start).find("{{")!=std::string_view::npos || html.substr(start,p-start).find("<%")!=std::string_view::npos) t.uncertain=true;
        if (t.uncertain) out.incomplete = true;
        tags.push_back(std::move(t)); advance(p);
        if (!closing && (tags.back().name=="script" || tags.back().name=="style")) {
            // Raw-text scanning is case-insensitive and avoids interpreting embedded markup.
            auto target="</"+tags.back().name; auto end=pos;
            for (; end<html.size(); ++end) { if (html[end]=='<' && end+target.size()<=html.size() && lower(html.substr(end,target.size()))==target) break; }
            if(end==html.size()) {out.incomplete=true; break;} advance(end);
        }
    }
    auto add=[&](const Tag& t, const char* rule, const char* message) { if (out.findings.size()<maxFindings) out.findings.push_back({rule,message,t.line,t.column}); else out.incomplete=true; };
    std::unordered_set<std::string> labelFor, knownIds;
    bool unresolvedIds = false;
    for (const auto& t : tags) {
        if (t.closing) continue;
        if (t.uncertain) { unresolvedIds = true; continue; }
        if (const auto id = t.attrs.find("id"); id != t.attrs.end() && !id->second.empty()) {
            // References cannot be compared reliably without decoding entities.
            if (id->second.find('&') != std::string::npos) unresolvedIds = true;
            else knownIds.insert(id->second);
        }
        if (t.name == "label") { auto i=t.attrs.find("for"); if(i!=t.attrs.end() && !i->second.empty()) labelFor.insert(i->second); }
    }
    unsigned labelDepth=0; std::unordered_set<std::string> seenIds;
    for (const auto& t:tags) {
        if (!t.closing && !t.uncertain) {
            const auto id = t.attrs.find("id");
            // Character references are not decoded by this scanner.
            if (id != t.attrs.end() && !id->second.empty() && id->second.find('&') == std::string::npos &&
                !seenIds.insert(id->second).second)
                add(t, "duplicate-id", "Duplicate literal id may make labels and ARIA references ambiguous.");
        }
        if (t.closing) { if (t.name=="label" && labelDepth) --labelDepth; continue; }
        if (t.uncertain) continue;
        if (possiblyTabbableAriaHidden(t)) add(t, "aria-hidden-focus", "Element is aria-hidden but may remain keyboard focusable; check its rendered state.");
        if (viewportMayRestrictZoom(t)) add(t, "viewport-zoom", "Viewport settings may restrict users from zooming to 200%; verify browser behavior.");
        // ARIA ID references may point forward, so compare against the full parsed
        // source. Incomplete input and undecoded IDs could contain the target.
        if (!out.incomplete && !unresolvedIds) {
            constexpr const char* referenceAttributes[] = {"aria-labelledby", "aria-describedby", "aria-controls", "aria-owns", "aria-flowto", "aria-activedescendant", "aria-details", "aria-errormessage"};
            for (const auto* name : referenceAttributes) {
                const auto attribute = t.attrs.find(name);
                if (attribute == t.attrs.end() || attribute->second.find('&') != std::string::npos) continue;
                const auto& value = attribute->second;
                for (std::size_t p = 0; p < value.size();) {
                    while (p < value.size() && space(value[p])) ++p;
                    const auto start = p;
                    while (p < value.size() && !space(value[p])) ++p;
                    if (p != start && knownIds.count(value.substr(start, p - start)) == 0) {
                        add(t, "aria-missing-reference", "An ARIA reference points to an ID not found in the parsed HTML.");
                        break;
                    }
                }
            }
        }
        if (t.name=="label") { if (!t.self) ++labelDepth; continue; }
        if (t.name=="html" && !t.attrs.count("lang")) add(t,"html-lang","HTML element has no lang attribute.");
        if (t.name=="img" && !t.attrs.count("alt") && !t.attrs.count("aria-label") && !t.attrs.count("aria-labelledby")) add(t,"image-alt","Image has no alt attribute or ARIA name.");
        if (t.name=="input" || t.name=="select" || t.name=="textarea") {
            auto type=t.attrs.find("type"); if (type!=t.attrs.end()) { auto v=lower(type->second); if (v=="hidden" || v=="submit" || v=="reset" || v=="button" || v=="image") continue; }
            auto id=t.attrs.find("id"); if (!labelDepth && !t.attrs.count("aria-label") && !t.attrs.count("aria-labelledby") && !t.attrs.count("title") && (id==t.attrs.end() || !labelFor.count(id->second))) add(t,"form-label","Form control has no associated label or ARIA name in the parsed markup.");
        }
    }
    return out;
}
}
