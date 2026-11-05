# Contributing to Lucid

Thanks for taking the time to contribute. Lucid is a small, focused editor; keeping the codebase
lean is part of the design.

## Before you start

- Search existing issues and pull requests before opening a new one. Use Discussions for questions and early ideas.
- For a large feature or a platform-behaviour change, open an issue first so the design can be
  discussed before you invest in an implementation.
- Security issues should follow [SECURITY.md](SECURITY.md), not the public issue tracker.
- Read [SUPPORT.md](SUPPORT.md) if you are unsure where a report belongs. The full guides are
  published at <https://initializeddd.github.io/Lucid/>.

## Development workflow

1. Fork the repository and create a branch from `main`.
2. Build and run the test suite locally using the instructions in
   [docs/building.md](docs/building.md).
3. Keep commits focused. Write a short imperative subject, then explain the *why* in the body.
4. Format C++ sources with the committed `.clang-format`.
5. Open a pull request against `main` and fill in the template.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

For an optional cross-platform smoke check, install a MinGW C++20 compiler and
run `python3 tools/modules/check_mingw.py`. Pass
`--compiler i686-w64-mingw32-g++` to check 32-bit Windows instead of the
default 64-bit target. This checks module translation units except the three
that require target-specific libcurl headers. It does not link or run Lucid and
does not replace native Windows tests.

## Code style

- C++20, four-space indentation, 100-column limit. The repository ships a `.clang-format`; run it
  before committing:

  ```sh
  clang-format -i path/to/changed_file.cpp
  ```

- Prefer explicit ownership and RAII. Avoid hidden global state and framework gravity.
- Keep platform code under `src/platform`; do not leak GLFW or native window handles into the
  core, editor, renderer facade or UI layers.
- Add a test when you change document, syntax, workspace, settings or compile-script behaviour.
  Tests live in `tests/` and are registered in `tests/CMakeLists.txt`.

## Commit messages

Use an imperative subject line under ~72 characters:

```text
Add JSON diagnostics to the status line
Fix caret drift on high-DPI displays
Document the Windows Vulkan SDK requirement
```

Group unrelated changes into separate commits. Do not commit build output, caches or generated
artifacts.

## Pull requests

A good pull request:

- explains the problem and the chosen approach;
- includes a test or a manual verification note;
- keeps unrelated refactors out of the diff;
- updates `README.md` or `docs/` when behaviour or requirements change;
- passes CI on macOS, Linux and Windows.

By contributing you agree that your contributions are licensed under the [MIT License](LICENSE).
