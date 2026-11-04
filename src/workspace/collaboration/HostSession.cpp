#include "workspace/collaboration/HostSession.hpp"
#include "workspace/collaboration/ShareLink.hpp"
#include "debug/Protocol.hpp"
#include "editor/json/Json.hpp"
#include <picosha2.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <chrono>
namespace lucid::workspace::collaboration {
HostSession::~HostSession() { if (m_fileScanCancel) m_fileScanCancel->store(true); }
namespace {
using editor::JsonType; using editor::JsonValue;
std::string string(const JsonValue* v) { return v && v->type == JsonType::String ? v->stringValue : std::string{}; }
bool number(const JsonValue* v, std::uint64_t& result) {
    if (!v || v->type != JsonType::Number || !std::isfinite(v->numberValue) || v->numberValue < 0 || v->numberValue > 9007199254740991.0 || std::floor(v->numberValue) != v->numberValue) return false;
    result = static_cast<std::uint64_t>(v->numberValue); return true;
}
std::string fail(std::string_view reason, std::string_view request = {}) {
    return "{\"type\":\"error\",\"request\":" + debug::jsonString(request) + ",\"message\":" + debug::jsonString(reason) + "}";
}
}
std::string HostSession::presence(std::string_view path) const {
    const auto* doc = m_project.document(path); if (!doc) return {};
    std::string result = "{\"type\":\"presence\",\"path\":" + debug::jsonString(path) + ",\"revision\":" + std::to_string(doc->revision()) + ",\"participants\":[";
    bool first = true;
    for (const auto& [id, participant] : doc->participants()) {
        if (!first) result += ',';
        first = false;
        result += "{\"id\":" + debug::jsonString(id) + ",\"name\":" + debug::jsonString(participant.name) + ",\"anchor\":" + std::to_string(participant.cursor.anchor) + ",\"caret\":" + std::to_string(participant.cursor.caret) + "}";
    }
    return result + "]}";
}
std::string HostSession::snapshot(std::string_view path, std::uint64_t context) const {
    const auto* doc = m_project.document(path); if (!doc) return {};
    return "{\"type\":\"snapshot\",\"path\":" + debug::jsonString(path) + ",\"context\":" + std::to_string(context) + ",\"revision\":" + std::to_string(doc->revision()) + ",\"text\":" + debug::jsonString(doc->text()) + "}";
}

void HostSession::broadcast(std::string_view file, const std::string& json, std::vector<Delivery>& deliveries) const {
    if (file.empty() || json.empty()) return;
    for (const auto& [id, peer] : m_peers) if (peer.file == file) deliveries.push_back({id, json});
}
std::vector<Delivery> HostSession::receive(std::string_view peer, std::string_view json) {
    std::vector<Delivery> out; const std::string id(peer);
    if (id.empty() || id.size() > 128 || json.size() > 5U * 1024U * 1024U) return {{id, fail("Invalid session message")}};
    editor::JsonError parseError; const auto message = editor::parseJson(json, parseError);
    if (!message || message->type != JsonType::Object) return {{id, fail("Invalid session message")}};
    const auto type = string(message->find("type")); const auto request = string(message->find("request"));
    if (request.size() > 128) return {{id, fail("Invalid request ID")}};
    auto existing = m_peers.find(id);
    if (existing == m_peers.end()) {
        if (type != "join" || !sameToken(m_token, string(message->find("token")))) return {{id, fail("Authentication failed")}};
        std::uint64_t version = 0;
        if (!number(message->find("version"), version) || version != 3) return {{id, fail("Unsupported collaboration protocol")}};
        std::string error; if (!m_project.join(id, string(message->find("name")), error)) return {{id, fail(error)}};
        m_peers.emplace(id, Peer{}); return {{id, "{\"type\":\"joined\",\"id\":" + debug::jsonString(id) + "}"}};
    }
    if (type == "files") {
        std::uint64_t offset = 0;
        const auto* offsetValue = message->find("offset");
        if (offsetValue && !number(offsetValue, offset)) return {{id, fail("Invalid file list offset", request)}};
        const auto* generationValue = message->find("generation");
        if ((generationValue && generationValue->type != JsonType::String) ||
            (offset > 0 && (!generationValue || generationValue->stringValue.size() != 64)))
            return {{id, fail("File list generation required for later pages", request)}};
        const std::string expectedGeneration = generationValue ? generationValue->stringValue : std::string{};
        if (m_fileScan.valid()) return {{id, fail("A project file scan is already running", request)}};
        m_fileScanCancel = std::make_shared<std::atomic_bool>(false);
        m_fileScanPeer = id;
        try {
        m_fileScan = std::async(std::launch::async, [project = m_project.projectCopy(), id, request, offset, expectedGeneration, cancel = m_fileScanCancel]() mutable {
            try {
                bool complete = false; project.refresh(&complete, cancel.get());
                if (!complete) return std::vector<Delivery>{{id, fail("Project files could not be fully scanned", request)}};
                const auto& files = project.files();
                picosha2::hash256_one_by_one hash;
                for (const auto& path : files) {
                    if (cancel->load()) return std::vector<Delivery>{};
                    hash.process(path.begin(), path.end());
                    const char separator = '\0'; hash.process(&separator, &separator + 1);
                }
                hash.finish(); std::array<picosha2::byte_t, picosha2::k_digest_size> digest{};
                hash.get_hash_bytes(digest.begin(), digest.end());
                const auto generation = picosha2::bytes_to_hex_string(digest);
                if (!expectedGeneration.empty() && expectedGeneration != generation)
                    return std::vector<Delivery>{{id, fail("Project file list changed; start browsing again", request)}};
                const auto start = static_cast<std::size_t>(std::min<std::uint64_t>(offset, files.size()));
                std::string result = "{\"type\":\"files\",\"request\":" + debug::jsonString(request) + ",\"generation\":" + debug::jsonString(generation) + ",\"offset\":" + std::to_string(start) + ",\"paths\":[";
                std::size_t count = 0;
                for (std::size_t i = start; i < files.size(); ++i) {
                    if (cancel->load()) return std::vector<Delivery>{};
                    const auto encoded = debug::jsonString(files[i]);
                    if (count == 5000 || result.size() + encoded.size() + 128 > 1024U * 1024U) break;
                    if (count++) result += ',';
                    result += encoded;
                }
                result += "],\"nextOffset\":" + std::to_string(start + count) + ",\"truncated\":" + std::string(start + count < files.size() ? "true" : "false") + "}";
                return std::vector<Delivery>{{id, std::move(result)}};
            } catch (const std::exception&) { return std::vector<Delivery>{{id, fail("Project file scan failed", request)}}; }
        });
        } catch (const std::exception&) { m_fileScanCancel.reset(); m_fileScanPeer.clear(); return {{id, fail("Could not start project file scan", request)}}; }
        return {};
    }
    if (type == "open") {
        std::string path, error; const std::string old = existing->second.file;
        if (existing->second.context == 9007199254740991ULL) return {{id, fail("Too many file openings; reconnect to continue", request)}};
        if (!m_project.open(id, string(message->find("path")), path, error)) return {{id, fail(error, request)}};
        existing->second.file = path;
        ++existing->second.context;
        out.push_back({id, snapshot(path, existing->second.context)}); if (old != path) broadcast(old, presence(old), out); broadcast(path, presence(path), out); return out;
    }
    if (existing->second.file.empty()) return {{id, fail("Open a file first", request)}};
    if (type == "edit" || type == "cursor" || type == "save") {
        std::uint64_t context = 0;
        if (string(message->find("path")) != existing->second.file ||
            !number(message->find("context"), context) || context != existing->second.context)
            return {{id, fail("File changed; reopen the intended file before continuing", request)}};
    }
    if (type == "cursor") {
        std::uint64_t revision = 0, anchor = 0, caret = 0; const auto* doc = m_project.document(existing->second.file);
        if (!doc || !number(message->find("revision"), revision) || revision != doc->revision() || !number(message->find("anchor"), anchor) || !number(message->find("caret"), caret)) return {{id, fail("Cursor is out of date", request)}};
        std::string error; if (!m_project.moveCursor(id, {static_cast<std::size_t>(anchor), static_cast<std::size_t>(caret)}, error)) return {{id, fail(error, request)}};
        broadcast(existing->second.file, presence(existing->second.file), out); return out;
    }
    if (type == "edit") {
        std::uint64_t revision = 0, start = 0, erase = 0; const auto* insert = message->find("insert");
        if (!number(message->find("revision"), revision) || !number(message->find("start"), start) || !number(message->find("erase"), erase) || !insert || insert->type != JsonType::String) return {{id, fail("Invalid edit", request)}};
        Edit applied; std::string error;
        if (!m_project.edit(id, {revision, static_cast<std::size_t>(start), static_cast<std::size_t>(erase), insert->stringValue}, applied, error)) {
            out.push_back({id, fail(error, request)}); out.push_back({id, snapshot(existing->second.file, existing->second.context)}); return out;
        }
        const auto* doc = m_project.document(existing->second.file);
        const std::string event = "{\"type\":\"edit\",\"path\":" + debug::jsonString(existing->second.file) + ",\"revision\":" + std::to_string(doc->revision()) +
            ",\"actor\":" + debug::jsonString(id) + ",\"request\":" + debug::jsonString(request) + ",\"start\":" + std::to_string(applied.start) +
            ",\"erase\":" + std::to_string(applied.eraseBytes) + ",\"insert\":" + debug::jsonString(applied.insert) + "}";
        broadcast(existing->second.file, event, out); broadcast(existing->second.file, presence(existing->second.file), out); return out;
    }
    if (type == "save") {
        std::string error; if (!m_project.save(existing->second.file, error)) return {{id, fail(error, request)}};
        return {{id, "{\"type\":\"saved\",\"request\":" + debug::jsonString(request) + "}"}};
    }
    return {{id, fail("Unknown session message", request)}};
}
std::vector<Delivery> HostSession::poll() {
    if (!m_fileScan.valid() || m_fileScan.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
    auto result = m_fileScan.get();
    const bool cancelled = m_fileScanCancel && m_fileScanCancel->load();
    m_fileScanCancel.reset(); m_fileScanPeer.clear();
    if (cancelled || (!result.empty() && !m_peers.contains(result.front().peer))) return {};
    return result;
}
std::vector<Delivery> HostSession::disconnect(std::string_view peer) {
    if (m_fileScanCancel && m_fileScanPeer == peer) m_fileScanCancel->store(true);
    std::vector<Delivery> out; const auto found = m_peers.find(std::string(peer)); if (found == m_peers.end()) return out;
    const std::string file = found->second.file; m_project.leave(peer); m_peers.erase(found); broadcast(file, presence(file), out); return out;
}
}
