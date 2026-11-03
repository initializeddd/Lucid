#include "ai/ModelDownload.hpp"

#include <curl/curl.h>
#include <picosha2.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <system_error>

namespace lucid::ai {
namespace {
constexpr auto kName = "qwen2.5-coder-0.5b-instruct-q4_k_m.gguf";
constexpr auto kHash = "1d9614638d18024d0fbb36575a15f1302a3adf044df10345688ec4f6e1c4ff32";
constexpr auto kUrl = "https://huggingface.co/Qwen/Qwen2.5-Coder-0.5B-Instruct-GGUF/resolve/ebb2015119c907b064c512bf053e945850b5875f/qwen2.5-coder-0.5b-instruct-q4_k_m.gguf";
constexpr std::uintmax_t kSize = 491400064;

bool valid(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::file_size(path, ec) != kSize || ec) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    return picosha2::hash256_hex_string(std::istreambuf_iterator<char>(file),
                                        std::istreambuf_iterator<char>()) == kHash && !file.bad();
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
    if (valid(model)) { if (cancel && cancel->load()) { error = "Model download cancelled"; return false; } if (progress) *progress = 100; return true; }
    auto temporary = model;
    temporary += ".download";
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
    if (result != CURLE_OK || !stream || !valid(temporary)) {
        std::filesystem::remove(temporary, ec);
        error = cancel && cancel->load() ? "Model download cancelled" : result == CURLE_OK ? "Model download failed integrity verification" :
            std::string("Model download failed: ") + curl_easy_strerror(result);
        return false;
    }
    std::filesystem::remove(model, ec);
    ec.clear();
    std::filesystem::rename(temporary, model, ec);
    if (ec) { error = "Could not install downloaded model: " + ec.message(); return false; }
    return true;
}
} // namespace lucid::ai
