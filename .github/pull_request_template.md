## Summary

<!-- What changed and why? Keep it to a few sentences. -->

## Related issues

<!-- e.g. Closes #123 -->

## How to test

<!-- Steps a reviewer can follow, or the commands you ran. -->

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Checklist

- [ ] I read the [contribution guide](https://github.com/initializeddd/Lucid/blob/main/CONTRIBUTING.md).
- [ ] The change is focused and contains no unrelated formatting churn.
- [ ] I added or updated tests for behaviour changes.
- [ ] I updated the README or `docs/` if requirements or behaviour changed.
- [ ] The build and tests pass on my platform.
- [ ] My commits use an imperative subject line.
