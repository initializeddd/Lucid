#include "ai/LocalModel.hpp"

#include <llama.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

namespace lucid::ai {

bool generate(const std::filesystem::path& modelPath, std::string_view request,
              std::string& output, std::string& error, const std::atomic_bool* cancel) {
    output.clear();
    error.clear();
    if (cancel && cancel->load()) { error = "AI request cancelled"; return false; }
    static std::once_flag initialized;
    std::call_once(initialized, [] { llama_backend_init(); });
    auto params = llama_model_default_params();
    params.n_gpu_layers = 0;
    using Model = std::unique_ptr<llama_model, decltype(&llama_model_free)>;
    Model model(llama_model_load_from_file(modelPath.string().c_str(), params),
                llama_model_free);
    if (!model) {
        error = "Could not load the local AI model";
        return false;
    }
    const std::string prompt = "<|im_start|>system\nYou are a concise coding assistant in the Lucid editor. "
                               "Offer suggestions, never claim to have edited files. "
                               "Lucid compile functions are written inside C or C++ block comments: "
                               "start with @name, put each shell command on a line beginning with $, and finish with @end. "
                               "For example: /*\n@build\n$cmake --build %project%/build\n@end\n*/. "
                               "Optional platform suffixes are :macOS, :windows, :linux, :freebsd and :openbsd. "
                               "Commands run from the project root; %project% expands to that root and %file% to the open file. "
                               "Compile functions are not currently executable on Windows.<|im_end|>\n"
                               "<|im_start|>user\n" + std::string(request) +
                               "<|im_end|>\n<|im_start|>assistant\n";
    if (prompt.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
        error = "AI request is too long";
        return false;
    }
    const auto* vocab = llama_model_get_vocab(model.get());
    const int count = -llama_tokenize(vocab, prompt.data(), static_cast<int32_t>(prompt.size()),
                                      nullptr, 0, true, true);
    if (count <= 0 || count > 1536) {
        error = "AI request is too long or could not be tokenized";
        return false;
    }
    std::vector<llama_token> tokens(static_cast<std::size_t>(count));
    if (llama_tokenize(vocab, prompt.data(), static_cast<int32_t>(prompt.size()),
                       tokens.data(), count, true, true) != count) {
        error = "Could not tokenize AI request";
        return false;
    }
    auto contextParams = llama_context_default_params();
    contextParams.abort_callback = [](void* data) -> bool {
        const auto* flag = static_cast<const std::atomic_bool*>(data);
        return flag && flag->load();
    };
    contextParams.abort_callback_data = const_cast<std::atomic_bool*>(cancel);
    contextParams.n_ctx = 2048;
    contextParams.n_batch = static_cast<uint32_t>(count);
    using Context = std::unique_ptr<llama_context, decltype(&llama_free)>;
    Context context(llama_init_from_model(model.get(), contextParams), llama_free);
    if (!context) {
        error = "Could not initialize the local AI model";
        return false;
    }
    using Sampler = std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)>;
    Sampler sampler(llama_sampler_init_greedy(), llama_sampler_free);
    if (!sampler) {
        error = "Could not initialize AI sampling";
        return false;
    }
    auto batch = llama_batch_get_one(tokens.data(), count);
    llama_token token = 0;
    bool completed = false;
    for (int i = 0; i < 512; ++i) {
        if (cancel && cancel->load()) { error = "AI request cancelled"; return false; }
        if (llama_decode(context.get(), batch) != 0) {
            error = "Local AI inference failed";
            return false;
        }
        token = llama_sampler_sample(sampler.get(), context.get(), -1);
        if (llama_vocab_is_eog(vocab, token)) { completed = true; break; }
        char buffer[256];
        const int size = llama_token_to_piece(vocab, token, buffer, sizeof(buffer), 0, false);
        if (size < 0) {
            error = "Could not decode AI response";
            return false;
        }
        output.append(buffer, static_cast<std::size_t>(size));
        batch = llama_batch_get_one(&token, 1);
    }
    if (!completed) { output.clear(); error = "AI response was too long; try a smaller selection"; return false; }
    return true;
}

} // namespace lucid::ai
