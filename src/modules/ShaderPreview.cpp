#include "modules/ShaderPreview.hpp"
#include "modules/ModuleProcess.hpp"
#include <utility>
namespace lucid::modules {
namespace { std::string u8(const std::filesystem::path& p) { const auto s=p.u8string(); return {s.begin(),s.end()}; } }
bool runShaderPreviewHelper(const std::filesystem::path& helper, const std::filesystem::path& stagedArtifact,
                            std::uint32_t width, std::uint32_t height, bool confirmed,
                            ShaderPreviewImage& output, std::string& error,
                            std::chrono::milliseconds timeout, const std::atomic_bool* cancel) {
    output={}; error.clear();
    if (!confirmed) { error="Shader execution requires explicit confirmation"; return false; }
    if (cancel && cancel->load()) { error="Shader preview cancelled"; return false; }
    if (!helper.is_absolute() || !stagedArtifact.is_absolute() || !width || !height || width>512 || height>512 || timeout.count()<=0 || timeout>std::chrono::seconds(30)) { error="Invalid shader preview request"; return false; }
    std::string path; try { path=u8(stagedArtifact); } catch (const std::exception&) { error="Invalid staged artifact path"; return false; }
    const std::string header="P6\n"+std::to_string(width)+" "+std::to_string(height)+"\n255\n";
    const std::size_t count=static_cast<std::size_t>(width)*height*3;
    auto process=runModuleProcess(helper,{path,std::to_string(width),std::to_string(height)},timeout,header.size()+count,cancel);
    if (process.cancelled) { error="Shader preview cancelled"; return false; }
    if (process.timedOut) { error="Shader preview timed out"; return false; }
    if (!process.error.empty() || process.exitCode!=0 || process.outputTruncated) { error="Shader preview helper failed or returned excessive output"; return false; }
    if (process.output.size()!=header.size()+count || process.output.compare(0,header.size(),header)!=0) { error="Shader preview helper returned invalid pixels"; return false; }
    ShaderPreviewImage result; result.width=width; result.height=height; result.rgb.assign(process.output.begin()+static_cast<std::ptrdiff_t>(header.size()),process.output.end()); output=std::move(result); return true;
}
}
