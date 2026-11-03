#include "workspace/git/GitReview.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lucid::workspace;
int main() {
#if defined(_WIN32)
    return 0;
#else
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / ("lucid-remote-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base);
    const auto command = [&](const std::string& args) { return std::system(("cd '" + base.string() + "' && " + args + " >/dev/null 2>&1").c_str()); };
    if (command("git init -q --bare remote.git") || command("git clone -q remote.git one") ||
        command("git -C one config user.name Test") || command("git -C one config user.email test@example.com") || command("git -C one config commit.gpgsign false")) return 1;
    { std::ofstream file(base / "one/file.txt"); file << "initial\n"; }
    if (command("git -C one add .") || command("git -C one commit -qm initial") || command("git -C one push -qu origin HEAD")) return 2;
    GitUpstream upstream; std::string error, output;
    std::vector<GitHistoryEntry> history;
    if (!gitHistory(base / "one", history, error) || history.empty() || history[0].subject != "initial" || history[0].author != "Test") return 18;
    if (!gitUpstream(base / "one", upstream, error) || upstream.remote != "origin") return 3;
    if (!gitRemote(base / "one", upstream, GitRemoteAction::Fetch, output, error)) return 4;
    if (command("git clone -q remote.git two") || command("git -C two config user.name Test") || command("git -C two config user.email test@example.com") || command("git -C two config commit.gpgsign false")) return 5;
    { std::ofstream file(base / "two/file.txt", std::ios::app); file << "remote\n"; }
    if (command("git -C two add .") || command("git -C two commit -qm remote") || command("git -C two push -q")) return 6;
    { std::ofstream file(base / "one/local.txt"); file << "unsaved\n"; }
    if (gitRemote(base / "one", upstream, GitRemoteAction::Pull, output, error) || error.empty()) return 7;
    fs::remove(base / "one/local.txt");
    if (!gitRemote(base / "one", upstream, GitRemoteAction::Pull, output, error)) return 8;
    if (gitRemote(base / "one", upstream, GitRemoteAction::Push, output, error) || error.empty()) return 9;
    if (!gitUpstream(base / "one", upstream, error)) return 10;
    { std::ofstream file(base / "one/file.txt", std::ios::app); file << "local\n"; }
    if (command("git -C one add .") || command("git -C one commit -qm local") || !gitUpstream(base / "one", upstream, error)) return 11;
    if (!gitRemote(base / "one", upstream, GitRemoteAction::Push, output, error)) return 12;
    if (command("git -C two fetch -q") || command("git -C two merge -q --ff-only origin/HEAD")) return 13;
    { std::ofstream file(base / "two/remote.txt"); file << "remote\n"; }
    if (command("git -C two add .") || command("git -C two commit -qm remote2") || command("git -C two push -q")) return 14;
    { std::ofstream file(base / "one/another.txt"); file << "local\n"; }
    if (command("git -C one add .") || command("git -C one commit -qm local2") || !gitUpstream(base / "one", upstream, error)) return 15;
    if (gitRemote(base / "one", upstream, GitRemoteAction::Push, output, error) || error.empty()) return 16;
    if (gitRemote(base / "one", upstream, GitRemoteAction::Pull, output, error) || error.empty()) return 17;
    fs::remove_all(base); return 0;
#endif
}
