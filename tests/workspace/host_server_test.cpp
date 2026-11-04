#include "workspace/collaboration/HostServer.hpp"
#include "workspace/collaboration/ShareLink.hpp"
#include "workspace/collaboration/Connection.hpp"
#include "debug/Protocol.hpp"
#include <chrono>
#include <filesystem>
#include <deque>
#include <fstream>
#include <string>
#include <thread>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
using namespace lucid::workspace;
using namespace lucid::workspace::collaboration;
#if !defined(_WIN32)
struct Client {
    int fd = -1; lucid::debug::FrameDecoder decoder; std::deque<std::string> pending;
    ~Client() { if (fd >= 0) close(fd); }
    bool connectTo(std::uint16_t port) {
        fd = socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return false;
        timeval timeout{3, 0}; (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        sockaddr_in endpoint{}; endpoint.sin_family = AF_INET; endpoint.sin_port = htons(port); endpoint.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        return connect(fd, reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) == 0;
    }
    bool sendMessage(const std::string& json) {
        const auto bytes = lucid::debug::frame(json); std::size_t offset = 0;
        while (offset < bytes.size()) { const auto n = send(fd, bytes.data() + offset, bytes.size() - offset, 0); if (n <= 0) return false; offset += static_cast<std::size_t>(n); }
        return true;
    }
    bool receiveContaining(std::string_view expected) {
        for (int i = 0; i < 10; ++i) {
            while (!pending.empty()) { auto item = std::move(pending.front()); pending.pop_front(); if (item.find(expected) != std::string::npos) return true; }
            char buffer[8192]; const auto n = recv(fd, buffer, sizeof(buffer), 0); if (n <= 0) return false;
            std::vector<std::string> messages; std::string error;
            if (!decoder.append(std::string_view(buffer, static_cast<std::size_t>(n)), messages, error)) return false;
            for (auto& item : messages) pending.push_back(std::move(item));
        }
        return false;
    }
};
#endif
int main() {
#if defined(_WIN32)
    return 0;
#else
    const auto dir = std::filesystem::temp_directory_path() / ("lucid-host-server-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir); { std::ofstream file(dir / "a.txt"); file << "hello"; }
    bool created = false; std::string error, token; auto project = Project::openOrCreate(dir, created, error);
    if (!error.empty() || !newShareToken(token, error)) return 1;
    HostServer server;
    if (server.start(project, "0.0.0.0", 0, token, error)) return 2;
    if (!server.start(project, "127.0.0.1", 0, token, error) || server.port() == 0) return 3;
    Client alice, bob;
    if (!alice.connectTo(server.port()) || !bob.connectTo(server.port())) return 4;
    if (!alice.sendMessage("{\"type\":\"join\",\"version\":3,\"name\":\"Alice\",\"token\":\"" + token + "\"}") || !alice.receiveContaining("joined")) return 5;
    if (!bob.sendMessage("{\"type\":\"join\",\"version\":3,\"name\":\"Bob\",\"token\":\"" + token + "\"}") || !bob.receiveContaining("joined")) return 6;
    if (!alice.sendMessage(R"({"type":"files","request":"browse"})") || !alice.receiveContaining("\"type\":\"files\"")) return 21;
    if (!alice.sendMessage(R"({"type":"open","path":"a.txt"})") || !alice.receiveContaining("snapshot")) return 7;
    if (!bob.sendMessage(R"({"type":"open","path":"a.txt"})") || !bob.receiveContaining("snapshot")) return 8;
    if (!alice.sendMessage(R"({"type":"edit","path":"a.txt","context":1,"request":"1","revision":0,"start":0,"erase":1,"insert":"H"})")) return 9;
    if (!alice.receiveContaining("\"type\":\"edit\"") || !bob.receiveContaining("\"type\":\"edit\"")) return 10;
    if (!bob.sendMessage(R"({"type":"edit","path":"a.txt","context":1,"request":"conflict","revision":0,"start":0,"erase":1,"insert":"X"})") ||
        !bob.receiveContaining("overlap") || !bob.receiveContaining("Hello")) return 11;
    if (!bob.sendMessage(R"({"type":"edit","path":"a.txt","context":1,"request":"2","revision":0,"start":5,"erase":0,"insert":"!"})") ||
        !alice.receiveContaining("\"revision\":2") || !bob.receiveContaining("\"revision\":2")) return 12;
    if (!bob.sendMessage(R"({"type":"cursor","path":"a.txt","context":1,"revision":2,"anchor":6,"caret":6})") || !alice.receiveContaining("\"caret\":6")) return 13;
    if (!bob.sendMessage(R"({"type":"save","path":"a.txt","context":1,"request":"save"})") || !bob.receiveContaining("saved")) return 14;
    { std::ifstream file(dir / "a.txt"); std::string text((std::istreambuf_iterator<char>(file)), {}); if (text != "Hello!") return 15; }
    Connection client; const ShareLink link{"127.0.0.1", server.port(), token};
    if (!client.connect(formatShareLink(link), "Carol", error)) return 16;
    bool joined = false; const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && !joined) {
        std::vector<std::string> messages; if (!client.poll(messages, error)) return 17;
        for (const auto& message : messages) if (message.find("joined") != std::string::npos) joined = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!joined || !client.send(R"({"type":"open","path":"a.txt"})", error)) return 18;
    bool snapshot = false;
    while (std::chrono::steady_clock::now() < deadline && !snapshot) {
        std::vector<std::string> messages; if (!client.poll(messages, error)) return 19;
        for (const auto& message : messages) if (message.find("Hello!") != std::string::npos) snapshot = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!snapshot) return 20;
    Client departing;
    if (!departing.connectTo(server.port()) ||
        !departing.sendMessage("{\"type\":\"join\",\"version\":3,\"name\":\"Dana\",\"token\":\"" + token + "\"}") ||
        !departing.receiveContaining("joined") ||
        !departing.sendMessage(R"({"type":"open","path":"a.txt"})") || !departing.receiveContaining("snapshot")) return 22;
    if (!departing.sendMessage(R"({"type":"edit","path":"a.txt","context":1,"request":"last","revision":2,"start":6,"erase":0,"insert":"?"})") ||
        shutdown(departing.fd, SHUT_WR) != 0 || !bob.receiveContaining("\"request\":\"last\"")) return 23;
    Connection observer;
    if (!observer.connect(formatShareLink(link), "Observer", error)) return 24;
    bool observerJoined = false;
    const auto observerDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < observerDeadline && !observerJoined) {
        std::vector<std::string> messages;
        if (!observer.poll(messages, error)) return 25;
        for (const auto& message : messages) if (message.find("joined") != std::string::npos) observerJoined = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!observerJoined) return 26;
    client.close(); server.stop();
    std::vector<std::string> oldMessages{"previously delivered"};
    bool disconnected = false;
    const auto disconnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < disconnectDeadline) {
        if (!observer.poll(oldMessages, error)) { disconnected = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!disconnected || error != "Session connection closed") return 27;
    std::filesystem::remove_all(dir); return 0;
#endif
}
