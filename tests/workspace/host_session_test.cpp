#include "workspace/collaboration/HostSession.hpp"
#include "workspace/collaboration/ShareLink.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace lucid::workspace;
using namespace lucid::workspace::collaboration;
int main() {
    const auto dir = std::filesystem::temp_directory_path() / ("lucid-host-session-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir); { std::ofstream file(dir / "a.txt"); file << "hello world"; }
    bool created = false; std::string error, token;
    auto project = Project::openOrCreate(dir, created, error); if (!error.empty() || !newShareToken(token, error)) return 1;
    HostSession host(std::move(project), token);
    if (host.receive("a", R"({"type":"join","version":1,"token":"wrong","name":"Alice"})")[0].json.find("Authentication failed") == std::string::npos) return 2;
    if (host.receive("a", R"({"type":"open","path":"a.txt"})")[0].json.find("Authentication failed") == std::string::npos) return 3;
    if (host.receive("a", "{\"type\":\"join\",\"version\":1,\"name\":\"Alice\",\"token\":\"" + token + "\"}")[0].json.find("joined") == std::string::npos) return 4;
    if (host.receive("b", "{\"type\":\"join\",\"version\":1,\"name\":\"Bob\",\"token\":\"" + token + "\"}")[0].json.find("joined") == std::string::npos) return 5;
    if (host.receive("a", R"({"type":"open","path":"../bad"})")[0].json.find("error") == std::string::npos) return 6;
    const auto first = host.receive("a", R"({"type":"open","path":"a.txt"})");
    if (first.empty() || first[0].json.find("hello world") == std::string::npos) return 7;
    const auto second = host.receive("b", R"({"type":"open","path":"a.txt"})");
    if (second.size() < 3) return 8;
    const auto edit = host.receive("a", R"({"type":"edit","request":"1","revision":0,"start":0,"erase":0,"insert":"Hi "})");
    if (edit.size() != 4 || edit[0].json.find("\"revision\":1") == std::string::npos) return 9;
    const auto merged = host.receive("b", R"({"type":"edit","request":"2","revision":0,"start":6,"erase":5,"insert":"Lucid"})");
    if (merged.size() != 4 || merged[0].json.find("\"start\":9") == std::string::npos) return 10;
    const auto conflict = host.receive("b", R"({"type":"edit","request":"3","revision":0,"start":7,"erase":1,"insert":"X"})");
    if (conflict.size() != 2 || conflict[0].json.find("overlap") == std::string::npos || conflict[1].json.find("Hi hello Lucid") == std::string::npos) return 11;
    const auto cursors = host.receive("b", R"({"type":"cursor","revision":2,"anchor":3,"caret":3})");
    if (cursors.size() != 2 || cursors[0].json.find("Bob") == std::string::npos || cursors[0].json.find("\"caret\":3") == std::string::npos) return 12;
    if (host.disconnect("b").size() != 1) return 13;
    std::filesystem::remove_all(dir); return 0;
}
