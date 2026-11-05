#include "modules/PullRequestData.hpp"
#include "editor/json/Json.hpp"
#include <cmath>
#include <algorithm>
#include <charconv>
#include <limits>
namespace lucid::modules {
namespace {
using editor::JsonValue; using editor::JsonType;
constexpr std::size_t maxBytes = 8U * 1024U * 1024U, maxEntries = 10000, maxWarnings = 1000;
void warn(PullRequestData& out, std::string message) {
    out.partial = true;
    if (out.warnings.size() < maxWarnings) out.warnings.push_back(std::move(message));
}
const JsonValue* object(const JsonValue& obj, const char* key, PullRequestData& out) {
    const auto* f = obj.find(key); if (!f || f->type != JsonType::Object) { warn(out, std::string("Missing or unsupported object: ") + key); return nullptr; } return f;
}
std::string field(const JsonValue& obj, const char* key, PullRequestData& out, bool nullable = false) {
    const auto* f = obj.find(key);
    if (f && f->type == JsonType::String) return f->stringValue;
    if (nullable && f && f->type == JsonType::Null) return {};
    warn(out, std::string("Missing or unsupported field: ") + key); return {};
}
bool optionalCount(const JsonValue& obj, const char* key, std::size_t& output) {
    const auto* value = obj.find(key);
    if (!value || value->type != JsonType::Number || !std::isfinite(value->numberValue) || value->numberValue < 0 ||
        std::floor(value->numberValue) != value->numberValue || value->numberValue > 9007199254740991.0 ||
        value->numberValue >= static_cast<double>(std::numeric_limits<std::size_t>::max())) return false;
    output = static_cast<std::size_t>(value->numberValue); return true;
}
std::string optionalString(const JsonValue& obj, const char* key) {
    const auto* value = obj.find(key);
    return value && value->type == JsonType::String ? value->stringValue : std::string{};
}
bool parse(std::string_view text, editor::JsonValue& out, std::string& error) {
    editor::JsonError e; auto value = editor::parseJson(text, e);
    if (!value) { error = "Invalid GitHub JSON"; return false; }
    out = std::move(*value); return true;
}
}
bool pullRequestEndpoints(std::string_view url, PullRequestEndpoints& output, std::string& error) {
    output = {}; error.clear();
    constexpr std::string_view prefix = "https://github.com/";
    if (!url.starts_with(prefix) || url.size() > 512) { error = "Use a GitHub pull request URL"; return false; }
    url.remove_prefix(prefix.size());
    if (!url.empty() && url.back() == '/') url.remove_suffix(1);
    const auto ownerEnd = url.find('/');
    const auto repoEnd = ownerEnd == std::string_view::npos ? ownerEnd : url.find('/', ownerEnd + 1);
    if (ownerEnd == std::string_view::npos || repoEnd == std::string_view::npos || !url.substr(repoEnd).starts_with("/pull/")) { error = "Invalid pull request URL"; return false; }
    const auto owner = url.substr(0, ownerEnd), repo = url.substr(ownerEnd + 1, repoEnd - ownerEnd - 1), number = url.substr(repoEnd + 6);
    const auto alnum = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
    if (owner.empty() || owner.size() > 39 || owner.front() == '-' || owner.back() == '-' ||
        !std::all_of(owner.begin(), owner.end(), [&](unsigned char c) { return alnum(c) || c == '-'; }) ||
        owner.find("--") != std::string_view::npos || repo.empty() || repo.size() > 100 || repo == "." || repo == ".." ||
        !std::all_of(repo.begin(), repo.end(), [&](unsigned char c) { return alnum(c) || c == '-' || c == '_' || c == '.'; })) {
        error = "Invalid GitHub owner or repository"; return false;
    }
    unsigned long long id = 0;
    const auto parsed = std::from_chars(number.data(), number.data() + number.size(), id);
    if (number.empty() || parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || id == 0) { error = "Invalid pull request number"; return false; }
    const auto base = "https://api.github.com/repos/" + std::string(owner) + "/" + std::string(repo) + "/pulls/" + std::to_string(id);
    output = {base, base + "/files?per_page=100", base + "/comments?per_page=100", "https://api.github.com/repos/" + std::string(owner) + "/" + std::string(repo) + "/issues/" + std::to_string(id) + "/comments?per_page=100"}; return true;
}
bool parsePullRequestConversation(std::string_view text, PullRequestData& output, std::string& error) {
    error.clear(); JsonValue root;
    if(text.size()>maxBytes) {error="GitHub response exceeds size limit";return false;}
    if(!parse(text,root,error) || root.type!=JsonType::Array || root.children.size()>100) {if(error.empty())error="Invalid conversation comments";return false;}
    PullRequestData parsed;
    for(const auto& item:root.children) {
        if(item.type!=JsonType::Object) {error="Invalid conversation comment";return false;}
        PullRequestConversation c;
        if(const auto* user=object(item,"user",parsed)) c.author=field(*user,"login",parsed);
        c.body=field(item,"body",parsed); c.createdAt=field(item,"created_at",parsed);
        parsed.conversation.push_back(std::move(c));
    }
    output=std::move(parsed); return true;
}
bool parsePullRequestData(std::string_view pull, std::string_view files, std::string_view comments, std::string_view checks, bool allPagesSupplied, PullRequestData& output, std::string& error) {
    output = {}; error.clear();
    if (pull.size() > maxBytes || files.size() > maxBytes || comments.size() > maxBytes || checks.size() > maxBytes) { error = "GitHub response exceeds size limit"; return false; }
    PullRequestData out; JsonValue root;
    if (!parse(pull, root, error) || root.type != JsonType::Object) { if (error.empty()) error = "Invalid pull request object"; return false; }
    if (const auto* n = root.find("number"); n && n->type == JsonType::Number && n->numberValue > 0 && std::floor(n->numberValue) == n->numberValue && n->numberValue <= 9007199254740991.0) out.number = std::to_string(static_cast<unsigned long long>(n->numberValue)); else warn(out, "Missing or unsupported pull request number");
    out.title = field(root,"title",out); out.state = field(root,"state",out); out.body = field(root,"body",out,true); out.url = field(root,"html_url",out);
    if (const auto* user = object(root,"user",out)) out.author = field(*user,"login",out);
    if (const auto* base = object(root,"base",out)) out.baseRef = field(*base,"ref",out);
    if (const auto* head = object(root,"head",out)) out.headRef = field(*head,"ref",out);
    if (!allPagesSupplied) warn(out, "Pagination may leave results incomplete");
    auto array = [&](std::string_view input, const char* label, const char* wrapper, auto append) {
        if (input.empty()) { warn(out, std::string("Missing response: ") + label); return true; }
        JsonValue json; if (!parse(input, json, error)) return false;
        const JsonValue* items = &json;
        if (wrapper) { if (json.type != JsonType::Object || !(items = json.find(wrapper))) { error = std::string("Invalid ") + label + " response"; return false; } }
        if (items->type != JsonType::Array || items->children.size() > maxEntries) { error = std::string("Invalid or oversized ") + label + " list"; return false; }
        for (const auto& item : items->children) { if (item.type != JsonType::Object) { error = std::string("Invalid ") + label + " entry"; return false; } append(item); }
        if (wrapper) {
            const auto* total = json.find("total_count");
            if (!total || total->type != JsonType::Number || total->numberValue < 0 || std::floor(total->numberValue) != total->numberValue) warn(out, "Missing or unsupported check count");
            else if (total->numberValue > static_cast<double>(items->children.size())) warn(out, "Check runs are incomplete");
        }
        return true;
    };
    if (!array(files,"files",nullptr,[&](const JsonValue& v){ PullRequestFile f; f.path=field(v,"filename",out); f.status=field(v,"status",out); f.previousPath=optionalString(v,"previous_filename"); const auto* patch=v.find("patch"); if (patch && patch->type==JsonType::String) { f.patch=patch->stringValue; f.hasPatch=true; } const bool additions=optionalCount(v,"additions",f.additions), deletions=optionalCount(v,"deletions",f.deletions); f.hasCounts=additions && deletions; out.files.push_back(std::move(f)); }) ||
        !array(comments,"review comments",nullptr,[&](const JsonValue& v){ PullRequestComment c; if (const auto* user = object(v,"user",out)) c.author = field(*user,"login",out); c.body=field(v,"body",out); c.path=field(v,"path",out); c.createdAt=field(v,"created_at",out); c.side=optionalString(v,"side"); c.hasLine=optionalCount(v,"line",c.line) && c.line>0; out.comments.push_back(std::move(c)); }) ||
        !array(checks,"checks","check_runs",[&](const JsonValue& v){ out.checks.push_back({field(v,"name",out), field(v,"status",out), field(v,"conclusion",out,true), field(v,"html_url",out,true)}); })) return false;
    output = std::move(out); return true;
}
}
