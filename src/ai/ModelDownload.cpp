#include "ai/ModelDownload.hpp"

#include <curl/curl.h>
#include <picosha2.h>

#include <cstdio>
#include <array>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <random>
#include <system_error>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace lucid::ai {
namespace {
constexpr auto kName = "qwen2.5-coder-0.5b-instruct-q4_k_m.gguf";
constexpr auto kHash = "1d9614638d18024d0fbb36575a15f1302a3adf044df10345688ec4f6e1c4ff32";
constexpr auto kUrl = "https://huggingface.co/Qwen/Qwen2.5-Coder-0.5B-Instruct-GGUF/resolve/ebb2015119c907b064c512bf053e945850b5875f/qwen2.5-coder-0.5b-instruct-q4_k_m.gguf";
constexpr std::uintmax_t kSize = 491400064;

bool valid(const std::filesystem::path& path, const std::atomic_bool* cancel) {
    std::error_code ec;
    if (std::filesystem::file_size(path, ec) != kSize || ec) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    picosha2::hash256_one_by_one hash;
    std::array<char, 64U * 1024U> buffer{};
    while (file) {
        if (cancel && cancel->load()) return false;
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        hash.process(buffer.begin(), buffer.begin() + file.gcount());
    }
    if (file.bad() || !file.eof() || (cancel && cancel->load())) return false;
    hash.finish();
    std::array<picosha2::byte_t, picosha2::k_digest_size> digest{};
    hash.get_hash_bytes(digest.begin(), digest.end());
    return picosha2::bytes_to_hex_string(digest) == kHash;
}
struct Download { std::ofstream* stream; std::uintmax_t written = 0; };
size_t writeData(char* data, size_t size, size_t count, void* context) {
    auto& download = *static_cast<Download*>(context);
    if (size != 0 && count > (kSize - download.written) / size) return 0;
    const size_t bytes = size * count;
    download.stream->write(data, static_cast<std::streamsize>(bytes));
    download.written += bytes;
    return *download.stream ? bytes : 0;
}
} // namespace

bool ensureModel(const std::filesystem::path& directory, std::filesystem::path& model,
                 std::string& error, const std::atomic_bool* cancel, std::atomic<int>* progress) {
    static std::mutex mutex;
    const std::lock_guard lock(mutex);
    error.clear();
    if (cancel && cancel->load()) { error = "Model download cancelled"; return false; }
    if (directory.empty()) { error = "Model storage directory unavailable"; return false; }
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) { error = "Could not create model directory: " + ec.message(); return false; }
    model = directory / kName;
    if (progress) *progress = kModelVerifying;
    if (valid(model, cancel)) { if (cancel && cancel->load()) { error = "Model download cancelled"; return false; } if (progress) *progress = 100; return true; }
    if (cancel && cancel->load()) { error = "Model download cancelled"; return false; }
    if (progress) *progress = 0;
    std::filesystem::path staging;
    std::random_device random;
    for (int attempt = 0; attempt < 16; ++attempt) {
        const auto candidate = directory / (".lucid-model-download-" + std::to_string(random()) + "-" + std::to_string(random()));
        if (std::filesystem::create_directory(candidate, ec)) { staging = candidate; break; }
        if (ec) { error = "Could not create model download directory: " + ec.message(); return false; }
    }
    if (staging.empty()) { error = "Could not create a unique model download directory"; return false; }
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } cleanup{staging};
    const auto temporary = staging / "model.download";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) { error = "Could not create model download"; return false; }
    static std::once_flag init;
    std::call_once(init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    using Handle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    Handle curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) { stream.close(); std::filesystem::remove(temporary, ec); error = "Could not initialize download"; return false; }
    curl_easy_setopt(curl.get(), CURLOPT_URL, kUrl);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
    struct Progress { const std::atomic_bool* cancel; std::atomic<int>* value; } progressState{cancel, progress};
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION,
        +[](void* data, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) -> int {
            auto* state = static_cast<Progress*>(data);
            if (state->value && total > 0) *state->value = static_cast<int>(now * 100 / total);
            return state->cancel && state->cancel->load() ? 1 : 0;
        });
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &progressState);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(kSize));
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeData);
    Download download{&stream};
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &download);
    const CURLcode result = curl_easy_perform(curl.get());
    stream.close();
    if (result == CURLE_OK && stream && progress) *progress = kModelVerifying;
    if (result != CURLE_OK || !stream || !valid(temporary, cancel)) {
        std::filesystem::remove(temporary, ec);
        error = cancel && cancel->load() ? "Model download cancelled" : result == CURLE_OK ? "Model download failed integrity verification" :
            std::string("Model download failed: ") + curl_easy_strerror(result);
        return false;
    }
    if (cancel && cancel->load()) { error = "Model download cancelled"; return false; }
    if (progress) *progress = kModelInstalling;
#if defined(_WIN32)
    if (!MoveFileExW(temporary.c_str(), model.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Could not install downloaded model (Windows error " + std::to_string(GetLastError()) + ")"; return false;
    }
#else
    std::filesystem::rename(temporary, model, ec);
    if (ec) { error = "Could not install downloaded model: " + ec.message(); return false; }
#endif
    return true;
}
} // namespace lucid::ai
