#include "workspace/collaboration/HostSession.hpp"
#include "workspace/collaboration/ShareLink.hpp"
#include "debug/Protocol.hpp"
#include "editor/json/Json.hpp"
#include <cmath>
namespace lucid::workspace::collaboration {
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
std::string HostSession::snapshot(std::string_view path) const {
    const auto* doc = m_project.document(path); if (!doc) return {};
    return "{\"type\":\"snapshot\",\"path\":" + debug::jsonString(path) + ",\"revision\":" + std::to_string(doc->revision()) + ",\"text\":" + debug::jsonString(doc->text()) + "}";
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
        if (!number(message->find("version"), version) || version != 1) return {{id, fail("Unsupported collaboration protocol")}};
        std::string error; if (!m_project.join(id, string(message->find("name")), error)) return {{id, fail(error)}};
        m_peers.emplace(id, Peer{}); return {{id, "{\"type\":\"joined\",\"id\":" + debug::jsonString(id) + "}"}};
    }
    if (type == "open") {
        std::string path, error; const std::string old = existing->second.file;
        if (!m_project.open(id, string(message->find("path")), path, error)) return {{id, fail(error, request)}};
        existing->second.file = path;
        out.push_back({id, snapshot(path)}); if (old != path) broadcast(old, presence(old), out); broadcast(path, presence(path), out); return out;
    }
    if (existing->second.file.empty()) return {{id, fail("Open a file first", request)}};
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
            out.push_back({id, fail(error, request)}); out.push_back({id, snapshot(existing->second.file)}); return out;
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
std::vector<Delivery> HostSession::disconnect(std::string_view peer) {
    std::vector<Delivery> out; const auto found = m_peers.find(std::string(peer)); if (found == m_peers.end()) return out;
    const std::string file = found->second.file; m_project.leave(peer); m_peers.erase(found); broadcast(file, presence(file), out); return out;
}
}
