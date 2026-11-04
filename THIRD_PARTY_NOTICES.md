# Third-party notices

Lucid includes and uses work from other projects. This page is a guide to the notices in the
current source tree; dependencies and their licenses may differ between release artifacts and
platform builds. Review the notices included with the specific distribution you use.

| Component | Use | Notice or source |
| --- | --- | --- |
| JetBrains Mono | Bundled font | [SIL Open Font License 1.1](assets/fonts/JetBrainsMono-OFL.txt) |
| stb | Font rasterization | [Vendoring notes](third_party/stb/README.md); license text is included in the header |
| PicoSHA2 | SHA-256 | [MIT license](assets/licenses/PicoSHA2-LICENSE.txt) |
| llama.cpp and ggml | Local inference | [MIT license](assets/licenses/llama.cpp-LICENSE.txt) |
| curl | Network transfers | [Copyright and permission notice](assets/licenses/curl-LICENSE.txt) |
| GLFW | Windows and input | [GLFW source and license](https://github.com/glfw/glfw) |

The build also uses system or SDK components, including Vulkan and platform graphics libraries.
The local AI model is downloaded separately; its model card and terms are available from
[Qwen2.5-Coder-0.5B-Instruct-GGUF](https://huggingface.co/Qwen/Qwen2.5-Coder-0.5B-Instruct-GGUF).
