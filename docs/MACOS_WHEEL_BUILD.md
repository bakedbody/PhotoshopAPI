# macOS wheel build note

## fmt 11.0.2 and newer AppleClang

When building the Python wheel on Apple Silicon with the repository's pinned
`fmt` 11.0.2, newer AppleClang releases may reject fmt's `consteval` path. The
known working workaround is to change fmt's detected value from:

```cpp
#define FMT_USE_CONSTEVAL 1
```

to:

```cpp
#define FMT_USE_CONSTEVAL 0
```

in the extracted fmt 11.0.2 `include/fmt/base.h` before compiling PhotoshopAPI.
With vcpkg this file is normally under a generated path similar to:

```text
thirdparty/vcpkg/buildtrees/fmt/src/11.0.2-*.clean/include/fmt/base.h
```

This is a build-time workaround only. Users installing the resulting wheel do
not need to set the macro or patch fmt.

### Verified build

The workaround was validated with:

- Apple Silicon (`arm64`)
- Python 3.12
- fmt 11.0.2
- a newer AppleClang toolchain that failed with the consteval path enabled
- PhotoshopAPI test result: `184 passed`

Validated wheel identity:

```text
photoshopapi-0.9.1-cp312-cp312-macosx_13_0_arm64.whl
SHA-256: 1566f3e9e800b4123c2f7977de41b6e7567cf7f7711f6fa02b755b6a7f5500ae
Mach-O: arm64, minos 13.3
```

### Scope

Treat this as a compile tip rather than a runtime or release blocker. Because
`buildtrees` is generated, the edit must be applied again after a clean vcpkg
build. If this wheel needs to be rebuilt regularly, automate the same change
with a vcpkg overlay patch. Re-test without the workaround after upgrading fmt.
