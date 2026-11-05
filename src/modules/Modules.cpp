#include "modules/Modules.hpp"
#include "editor/document/Document.hpp"
#include "editor/json/Json.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>
namespace lucid::modules {
bool supportsCapability(std::string_view capability) noexcept {
    if (capability == "snippet-indent-v1") return true;
    if (capability == "api-response-assertions-v1" || capability == "api-response-save-v1") return true;
#if !defined(__APPLE__)
    if (capability == "web-component-preview-macos-v1") return false;
#endif
    return capability == "crash-lldb-text-v1" || capability == "diagram-flow-rounded-v1" || capability == "crash-ips-source-map-v1" || capability == "crash-apple-ips-v1" || capability == "crash-apple-text-v1" || capability == "crash-gdb-text-v1" || capability == "rust-diagnostic-run-v1" || capability == "cmake-build-target-v1" || capability == "cmake-cache-values-v1" || capability == "toolchain-configure-v1" || capability == "toolchain-target-edit-v1" || capability == "openocd-bin-v1" || capability == "serial-continuous-v1" || capability == "usb-board-identification-v1" || capability == "svd-registers-v1" || capability == "markdown-external-links-v1" || capability == "toolchain-profiles-v1" || capability == "cmake-cache-edit-v1" || capability == "coverage-cobertura-branches-v1" || capability == "coverage-cobertura-v1" || capability == "api-environment-preflight-v1" || capability == "api-environments-v1" || capability == "openocd-flash-v1" || capability == "git-range-history-v1" || capability == "python-pylsp-install-v1" || capability == "pull-request-review-v1" || capability == "pull-request-comment-v1" || capability == "container-lifecycle-v1" || capability == "sql-parameters-v1" || capability == "visual-mask-draw-v1" || capability == "visual-config-edit-v1" || capability == "visual-review-crop-v1" || capability == "visual-regression-config-v1" || capability == "shader-render-preview-v1" || capability == "shader-spirv-inspection-v1" || capability == "shader-includes-v1" || capability == "shader-artifact-v1" || capability == "web-component-preview-macos-v1" || capability == "microcontroller-serial-v1" || capability == "shader-compile-v1" || capability == "pull-request-companion-v1" || capability == "container-inspection-v1" || capability == "snippet-v1" || capability == "snippet-placeholder-v1" || capability == "snippet-placeholders-v2" || capability == "project-secret-scan-v1" || capability == "local-error-explainer-v1" || capability == "local-debug-explainer-v1" || capability == "cmake-codemodel-v1" || capability == "cmake-query-v1" || capability == "markdown-outline-v1" || capability == "markdown-preview-v1" || capability == "markdown-local-links-v1" || capability == "coverage-lcov-v1" || capability == "git-file-history-v1" || capability == "benchmark-samples-v1" || capability == "benchmark-comparison-v1" || capability == "binary-inspector-v1" || capability == "localization-workspace-v1" || capability == "design-token-aliases-v1" || capability == "design-token-explorer-v1" || capability == "log-json-lines-v1" || capability == "log-correlation-v1" || capability == "python-environments-v1" || capability == "python-environments-v2" || capability == "junit-producer-compat-v1" || capability == "junit-import-v1" || capability == "ctest-runner-v2" || capability == "hex-editor-v1" || capability == "linker-map-v1" || capability == "crash-dump-reader-v1" || capability == "rust-ownership-lens-v1" || capability == "diagram-flow-branches-v1" || capability == "diagram-flow-bidirectional-v1" || capability == "diagram-flow-chains-v1" || capability == "diagram-flow-cycles-v1" || capability == "diagram-preview-v1" || capability == "accessibility-audit-v1" || capability == "release-note-builder-v1" || capability == "visual-regression-review-v1" || capability == "sql-snapshot-v1" || capability == "api-request-studio-v1" || capability == "dependency-risk-review-v1" || capability == "dependency-risk-go-v1" || capability == "dependency-risk-cargo-v1" || capability == "dependency-provenance-v1" || capability == "api-collection-edit-v1" || capability == "sql-change-row-diff-v1" || capability == "sql-change-bound-v1" || capability == "sql-change-copy-v1" || capability == "sql-columns-v1" || capability == "container-project-actions-v1" || capability == "localization-edit-v1" || capability == "dependency-artifact-verify-v1" || capability == "dependency-artifact-sha2-v2" || capability == "dependency-artifact-go-v1";
}
namespace {
using editor::JsonType;
bool uniqueKeys(const editor::JsonValue& value, unsigned depth = 0) {
    if (depth > 32) return false;
    std::set<std::string_view> keys;
    for (const auto& child : value.children) {
        if (value.type == JsonType::Object && !keys.insert(child.key).second) return false;
        if (!uniqueKeys(child, depth + 1)) return false;
    }
    return true;
}
bool validId(std::string_view id) {
    if (id.empty() || id.size() > 80 || id == "." || id == "..") return false;
    for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
bool stringField(const editor::JsonValue& root, const char* key, std::string& value, std::size_t limit) {
    const auto* v = root.find(key); if (!v || v->type != JsonType::String || v->stringValue.empty() || v->stringValue.size() > limit) return false;
    value = v->stringValue; return true;
}
bool boolField(const editor::JsonValue& root, const char* key, bool& value) {
    const auto* v = root.find(key); if (!v || v->type != JsonType::Bool) return false;
    value = v->boolValue; return true;
}
bool safeDirectory(const std::filesystem::path& root, std::string_view id, std::filesystem::path& path, std::string& error) {
    if (!validId(id)) { error = "Invalid module ID"; return false; }
    path = root / std::string(id); std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || status.type() != std::filesystem::file_type::directory) { error = "Module directory is missing or is a link"; return false; }
    return true;
}
Module parseManifest(std::string id, std::string_view text) {
    Module m; m.id = std::move(id); m.name = m.id;
    if (!validId(m.id)) { m.error = "Invalid module ID"; return m; }
    if (text.size() > 65536) { m.error = "Module manifest is too large"; return m; }
    editor::JsonError jsonError; const auto json = editor::parseJson(text, jsonError);
    if (!json || json->type != JsonType::Object || !uniqueKeys(*json) || !stringField(*json, "name", m.name, 128) || !stringField(*json, "version", m.version, 64)) { m.error = "Invalid module manifest"; return m; }
    if (const auto* description = json->find("description")) {
        if (description->type != JsonType::String || description->stringValue.size() > 512) { m.error = "Invalid module description"; return m; }
        m.description = description->stringValue;
    }
    if (const auto* required = json->find("requires")) {
        if (required->type != JsonType::Array || required->children.size() > 32) { m.error = "Invalid module capability requirements"; return m; }
        for (const auto& capability : required->children) {
            if (capability.type != JsonType::String || capability.stringValue.empty() || capability.stringValue.size() > 80 ||
                std::find(m.requirements.begin(), m.requirements.end(), capability.stringValue) != m.requirements.end()) { m.error = "Invalid module capability requirements"; return m; }
            m.requirements.push_back(capability.stringValue);
        }
    }
    const auto* p = json->find("permissions"); const auto* c = json->find("commands");
    if (!p || p->type != JsonType::Object || (p->children.size() != 4 && p->children.size() != 5) || !boolField(*p, "project", m.project) ||
        !boolField(*p, "network", m.network) || !boolField(*p, "processes", m.processes) || !boolField(*p, "settings", m.settings) ||
        (p->children.size() == 5 && !boolField(*p, "devices", m.devices)) ||
        !c || c->type != JsonType::Array || c->children.empty() || c->children.size() > 32) { m.error = "Invalid permissions or commands"; return m; }
    for (const auto& item : c->children) {
        Command command;
        if (item.type != JsonType::Object || !stringField(item, "title", command.title, 128)) { m.error = "Invalid command"; return m; }
        const auto* action = item.find("action");
        if (action) {
            if (action->type != JsonType::String || item.find("insert") || item.find("snippet") ||
                !(((action->stringValue == "scan-secrets" || action->stringValue == "browse-cmake" || action->stringValue == "prepare-cmake-query" || action->stringValue == "markdown-local-links" || action->stringValue == "coverage-lcov") && m.project) || (action->stringValue == "markdown-external-links" && m.project && m.network && std::find(m.requirements.begin(), m.requirements.end(), "markdown-external-links-v1") != m.requirements.end()) || (action->stringValue == "web-preview" && m.project && m.network && std::find(m.requirements.begin(), m.requirements.end(), "web-component-preview-macos-v1") != m.requirements.end()) || ((action->stringValue == "serial-monitor" || action->stringValue == "board-profile") && m.devices && std::find(m.requirements.begin(), m.requirements.end(), "microcontroller-serial-v1") != m.requirements.end()) || (action->stringValue == "flash-firmware" && m.devices && m.project && m.processes && std::find(m.requirements.begin(), m.requirements.end(), "openocd-flash-v1") != m.requirements.end()) || (action->stringValue == "compile-shader" && m.project && m.processes && std::find(m.requirements.begin(), m.requirements.end(), "shader-compile-v1") != m.requirements.end()) || (action->stringValue == "fetch-pull-request" && !m.project && m.network && std::find(m.requirements.begin(), m.requirements.end(), "pull-request-companion-v1") != m.requirements.end()) || (action->stringValue == "configured-container-actions" && m.project && m.network && m.processes && std::find(m.requirements.begin(), m.requirements.end(), "container-project-actions-v1") != m.requirements.end()) || (action->stringValue == "inspect-containers" && (!m.project || std::find(m.requirements.begin(), m.requirements.end(), "container-project-actions-v1") != m.requirements.end()) && m.processes && m.network && std::find(m.requirements.begin(), m.requirements.end(), "container-inspection-v1") != m.requirements.end()) || ((action->stringValue == "compare-visual-images" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "visual-regression-review-v1") != m.requirements.end()) || (action->stringValue == "compare-visual-configured" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "visual-regression-config-v1") != m.requirements.end())) || (action->stringValue == "review-dependency-risk" && (!m.project || std::find(m.requirements.begin(), m.requirements.end(), "dependency-artifact-verify-v1") != m.requirements.end()) && m.network && std::find(m.requirements.begin(), m.requirements.end(), "dependency-risk-review-v1") != m.requirements.end()) || (action->stringValue == "identify-usb-boards" && m.project && m.devices && std::find(m.requirements.begin(), m.requirements.end(), "usb-board-identification-v1") != m.requirements.end()) || (action->stringValue == "inspect-svd-registers" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "svd-registers-v1") != m.requirements.end()) || (action->stringValue == "inspect-toolchain-profiles" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "toolchain-profiles-v1") != m.requirements.end()) || (action->stringValue == "edit-cmake-options" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "cmake-cache-edit-v1") != m.requirements.end()) || (action->stringValue == "coverage-cobertura" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "coverage-cobertura-v1") != m.requirements.end()) || (action->stringValue == "open-api-environment" && m.project && m.network && std::find(m.requirements.begin(), m.requirements.end(), "api-environments-v1") != m.requirements.end()) || (action->stringValue == "edit-api-collection" && std::find(m.requirements.begin(), m.requirements.end(), "api-collection-edit-v1") != m.requirements.end()) || (action->stringValue == "open-api-requests" && m.network && std::find(m.requirements.begin(), m.requirements.end(), "api-request-studio-v1") != m.requirements.end()) || (action->stringValue == "sql-change-bound" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "sql-change-bound-v1") != m.requirements.end()) || (action->stringValue == "sql-change-preview" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "sql-change-copy-v1") != m.requirements.end()) || ((action->stringValue == "sql-query-bound" || action->stringValue == "sql-explain-bound") && m.project && std::find(m.requirements.begin(), m.requirements.end(), "sql-parameters-v1") != m.requirements.end()) || ((action->stringValue == "sql-query" || action->stringValue == "sql-explain") && m.project && std::find(m.requirements.begin(), m.requirements.end(), "sql-snapshot-v1") != m.requirements.end()) || (action->stringValue == "release-note-builder" && m.project && m.processes && std::find(m.requirements.begin(), m.requirements.end(), "release-note-builder-v1") != m.requirements.end()) || (action->stringValue == "accessibility-audit" && !m.project && std::find(m.requirements.begin(), m.requirements.end(), "accessibility-audit-v1") != m.requirements.end()) || (action->stringValue == "diagram-preview" && !m.project && std::find(m.requirements.begin(), m.requirements.end(), "diagram-preview-v1") != m.requirements.end()) || (action->stringValue == "run-rust-ownership" && m.project && m.processes && std::find(m.requirements.begin(),m.requirements.end(),"rust-diagnostic-run-v1")!=m.requirements.end()) || (action->stringValue == "review-rust-ownership" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "rust-ownership-lens-v1") != m.requirements.end()) || (action->stringValue == "crash-dump-reader" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "crash-dump-reader-v1") != m.requirements.end()) || (action->stringValue == "review-linker-map" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "linker-map-v1") != m.requirements.end()) || (action->stringValue == "hex-editor" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "hex-editor-v1") != m.requirements.end()) || (action->stringValue == "import-junit" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "junit-import-v1") != m.requirements.end()) || (action->stringValue == "run-ctest" && m.project && m.processes && m.settings && std::find(m.requirements.begin(), m.requirements.end(), "ctest-runner-v2") != m.requirements.end()) || (action->stringValue == "install-python-pylsp" && m.project && m.processes && m.network && m.settings && std::find(m.requirements.begin(), m.requirements.end(), "python-pylsp-install-v1") != m.requirements.end()) || (action->stringValue == "manage-python-environments" && m.project && m.processes && m.settings && std::find(m.requirements.begin(), m.requirements.end(), "python-environments-v2") != m.requirements.end()) || (action->stringValue == "browse-python-environments" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "python-environments-v1") != m.requirements.end()) || (action->stringValue == "log-correlation-json" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "log-json-lines-v1") != m.requirements.end()) || (action->stringValue == "log-correlation" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "log-correlation-v1") != m.requirements.end()) || (action->stringValue == "design-token-explorer" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "design-token-explorer-v1") != m.requirements.end()) || (action->stringValue == "compare-localization" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "localization-workspace-v1") != m.requirements.end()) || (action->stringValue == "binary-inspector" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "binary-inspector-v1") != m.requirements.end()) || (action->stringValue == "benchmark-comparison" && m.project && std::find(m.requirements.begin(), m.requirements.end(), "benchmark-comparison-v1") != m.requirements.end()) || (action->stringValue == "git-range-history" && m.project && m.processes && std::find(m.requirements.begin(), m.requirements.end(), "git-range-history-v1") != m.requirements.end()) || (action->stringValue == "git-file-history" && m.project && m.processes && std::find(m.requirements.begin(), m.requirements.end(), "git-file-history-v1") != m.requirements.end()) || ((action->stringValue == "explain-diagnostic-local" || action->stringValue == "explain-debug-local") && !m.project) || action->stringValue == "markdown-outline" || action->stringValue == "markdown-preview")) { m.error = "Invalid or unauthorized module action"; return m; }
            command.action = action->stringValue; m.commands.push_back(std::move(command)); continue;
        }
        if (!stringField(item, "insert", command.insert, 4096)) { m.error = "Invalid text command"; return m; }
        if (const auto* snippet = item.find("snippet")) {
            if (snippet->type != JsonType::Bool) { m.error = "Invalid snippet flag"; return m; }
            command.snippet = snippet->boolValue;
        }
        std::string textError;
        if (std::any_of(command.insert.begin(), command.insert.end(), [](unsigned char c) {
                return (c < 32 && c != '\n' && c != '\r' && c != '\t') || c == 127;
            }) || !editor::Document::canEditLosslessly(command.insert, textError)) {
            m.error = "Module command contains unsupported text"; return m;
        }
        if (command.snippet) {
            ExpandedSnippet expanded;
            const bool multiple = std::find(m.requirements.begin(), m.requirements.end(), "snippet-placeholders-v2") != m.requirements.end();
            if (!expandSnippet(command.insert, "", expanded, textError, multiple || std::find(m.requirements.begin(), m.requirements.end(), "snippet-placeholder-v1") != m.requirements.end(), multiple)) { m.error = textError; return m; }
        }
        m.commands.push_back(std::move(command));
    }
    m.valid = true;
    const auto unsupported = std::find_if(m.requirements.begin(), m.requirements.end(), [](const std::string& capability) {
        return !supportsCapability(capability);
    });
    if (unsupported != m.requirements.end()) m.error = "Requires unsupported capability: " + *unsupported;
    else if ((std::find(m.requirements.begin(), m.requirements.end(), "pull-request-review-v1") != m.requirements.end() || std::find(m.requirements.begin(), m.requirements.end(), "pull-request-comment-v1") != m.requirements.end()) && (!m.network || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "fetch-pull-request"; }))) m.error = "Pull request review requires network access and a pull request command";
    else if ((std::find(m.requirements.begin(), m.requirements.end(), "dependency-risk-go-v1") != m.requirements.end() || std::find(m.requirements.begin(), m.requirements.end(), "dependency-risk-cargo-v1") != m.requirements.end() || std::find(m.requirements.begin(), m.requirements.end(), "dependency-provenance-v1") != m.requirements.end()) && (std::find(m.requirements.begin(), m.requirements.end(), "dependency-risk-review-v1") == m.requirements.end() || !m.network || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "review-dependency-risk"; }))) m.error = "Extended dependency review requires network access and a dependency review command";
    else if (std::find(m.requirements.begin(),m.requirements.end(),"cmake-build-target-v1")!=m.requirements.end() && (!m.project || !m.processes || std::find(m.requirements.begin(),m.requirements.end(),"cmake-codemodel-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="browse-cmake";}))) m.error="Target build requires CMake browsing and process access";
    else if (std::find(m.requirements.begin(),m.requirements.end(),"cmake-cache-values-v1")!=m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"cmake-cache-edit-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="edit-cmake-options";}))) m.error="Typed cache editing requires CMake cache edit access";
    else if (std::find(m.requirements.begin(),m.requirements.end(),"toolchain-configure-v1")!=m.requirements.end() && (!m.project || !m.processes || std::find(m.requirements.begin(),m.requirements.end(),"toolchain-profiles-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="inspect-toolchain-profiles";}))) m.error="Target configuration requires project, process and toolchain profile access";
    else if (std::find(m.requirements.begin(),m.requirements.end(),"toolchain-target-edit-v1")!=m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"toolchain-profiles-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="inspect-toolchain-profiles";}))) m.error="Target editing requires project and toolchain profile access";
    else if (std::find(m.requirements.begin(),m.requirements.end(),"openocd-bin-v1")!=m.requirements.end() && (std::find(m.requirements.begin(),m.requirements.end(),"openocd-flash-v1")==m.requirements.end() || !m.devices || !m.project || !m.processes || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="flash-firmware";}))) m.error="BIN flashing requires OpenOCD flash capability and permissions";
    else if (std::find(m.requirements.begin(),m.requirements.end(),"serial-continuous-v1")!=m.requirements.end() && (!m.devices || std::find(m.requirements.begin(),m.requirements.end(),"microcontroller-serial-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="serial-monitor";}))) m.error="Continuous serial monitoring requires device access and serial-monitor";
    else if ((std::find(m.requirements.begin(), m.requirements.end(), "shader-includes-v1") != m.requirements.end() || std::find(m.requirements.begin(), m.requirements.end(), "shader-spirv-inspection-v1") != m.requirements.end() || std::find(m.requirements.begin(), m.requirements.end(), "shader-render-preview-v1") != m.requirements.end()) && (!m.project || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "compile-shader"; }))) m.error = "Shader include inspection requires project access and a shader command";
    else if ((std::find(m.requirements.begin(), m.requirements.end(), "dependency-artifact-sha2-v2") != m.requirements.end() || std::find(m.requirements.begin(), m.requirements.end(), "dependency-artifact-go-v1") != m.requirements.end()) && std::find(m.requirements.begin(), m.requirements.end(), "dependency-artifact-verify-v1") == m.requirements.end()) m.error = "Extended artifact checks require artifact verification";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "dependency-artifact-verify-v1") != m.requirements.end() && (!m.project || !m.network || std::find(m.requirements.begin(), m.requirements.end(), "dependency-provenance-v1") == m.requirements.end() || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "review-dependency-risk"; }))) m.error = "Artifact verification requires project access and dependency provenance";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "localization-edit-v1") != m.requirements.end() && (!m.project || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "compare-localization"; }))) m.error = "Localization editing requires project access and a comparison command";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "container-project-actions-v1") != m.requirements.end() && (!m.project || !m.network || !m.processes || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "configured-container-actions"; }))) m.error = "Configured container actions require project, network and process access";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "api-response-assertions-v1") != m.requirements.end() && (!m.project || !m.network || std::find(m.requirements.begin(), m.requirements.end(), "api-request-studio-v1") == m.requirements.end() || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "open-api-requests" || c.action == "open-api-environment"; }))) m.error = "Response assertions require project access and an API request command";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "api-environment-preflight-v1") != m.requirements.end() && (!m.project || !m.network || std::find(m.requirements.begin(),m.requirements.end(),"api-environments-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="open-api-environment"; }))) m.error = "API preflight requires project access and environment profiles";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "junit-producer-compat-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"junit-import-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="import-junit"; }))) m.error = "JUnit producer compatibility requires project access and JUnit import";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "junit-import-v1") != m.requirements.end() && (!m.project || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="import-junit"; }))) m.error = "JUnit import requires project access and an import command";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "coverage-cobertura-branches-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"coverage-cobertura-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="coverage-cobertura"; }))) m.error = "Cobertura branch coverage requires project access and Cobertura import";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "design-token-aliases-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"design-token-explorer-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="design-token-explorer"; }))) m.error = "Design token aliases require project access and token exploration";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "diagram-flow-rounded-v1") != m.requirements.end() && (std::find(m.requirements.begin(),m.requirements.end(),"diagram-preview-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="diagram-preview"; }))) m.error = "Diagram node shapes require diagram preview support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "diagram-flow-branches-v1") != m.requirements.end() && (std::find(m.requirements.begin(),m.requirements.end(),"diagram-preview-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="diagram-preview"; }))) m.error = "Diagram branches require diagram preview support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "diagram-flow-bidirectional-v1") != m.requirements.end() && (std::find(m.requirements.begin(),m.requirements.end(),"diagram-preview-v1")==m.requirements.end() || std::find(m.requirements.begin(),m.requirements.end(),"diagram-flow-cycles-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="diagram-preview"; }))) m.error = "Bidirectional diagrams require cyclic diagram preview support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "diagram-flow-chains-v1") != m.requirements.end() && (std::find(m.requirements.begin(),m.requirements.end(),"diagram-preview-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="diagram-preview"; }))) m.error = "Diagram chains require diagram preview support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "diagram-flow-cycles-v1") != m.requirements.end() && (std::find(m.requirements.begin(),m.requirements.end(),"diagram-preview-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="diagram-preview"; }))) m.error = "Cyclic diagrams require diagram preview support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "benchmark-samples-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"benchmark-comparison-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="benchmark-comparison"; }))) m.error = "Benchmark samples require project access and benchmark comparison support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "log-json-lines-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"log-correlation-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="log-correlation-json"; }))) m.error = "JSON log correlation requires project access and log correlation support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "visual-mask-draw-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"visual-config-edit-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="compare-visual-configured"; }))) m.error = "Drawing visual masks requires configured visual editing";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "visual-config-edit-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"visual-regression-config-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="compare-visual-configured"; }))) m.error = "Visual config editing requires project access and configured review";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "visual-review-crop-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"visual-regression-review-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="compare-visual-images" || c.action=="compare-visual-configured"; }))) m.error = "Visual crop requires project access and visual review support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "sql-change-row-diff-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"sql-change-copy-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="sql-change-preview" || c.action=="sql-change-bound"; }))) m.error = "SQL row comparison requires project access and change-copy support";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "sql-change-bound-v1") != m.requirements.end() && (!m.project || std::find(m.requirements.begin(),m.requirements.end(),"sql-change-copy-v1")==m.requirements.end() || std::find(m.requirements.begin(),m.requirements.end(),"sql-parameters-v1")==m.requirements.end() || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="sql-change-bound"; }))) m.error = "Bound SQL changes require project access, change-copy and parameter capabilities";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "sql-change-copy-v1") != m.requirements.end() && (!m.project || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "sql-change-preview"; }))) m.error = "SQL change previews require project access and a preview command";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "sql-columns-v1") != m.requirements.end() && (!m.project || std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "sql-query" || c.action == "sql-explain" || c.action == "sql-query-bound" || c.action == "sql-explain-bound"; }))) m.error = "SQL column inspection requires project access and a query command";
    else if (std::find(m.requirements.begin(), m.requirements.end(), "api-collection-edit-v1") != m.requirements.end() && std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "edit-api-collection"; })) m.error = "API collection editing requires an editing command";
    else if (std::find(m.requirements.begin(),m.requirements.end(),"api-response-save-v1")!=m.requirements.end() && (!m.project || std::none_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="open-api-requests" || c.action=="open-api-environment"; }))) m.error = "Response saving requires project access and an API request command";
    else if ((m.devices && std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "serial-monitor" || c.action == "identify-usb-boards" || c.action == "board-profile" || c.action == "flash-firmware"; })) || (m.network && std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "markdown-external-links" || c.action == "install-python-pylsp" || c.action == "web-preview" || c.action == "fetch-pull-request" || c.action == "open-api-requests" || c.action == "open-api-environment" || c.action == "review-dependency-risk" || c.action == "inspect-containers" || c.action == "configured-container-actions"; })) || (m.settings && std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "install-python-pylsp" || c.action == "manage-python-environments" || c.action == "run-ctest"; })) || (m.processes && !(std::find(m.requirements.begin(),m.requirements.end(),"cmake-build-target-v1")!=m.requirements.end() && std::any_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="browse-cmake";})) && !(std::find(m.requirements.begin(),m.requirements.end(),"toolchain-configure-v1")!=m.requirements.end() && std::any_of(m.commands.begin(),m.commands.end(),[](const Command& c){return c.action=="inspect-toolchain-profiles";})) && std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "run-rust-ownership" || c.action == "flash-firmware" || c.action == "compile-shader" || c.action == "inspect-containers" || c.action == "configured-container-actions" || c.action == "release-note-builder" || c.action == "git-file-history" || c.action == "git-range-history" || c.action == "install-python-pylsp" || c.action == "manage-python-environments" || c.action == "run-ctest"; })) || (m.project && !(std::find(m.requirements.begin(), m.requirements.end(), "dependency-artifact-verify-v1") != m.requirements.end() && std::any_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "review-dependency-risk"; })) && !((std::find(m.requirements.begin(),m.requirements.end(),"api-response-save-v1")!=m.requirements.end() || std::find(m.requirements.begin(),m.requirements.end(),"api-response-assertions-v1")!=m.requirements.end()) && std::any_of(m.commands.begin(),m.commands.end(),[](const Command& c) { return c.action=="open-api-requests"; })) && std::none_of(m.commands.begin(), m.commands.end(), [](const Command& c) { return c.action == "configured-container-actions" || c.action == "markdown-external-links" || c.action == "inspect-toolchain-profiles" || c.action == "edit-cmake-options" || c.action == "coverage-cobertura" || c.action == "open-api-environment" || c.action == "flash-firmware" || c.action == "web-preview" || c.action == "compile-shader" || c.action == "compare-visual-images" || c.action == "compare-visual-configured" || c.action == "sql-change-bound" || c.action == "sql-change-preview" || c.action == "sql-query" || c.action == "sql-explain" || c.action == "sql-query-bound" || c.action == "sql-explain-bound" || c.action == "release-note-builder" || c.action == "scan-secrets" || c.action == "browse-cmake" || c.action == "prepare-cmake-query" || c.action == "markdown-local-links" || c.action == "coverage-lcov" || c.action == "git-file-history" || c.action == "git-range-history" || c.action == "benchmark-comparison" || c.action == "binary-inspector" || c.action == "compare-localization" || c.action == "design-token-explorer" || c.action == "log-correlation-json" || c.action == "log-correlation" || c.action == "browse-python-environments" || c.action == "install-python-pylsp" || c.action == "manage-python-environments" || c.action == "import-junit" || c.action == "run-ctest" || c.action == "hex-editor" || c.action == "identify-usb-boards" || c.action == "inspect-svd-registers" || c.action == "review-linker-map" || c.action == "crash-dump-reader" || c.action == "run-rust-ownership" || c.action == "review-rust-ownership"; }))) m.error = "Requested permissions are not supported";
    return m;
}
Module readModule(const std::filesystem::path& dir, std::string id) {
    Module m; m.id = id; m.name = id;
    std::error_code ec; auto status = std::filesystem::symlink_status(dir / "module.json", ec);
    if (ec || status.type() != std::filesystem::file_type::regular) { m.error = "Missing or unsafe module.json"; return m; }
    auto size = std::filesystem::file_size(dir / "module.json", ec);
    if (ec || size > 65536) { m.error = "Module manifest is too large"; return m; }
    std::ifstream input(dir / "module.json", std::ios::binary);
    if (!input) { m.error = "Cannot read module manifest"; return m; }
    std::string text(static_cast<std::size_t>(size) + 1, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (input.bad() || static_cast<std::uintmax_t>(input.gcount()) != size) { m.error = "Module manifest changed or could not be read"; return m; }
    text.resize(static_cast<std::size_t>(size));
    m = parseManifest(std::move(id), text);
    if (!m.valid) return m;
    const auto marker = std::filesystem::symlink_status(dir / ".enabled", ec);
    m.enabled = !ec && marker.type() == std::filesystem::file_type::directory && m.error.empty();
    return m;
}
}
bool expandSnippet(std::string_view text, std::string_view selection, ExpandedSnippet& result, std::string& error, bool placeholders, bool multiple) {
    result = {}; error.clear();
    // Document paste normalizes CR and CRLF to LF. Normalize before calculating
    // byte ranges so placeholder offsets match the resulting editor buffer.
    std::string normalized; normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') { if (i + 1 < text.size() && text[i + 1] == '\n') ++i; normalized.push_back('\n'); }
        else normalized.push_back(text[i]);
    }
    text = normalized;
    constexpr std::string_view selectionMarker = "{{selection}}", cursorMarker = "{{cursor}}";
    constexpr std::size_t maxExpansion = 1024U * 1024U;
    std::size_t cursorOffset = 0, endOffset = 0;
    const auto append = [&](std::string_view part) {
        if (part.size() > maxExpansion - result.text.size()) return false;
        result.text.append(part); return true;
    };
    for (std::size_t i = 0; i < text.size();) {
        if (text.substr(i).starts_with(selectionMarker)) {
            if (!append(selection)) { error = "Expanded snippet exceeds 1 MiB"; return false; }
            i += selectionMarker.size();
        } else if (text.substr(i).starts_with(cursorMarker)) {
            if (result.hasCursor || (!multiple && result.hasPlaceholder)) { error = "A snippet may contain only one cursor or placeholder"; return false; }
            result.hasCursor = true; result.finalOffset = result.text.size(); result.finalFieldIndex = result.fields.size(); if (!result.hasPlaceholder) cursorOffset = result.text.size(); i += cursorMarker.size();
        } else if (placeholders && text.substr(i).starts_with("{{placeholder:")) {
            if ((!multiple && (result.hasCursor || result.hasPlaceholder)) || result.fields.size() >= 32) { error = "Too many snippet placeholders or cursor markers"; return false; }
            const auto end = text.find("}}", i + 14);
            if (end == std::string_view::npos) { error = "Unclosed snippet placeholder"; return false; }
            const auto start = result.text.size(); if (!result.hasPlaceholder) cursorOffset = start; result.hasPlaceholder = true;
            if (!append(text.substr(i + 14, end - i - 14))) { error = "Expanded snippet exceeds 1 MiB"; return false; }
            if (result.fields.empty()) endOffset = result.text.size();
            result.fields.push_back({start, result.text.size()}); i = end + 2;
        } else {
            if (!append(text.substr(i, 1))) { error = "Expanded snippet exceeds 1 MiB"; return false; }
            ++i;
        }
    }
    if (!result.hasCursor) { result.finalOffset = result.text.size(); result.finalFieldIndex = result.fields.size(); }
    if (result.hasCursor || result.hasPlaceholder) {
        auto prefix = editor::Document::fromUtf8(std::string_view(result.text).substr(0, cursorOffset));
        result.cursorLine = prefix.lineCount() - 1; result.cursorColumn = prefix.line(result.cursorLine).size();
        if (result.hasPlaceholder) {
            auto end = editor::Document::fromUtf8(std::string_view(result.text).substr(0, endOffset));
            result.endLine = end.lineCount() - 1; result.endColumn = end.line(result.endLine).size();
        }
    }
    return true;
}
bool indentSnippet(ExpandedSnippet& result, std::string_view indent, std::string& error) {
    error.clear();
    constexpr std::size_t maxExpansion=1024U*1024U;
    if (indent.size()>4096 || !std::all_of(indent.begin(),indent.end(),[](char c){return c==' ' || c=='\t';})) {error="Invalid snippet indentation";return false;}
    const auto count=static_cast<std::size_t>(std::count(result.text.begin(),result.text.end(),'\n'));
    if (result.text.size()>maxExpansion || (count && indent.size()>(maxExpansion-result.text.size())/count)) {error="Indented snippet exceeds 1 MiB";return false;}
    if (indent.empty() || !count) return true;
    const auto old=result.text;
    const auto shifted=[&](std::size_t offset) { return offset + static_cast<std::size_t>(std::count(old.begin(),old.begin()+static_cast<std::ptrdiff_t>(offset),'\n'))*indent.size(); };
    if((result.hasPlaceholder && result.fields.empty()) || result.finalOffset>old.size() || std::any_of(result.fields.begin(),result.fields.end(),[&](const auto& field){return field.start>field.end || field.end>old.size();})) {error="Invalid snippet field range";return false;}
    for(auto& field:result.fields) {field.start=shifted(field.start);field.end=shifted(field.end);}
    result.finalOffset=shifted(result.finalOffset);
    result.text.clear(); result.text.reserve(old.size()+count*indent.size());
    for(char c:old) {result.text.push_back(c);if(c=='\n') result.text.append(indent);}
    if(result.hasCursor || result.hasPlaceholder) {
        const auto start=result.hasPlaceholder ? result.fields.front().start : result.finalOffset;
        const auto p=editor::Document::fromUtf8(std::string_view(result.text).substr(0,start));
        result.cursorLine=p.lineCount()-1;result.cursorColumn=p.line(result.cursorLine).size();
        if(result.hasPlaceholder) {const auto q=editor::Document::fromUtf8(std::string_view(result.text).substr(0,result.fields.front().end));result.endLine=q.lineCount()-1;result.endColumn=q.line(result.endLine).size();}
    }
    return true;
}
Module preview(std::string_view id, std::string_view manifest) {
    return parseManifest(std::string(id), manifest);
}
bool install(const std::filesystem::path& root, std::string_view id,
             std::string_view manifest, std::string& error) {
    const auto module = preview(id, manifest);
    if (!module.valid || !module.error.empty()) { error = module.error; return false; }
    if (root.empty()) { error = "Module storage unavailable"; return false; }
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) { error = "Cannot open module storage: " + ec.message(); return false; }
    const auto dir = root / std::string(id);
    if (!std::filesystem::create_directory(dir, ec)) {
        error = ec ? "Cannot create module directory: " + ec.message() : "A module with this ID is already installed";
        return false;
    }
    std::ofstream output(dir / "module.json", std::ios::binary | std::ios::trunc);
    output.write(manifest.data(), static_cast<std::streamsize>(manifest.size()));
    output.close();
    if (!output) {
        std::error_code ignored; std::filesystem::remove_all(dir, ignored);
        error = "Could not save module manifest"; return false;
    }
    error.clear(); return true;
}
bool list(const std::filesystem::path& root, std::vector<Module>& result, std::string& error) {
    result.clear(); error.clear(); if (root.empty()) { error = "Module storage unavailable"; return false; }
    std::error_code ec; std::filesystem::create_directories(root, ec);
    if (ec) { error = "Cannot open module storage: " + ec.message(); return false; }
    std::filesystem::directory_iterator it(root, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        const auto id = it->path().filename().string();
        const auto status = it->symlink_status(ec); if (ec) break;
        if (validId(id) && status.type() == std::filesystem::file_type::directory) result.push_back(readModule(it->path(), id));
    }
    if (ec) { error = "Cannot list modules: " + ec.message(); return false; }
    std::sort(result.begin(), result.end(), [](const Module& a, const Module& b) { return a.id < b.id; }); return true;
}
bool setEnabled(const std::filesystem::path& root, std::string_view id, bool enabled, std::string& error) {
    std::filesystem::path dir; if (!safeDirectory(root, id, dir, error)) return false;
    auto module = readModule(dir, std::string(id));
    if (enabled && (!module.valid || !module.error.empty())) { error = module.error.empty() ? "Module cannot be enabled" : module.error; return false; }
    std::error_code ec;
    const auto marker = dir / ".enabled";
    if (enabled) {
        if (!std::filesystem::create_directory(marker, ec) && !ec) {
            auto status = std::filesystem::symlink_status(marker, ec);
            if (ec || status.type() != std::filesystem::file_type::directory) { error = "Unsafe module state"; return false; }
        }
    } else { (void)std::filesystem::remove(marker, ec); }
    if (ec) { error = "Could not change module state: " + ec.message(); return false; }
    error.clear(); return true;
}
bool remove(const std::filesystem::path& root, std::string_view id, std::string& error) {
    std::filesystem::path dir; if (!safeDirectory(root, id, dir, error)) return false;
    std::error_code ec; std::filesystem::remove_all(dir, ec);
    if (ec) { error = "Could not remove module: " + ec.message(); return false; }
    error.clear(); return true;
}
}
