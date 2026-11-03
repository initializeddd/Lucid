#include "platform/security/Credentials.hpp"
#if defined(__APPLE__)
#include <Security/Security.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <wincred.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <cerrno>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
namespace lucid::credentials {
#if defined(__APPLE__)
namespace {
constexpr auto service = "dev.lucid.editor.openrouter";
constexpr auto account = "api-key";
}
bool save(std::string_view key, std::string& error) {
    auto status = SecKeychainAddGenericPassword(nullptr, static_cast<UInt32>(sizeof("dev.lucid.editor.openrouter")-1), service, static_cast<UInt32>(sizeof("api-key")-1), account, static_cast<UInt32>(key.size()), key.data(), nullptr);
    if (status == errSecDuplicateItem) {
        SecKeychainItemRef item = nullptr;
        status = SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(sizeof("dev.lucid.editor.openrouter")-1), service, static_cast<UInt32>(sizeof("api-key")-1), account, nullptr, nullptr, &item);
        if (status == errSecSuccess) { status = SecKeychainItemModifyAttributesAndData(item, nullptr, static_cast<UInt32>(key.size()), key.data()); CFRelease(item); }
    }
    if (status != errSecSuccess) error = "Could not save key in macOS Keychain (" + std::to_string(status) + ")";
    return status == errSecSuccess;
}
bool load(std::string& key, std::string& error) {
    UInt32 length = 0; void* data = nullptr;
    const auto status = SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(sizeof("dev.lucid.editor.openrouter")-1), service, static_cast<UInt32>(sizeof("api-key")-1), account, &length, &data, nullptr);
    if (status != errSecSuccess) { error = "OpenRouter key unavailable in Keychain; add it in AI settings"; return false; }
    key.assign(static_cast<const char*>(data), length); SecKeychainItemFreeContent(nullptr, data); return true;
}
bool remove(std::string& error) {
    SecKeychainItemRef item = nullptr;
    auto status = SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(sizeof("dev.lucid.editor.openrouter")-1), service, static_cast<UInt32>(sizeof("api-key")-1), account, nullptr, nullptr, &item);
    if (status == errSecItemNotFound) return true;
    if (status == errSecSuccess) { status = SecKeychainItemDelete(item); CFRelease(item); }
    if (status != errSecSuccess) error = "Could not remove key from Keychain";
    return status == errSecSuccess;
}
#elif defined(_WIN32)
bool save(std::string_view key, std::string& error) {
    CREDENTIALW cred{}; cred.Type = CRED_TYPE_GENERIC; cred.TargetName = const_cast<wchar_t*>(L"Lucid/OpenRouter"); cred.Persist = CRED_PERSIST_LOCAL_MACHINE; cred.CredentialBlobSize = static_cast<DWORD>(key.size()); cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(key.data()));
    if (!CredWriteW(&cred, 0)) { error = "Could not save key in Windows Credential Manager"; return false; } return true;
}
bool load(std::string& key, std::string& error) {
    PCREDENTIALW cred = nullptr; if (!CredReadW(L"Lucid/OpenRouter", CRED_TYPE_GENERIC, 0, &cred)) { error = "Add an OpenRouter key in AI settings"; return false; }
    key.assign(reinterpret_cast<char*>(cred->CredentialBlob), cred->CredentialBlobSize); CredFree(cred); return true;
}
bool remove(std::string& error) { if (CredDeleteW(L"Lucid/OpenRouter", CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND) return true; error = "Could not remove key from Windows Credential Manager"; return false; }
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
namespace {
bool runSecretTool(const char* action, std::string_view input, std::string& output, std::string& error) {
    int sockets[2], outputPipe[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) { error = "Could not access the system keyring"; return false; }
    if (pipe(outputPipe) != 0) { close(sockets[0]); close(sockets[1]); error = "Could not access the system keyring"; return false; }
#if defined(SO_NOSIGPIPE)
    int noSigPipe = 1;
    if (setsockopt(sockets[0], SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe)) != 0) { close(sockets[0]); close(sockets[1]); close(outputPipe[0]); close(outputPipe[1]); error = "Could not configure the system keyring connection"; return false; }
#endif
    posix_spawn_file_actions_t actions; posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, outputPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, sockets[0]); posix_spawn_file_actions_addclose(&actions, sockets[1]);
    posix_spawn_file_actions_addclose(&actions, outputPipe[0]); posix_spawn_file_actions_addclose(&actions, outputPipe[1]);
    const bool store = std::string_view(action) == "store";
    const char* args[] = {"secret-tool", action, store ? "--label=Lucid OpenRouter" : "service", store ? "service" : "dev.lucid.editor.openrouter", store ? "dev.lucid.editor.openrouter" : "account", store ? "account" : "api-key", store ? "api-key" : nullptr, nullptr};
    pid_t pid{}; const int result = posix_spawnp(&pid, "secret-tool", &actions, nullptr, const_cast<char* const*>(args), environ);
    posix_spawn_file_actions_destroy(&actions); close(sockets[1]); close(outputPipe[1]);
    if (result != 0) { close(sockets[0]); close(outputPipe[0]); error = "Install secret-tool and unlock your system keyring to use OpenRouter"; return false; }
    std::size_t sent = 0; while (sent < input.size()) { const auto n = send(sockets[0], input.data() + sent, input.size() - sent,
#if defined(MSG_NOSIGNAL)
        MSG_NOSIGNAL
#else
        0
#endif
        ); if (n <= 0) break; sent += static_cast<std::size_t>(n); }
    shutdown(sockets[0], SHUT_WR); close(sockets[0]);
    char buffer[256]; ssize_t count; bool tooLong = false;
    while ((count = read(outputPipe[0], buffer, sizeof(buffer))) > 0) { if (output.size() + static_cast<std::size_t>(count) <= 1025) output.append(buffer, static_cast<std::size_t>(count)); else tooLong = true; }
    close(outputPipe[0]); int status = 0; while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || sent != input.size() || tooLong) { error = "Could not access the system keyring; install secret-tool and unlock the keyring"; return false; }
    if (!output.empty() && output.back() == '\n') output.pop_back(); return true;
}
}
bool save(std::string_view key, std::string& error) { std::string output; return runSecretTool("store", key, output, error); }
bool load(std::string& key, std::string& error) { return runSecretTool("lookup", {}, key, error); }
bool remove(std::string& error) { std::string output; return runSecretTool("clear", {}, output, error); }
#else
bool save(std::string_view, std::string& error) { error = "Secure credential storage is not yet available on this platform"; return false; }
bool load(std::string&, std::string& error) { error = "Secure credential storage is not yet available on this platform"; return false; }
bool remove(std::string& error) { error = "Secure credential storage is not yet available on this platform"; return false; }
#endif
}
