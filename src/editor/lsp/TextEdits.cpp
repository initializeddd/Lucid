#include "editor/lsp/TextEdits.hpp"
#include "editor/document/Document.hpp"
#include "editor/lsp/Client.hpp"
#include "workspace/Project.hpp"
#include "workspace/recovery/History.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
namespace lucid::editor::lsp {
namespace {
bool number(const JsonValue* value, std::size_t& out) {
    if (!value || value->type != JsonType::Number || !std::isfinite(value->numberValue) || value->numberValue < 0 || value->numberValue > 16777216 || std::floor(value->numberValue) != value->numberValue) return false;
    out = static_cast<std::size_t>(value->numberValue); return true;
}
std::vector<std::size_t> lineStarts(std::string_view source) {
    std::vector<std::size_t> starts{0};
    for (std::size_t i = 0; i < source.size(); ++i) if (source[i] == '\n') starts.push_back(i + 1);
    return starts;
}
bool offset(std::string_view source, const std::vector<std::size_t>& starts, const JsonValue* position, std::size_t& result) {
    if (!position || position->type != JsonType::Object) return false;
    std::size_t line = 0, target = 0;
    if (!number(position->find("line"), line) || !number(position->find("character"), target)) return false;
    if (line >= starts.size()) return false;
    const auto start = starts[line];
    const auto end = line + 1 < starts.size() ? starts[line + 1] - 1 : source.size();
    std::size_t units = 0, cursor = start;
    while (cursor < end && units < target) {
        const auto first = static_cast<unsigned char>(source[cursor]);
        const std::size_t count = first < 0x80 ? 1 : (first & 0xe0U) == 0xc0U ? 2 : (first & 0xf0U) == 0xe0U ? 3 : 4;
        const auto width = count == 4 ? 2U : 1U;
        if (cursor + count > end || units + width > target) return false;
        cursor += count; units += width;
    }
    if (units != target) return false;
    result = cursor; return true;
}
struct Edit { std::size_t start, end; std::string text; };
}
std::optional<std::string> signatureParameterLabel(const JsonValue& signature, std::size_t parameter) {
    const auto* params = signature.find("parameters"); const auto* signatureLabel = signature.find("label");
    if (!params || params->type != JsonType::Array || parameter >= params->children.size()) return std::nullopt;
    const auto* label = params->children[parameter].find("label");
    if (!label) return std::nullopt;
    if (label->type == JsonType::String) return label->stringValue;
    if (label->type != JsonType::Array || label->children.size() != 2 || !signatureLabel || signatureLabel->type != JsonType::String) return std::nullopt;
    std::size_t begin = 0, end = 0;
    if (!number(&label->children[0], begin) || !number(&label->children[1], end) || begin > end) return std::nullopt;
    const auto& text = signatureLabel->stringValue; std::string error;
    if (!Document::canEditLosslessly(text, error)) return std::nullopt;
    std::size_t cursor = 0, units = 0, beginByte = std::string::npos, endByte = std::string::npos;
    while (true) {
        if (units == begin) beginByte = cursor;
        if (units == end) { endByte = cursor; break; }
        if (cursor == text.size() || units > end) break;
        const auto first = static_cast<unsigned char>(text[cursor]);
        const std::size_t bytes = first < 0x80 ? 1 : (first & 0xe0U) == 0xc0U ? 2 : (first & 0xf0U) == 0xe0U ? 3 : 4;
        cursor += bytes; units += bytes == 4 ? 2 : 1;
    }
    if (beginByte == std::string::npos || endByte == std::string::npos) return std::nullopt;
    return text.substr(beginByte, endByte - beginByte);
}
bool applyTextEdits(std::string_view source, const JsonValue& edits, std::string& output, std::string& error) {
    output.clear(); error.clear();
    if (edits.type != JsonType::Array || edits.children.size() > 10000 || !Document::canEditLosslessly(source, error) || source.find('\r') != std::string_view::npos) { if (error.empty()) error = "Invalid language-server edit list"; return false; }
    std::vector<Edit> parsed;
    const auto starts = lineStarts(source);
    for (const auto& value : edits.children) {
        const auto* range = value.find("range"); const auto* text = value.find("newText"); std::size_t start = 0, end = 0;
        if (!range || range->type != JsonType::Object || !text || text->type != JsonType::String || !offset(source, starts, range->find("start"), start) || !offset(source, starts, range->find("end"), end) || start > end) { error = "Invalid language-server edit range"; return false; }
        std::string textError;
        if (!Document::canEditLosslessly(text->stringValue, textError)) { error = "Invalid text returned by language server"; return false; }
        parsed.push_back({start, end, Document::fromUtf8(text->stringValue).toUtf8()});
    }
    std::sort(parsed.begin(), parsed.end(), [](const Edit& a, const Edit& b) { return a.start < b.start || (a.start == b.start && a.end < b.end); });
    std::size_t cursor = 0, previousStart = 0; bool first = true;
    constexpr std::size_t limit = 16U * 1024U * 1024U;
    const auto append = [&output](std::string_view text) {
        constexpr std::size_t maxSize = 16U * 1024U * 1024U;
        if (text.size() > maxSize - output.size()) return false;
        output.append(text); return true;
    };
    for (const auto& edit : parsed) {
        if (edit.start < cursor || (!first && edit.start == previousStart)) { output.clear(); error = "Overlapping language-server edits"; return false; }
        if (!append(source.substr(cursor, edit.start - cursor)) || !append(edit.text)) { output.clear(); error = "Language-server edit is too large"; return false; }
        cursor = edit.end; previousStart = edit.start; first = false;
    }
    if (output.size() > limit || !append(source.substr(cursor))) { output.clear(); error = "Language-server edit is too large"; return false; }
    return true;
}
bool prepareWorkspaceEdits(const workspace::Project& project, const JsonValue& edit,
    std::string_view activePath, std::string_view activeText, int activeVersion,
    std::vector<WorkspaceFileEdit>& files, std::string& error) {
    files.clear(); error.clear();
    if (edit.type != JsonType::Object || edit.find("changeAnnotations")) { error = "Unsupported workspace edit or annotations"; return false; }
    const auto* changes = edit.find("changes"); const auto* documentChanges = edit.find("documentChanges");
    if ((changes != nullptr) == (documentChanges != nullptr)) { error = "Workspace edit must contain exactly one edit list"; return false; }
    const auto* list = changes ? changes : documentChanges;
    if ((changes && list->type != JsonType::Object) || (documentChanges && list->type != JsonType::Array) || list->children.size() > 1000) {
        error = "Invalid or oversized workspace edit"; return false;
    }
    std::vector<WorkspaceFileEdit> prepared; std::size_t totalBytes = 0, totalEdits = 0;
    for (const auto& item : list->children) {
        std::string uri; const JsonValue* edits = nullptr; const JsonValue* version = nullptr;
        if (changes) { uri = item.key; edits = &item; }
        else {
            if (item.type != JsonType::Object || item.find("kind")) { error = "Workspace file operations are not supported"; return false; }
            const auto* document = item.find("textDocument"); const auto* value = document ? document->find("uri") : nullptr;
            if (!document || document->type != JsonType::Object || !value || value->type != JsonType::String) { error = "Invalid workspace document"; return false; }
            uri = value->stringValue; version = document->find("version"); edits = item.find("edits");
        }
        std::filesystem::path absolute;
        if (!pathFromFileUri(uri, absolute)) { error = "Invalid workspace file URI"; return false; }
        std::error_code ec; absolute = std::filesystem::weakly_canonical(absolute, ec);
        const auto relative = absolute.lexically_relative(project.root());
        if (ec || relative.empty() || *relative.begin() == "..") { error = "Workspace edit is outside the project"; return false; }
        const std::string path = relative.generic_string(); std::string pathError;
        if (project.resolve(path, pathError) != absolute || !pathError.empty() || !project.exists(path) || project.isDirectory(path) ||
            std::any_of(prepared.begin(), prepared.end(), [&path](const auto& file) { return file.path == path; })) {
            error = "Invalid or duplicate workspace file"; return false;
        }
        if (version && version->type != JsonType::Null && (path != activePath || version->type != JsonType::Number || version->numberValue != activeVersion)) {
            error = "Workspace edit document version is unavailable or stale"; return false;
        }
        if (!edits || edits->type != JsonType::Array || std::any_of(edits->children.begin(), edits->children.end(), [](const auto& value) { return value.find("annotationId") != nullptr; })) {
            error = "Unsupported or invalid workspace text edits"; return false;
        }
        if (edits->children.size() > 10000U - totalEdits) { error = "Workspace edit has too many changes"; return false; }
        totalEdits += edits->children.size();
        std::string original;
        if (path == activePath) original = activeText;
        else if (!project.readFile(path, original, error)) { error = "Cannot read workspace file " + path + ": " + error; return false; }
        if (original.size() > 64U * 1024U * 1024U - totalBytes) { error = "Workspace edit exceeds memory limit"; return false; }
        totalBytes += original.size();
        if (std::any_of(original.begin(), original.end(), [](unsigned char c) { return c == 0 || (c < 32 && c != '\r' && c != '\n' && c != '\t'); }) || !Document::canEditLosslessly(original, error)) {
            error = "Workspace file cannot be edited safely: " + path; return false;
        }
        auto document = Document::fromUtf8(original); std::string normalized, updated;
        const auto source = document.toUtf8();
        if (!applyTextEdits(source, *edits, normalized, error)) return false;
        const auto starts = lineStarts(source);
        std::vector<WorkspaceTextChange> summaries;
        for (const auto& change : edits->children) {
            const auto* range = change.find("range"); const auto* replacement = change.find("newText");
            const auto* startPosition = range->find("start"); std::size_t start = 0, end = 0, row = 0;
            if (!offset(source, starts, startPosition, start) || !offset(source, starts, range->find("end"), end) || !number(startPosition->find("line"), row)) { error = "Invalid workspace edit range"; return false; }
            const auto preview = [](std::string_view text, bool& truncated) {
                constexpr std::size_t limit = 160;
                auto count = std::min(text.size(), limit);
                while (count && count < text.size() && (static_cast<unsigned char>(text[count]) & 0xc0U) == 0x80U) --count;
                truncated = truncated || count < text.size();
                std::string result;
                for (std::size_t i = 0; i < count; ++i) {
                    if (text[i] == '\r' || text[i] == '\n') { if (text[i] == '\r' && i + 1 < count && text[i + 1] == '\n') ++i; result += " ↵ "; }
                    else if (text[i] == '\t') result += " ⇥ "; else result.push_back(text[i]);
                }
                if (count < text.size()) result += "…";
                return result.empty() ? std::string("(empty)") : result;
            };
            bool truncated = false;
            auto before = preview(std::string_view(source).substr(start, end - start), truncated);
            auto after = preview(replacement->stringValue, truncated);
            summaries.push_back({row, std::move(before), std::move(after), truncated});
        }
        auto result = Document::fromUtf8(normalized);
        if (!result.serializeForFile(original, updated, error) || updated.size() > workspace::Project::maxFileSize ||
            std::any_of(updated.begin(), updated.end(), [](unsigned char c) { return c == 0 || (c < 32 && c != '\r' && c != '\n' && c != '\t'); })) {
            error = "Workspace edit cannot be saved safely: " + path; return false;
        }
        prepared.push_back({path, std::move(original), std::move(updated), std::move(summaries)});
    }
    files = std::move(prepared); return true;
}
bool applyWorkspaceEdits(workspace::Project& project, const std::vector<WorkspaceFileEdit>& files,
    const std::filesystem::path& historyRoot, std::vector<std::string>& changedPaths, std::string& error) {
    changedPaths.clear(); error.clear();
    if (historyRoot.empty() || files.empty() || files.size() > 1000) { error = "Workspace edit has no files or recovery storage"; return false; }
    std::vector<std::string> seen;
    for (const auto& file : files) {
        if (file.path.empty() || std::find(seen.begin(), seen.end(), file.path) != seen.end()) { error = "Invalid or duplicate workspace file"; return false; }
        seen.push_back(file.path);
        std::string current, validationError;
        if (!project.readFile(file.path, current, error) || current != file.original) { error = "File changed since review: " + file.path; return false; }
        const auto unsafe = [](unsigned char c) { return c == 0 || (c < 32 && c != '\r' && c != '\n' && c != '\t'); };
        if (file.updated.size() > workspace::Project::maxFileSize || std::any_of(file.updated.begin(), file.updated.end(), unsafe) ||
            !Document::canEditLosslessly(file.updated, validationError)) { error = "Unsafe workspace edit: " + file.path; return false; }
    }
    for (const auto& file : files) {
        if (file.original == file.updated) continue;
        workspace::recovery::Snapshot backup; backup.project = project.root().generic_string(); backup.file = file.path; backup.contents = file.original;
        if (!workspace::recovery::recordHistory(historyRoot, backup, error)) { error = "No files changed; could not back up " + file.path + ": " + error; return false; }
    }
    for (const auto& file : files) {
        if (file.original == file.updated) continue;
        if (!project.writeFile(file.path, file.updated, error, &file.original)) {
            error = "Stopped after " + std::to_string(changedPaths.size()) + " files: " + error; return false;
        }
        changedPaths.push_back(file.path);
    }
    return true;
}
}
