#include "platform/browser/WebPreviewPresentation.hpp"
#include <utility>
namespace lucid::platform {
DecodedWebPreview decodeWebPreviewPng(std::string png) {
    DecodedWebPreview result; (void)modules::decodeVisualPng(png,result.image,result.error); return result;
}
}
