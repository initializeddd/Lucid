#include "workspace/collaboration/HostSession.hpp"
#include "workspace/collaboration/ShareLink.hpp"
#include "editor/json/Json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
using namespace lucid::workspace;
using namespace lucid::workspace::collaboration;
int main() {
    const auto dir = std::filesystem::temp_directory_path() / ("lucid-host-session-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir); { std::ofstream file(dir / "a.txt"); file << "hello world"; }
    { std::ofstream file(dir / "z.txt"); file << "other"; }
    bool created = false; std::string error, token;
    auto project = Project::openOrCreate(dir, created, error); if (!error.empty() || !newShareToken(token, error)) return 1;
    HostSession host(std::move(project), token);
    if (host.receive("a", R"({"type":"join","version":3,"token":"wrong","name":"Alice"})")[0].json.find("Authentication failed") == std::string::npos) return 2;
    if (host.receive("a", R"({"type":"open","path":"a.txt"})")[0].json.find("Authentication failed") == std::string::npos) return 3;
    if (host.receive("a", R"({"type":"files"})")[0].json.find("Authentication failed") == std::string::npos) return 14;
    if (host.receive("a", "{\"type\":\"join\",\"version\":3,\"name\":\"Alice\",\"token\":\"" + token + "\"}")[0].json.find("joined") == std::string::npos) return 4;
    if (!host.receive("a", R"({"type":"files","request":"browse"})").empty()) return 15;
    const auto whileScanning = host.receive("a", R"({"type":"open","path":"a.txt"})");
    if (whileScanning.empty() || whileScanning.front().json.find("snapshot") == std::string::npos) return 16;
    std::vector<Delivery> files;
    for (int i = 0; i < 500 && files.empty(); ++i) {
        files = host.poll();
        if (files.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (files.size() != 1 || files[0].json.find("\"type\":\"files\"") == std::string::npos ||
        files[0].json.find("a.txt") == std::string::npos || files[0].json.find("browse") == std::string::npos) return 15;
    lucid::editor::JsonError jsonError;
    const auto listing = lucid::editor::parseJson(files[0].json, jsonError);
    const auto* generation = listing ? listing->find("generation") : nullptr;
    if (!generation || generation->stringValue.size() != 64) return 20;
    const auto generationText = generation->stringValue;
    if (host.receive("a", R"({"type":"files","offset":-1})").front().json.find("Invalid file list offset") == std::string::npos ||
        host.receive("a", R"({"type":"files","offset":1})").front().json.find("generation required") == std::string::npos) return 21;
    if (!host.receive("a", "{\"type\":\"files\",\"offset\":1,\"generation\":\"" + generationText + "\"}").empty()) return 22;
    files.clear();
    for (int i = 0; i < 500 && files.empty(); ++i) { files = host.poll(); if (files.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    if (files.empty() || files[0].json.find("z.txt") == std::string::npos || files[0].json.find("a.txt") != std::string::npos ||
        files[0].json.find("\"offset\":1") == std::string::npos) return 23;
    { std::ofstream file(dir / "new.txt"); file << "new"; }
    if (!host.receive("a", "{\"type\":\"files\",\"offset\":1,\"generation\":\"" + generationText + "\"}").empty()) return 24;
    files.clear();
    for (int i = 0; i < 500 && files.empty(); ++i) { files = host.poll(); if (files.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    if (files.empty() || files[0].json.find("start browsing again") == std::string::npos) return 25;
    if (host.receive("b", "{\"type\":\"join\",\"version\":3,\"name\":\"Bob\",\"token\":\"" + token + "\"}")[0].json.find("joined") == std::string::npos) return 5;
    if (host.receive("a", R"({"type":"open","path":"../bad"})")[0].json.find("error") == std::string::npos) return 6;
    const auto first = host.receive("a", R"({"type":"open","path":"a.txt"})");
    if (first.empty() || first[0].json.find("hello world") == std::string::npos) return 7;
    const auto second = host.receive("b", R"({"type":"open","path":"a.txt"})");
    if (second.size() < 3) return 8;
    const auto edit = host.receive("a", R"({"type":"edit","path":"a.txt","context":2,"request":"1","revision":0,"start":0,"erase":0,"insert":"Hi "})");
    if (edit.size() != 4 || edit[0].json.find("\"revision\":1") == std::string::npos) return 9;
    const auto merged = host.receive("b", R"({"type":"edit","path":"a.txt","context":1,"request":"2","revision":0,"start":6,"erase":5,"insert":"Lucid"})");
    if (merged.size() != 4 || merged[0].json.find("\"start\":9") == std::string::npos) return 10;
    const auto conflict = host.receive("b", R"({"type":"edit","path":"a.txt","context":1,"request":"3","revision":0,"start":7,"erase":1,"insert":"X"})");
    if (conflict.size() != 2 || conflict[0].json.find("overlap") == std::string::npos || conflict[1].json.find("Hi hello Lucid") == std::string::npos) return 11;
    const auto cursors = host.receive("b", R"({"type":"cursor","path":"a.txt","context":1,"revision":2,"anchor":3,"caret":3})");
    if (cursors.size() != 2 || cursors[0].json.find("Bob") == std::string::npos || cursors[0].json.find("\"caret\":3") == std::string::npos) return 12;
    if (host.disconnect("b").size() != 1) return 13;
    if (host.receive("a", R"({"type":"open","path":"z.txt"})").front().json.find("other") == std::string::npos ||
        host.receive("a", R"({"type":"edit","path":"a.txt","context":1,"revision":0,"start":0,"erase":0,"insert":"wrong"})").front().json.find("File changed") == std::string::npos ||
        host.receive("a", R"({"type":"cursor","path":"a.txt","context":1,"revision":0,"anchor":0,"caret":0})").front().json.find("File changed") == std::string::npos ||
        host.receive("a", R"({"type":"save","path":"a.txt"})").front().json.find("File changed") == std::string::npos ||
        host.receive("a", R"({"type":"edit","revision":0,"start":0,"erase":0,"insert":"wrong"})").front().json.find("File changed") == std::string::npos ||
        host.receive("a", R"({"type":"open","path":"z.txt"})").front().json.find("other") == std::string::npos) return 26;
    const auto stale = host.receive("a", R"({"type":"edit","path":"z.txt","context":3,"revision":0,"start":0,"erase":0,"insert":"wrong"})");
    if (stale.empty() || stale.front().json.find("File changed") == std::string::npos ||
        host.receive("a", R"({"type":"edit","path":"z.txt","context":4,"revision":0,"start":0,"erase":0,"insert":"right "})").front().json.find("\"type\":\"edit\"") == std::string::npos) return 27;
    if (host.receive("c", "{\"type\":\"join\",\"version\":3,\"name\":\"Carol\",\"token\":\"" + token + "\"}")[0].json.find("joined") == std::string::npos ||
        !host.receive("c", R"({"type":"files","request":"old"})").empty()) return 17;
    (void)host.disconnect("c");
    if (host.receive("c", "{\"type\":\"join\",\"version\":3,\"name\":\"Carol\",\"token\":\"" + token + "\"}")[0].json.find("joined") == std::string::npos) return 18;
    for (int i = 0; i < 100; ++i) {
        if (!host.poll().empty()) return 19;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::filesystem::remove_all(dir); return 0;
}
