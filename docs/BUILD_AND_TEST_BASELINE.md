# ST Music Workstation — Build & Test Baseline v0.1

Status: Stage 0-E.1 candidate; build/test infrastructure only, no DAW runtime implementation

## 1. Scope

Stage 0-E.1 establishes the first reproducible repository build/test foundation required before production DAW code is introduced.

This baseline activates only:

- C++20 as the ST-owned implementation language standard;
- CMake as the repository build generator;
- CTest as the initial dependency-free test runner supplied by CMake;
- a compiler/build contract smoke executable;
- a least-privilege GitHub Actions build/test workflow.

It does **not** activate Catch2, JUCE, Verovio, FluidSynth, VST3, CLAP, LV2, RtAudio, RtMidi, libsndfile, any AI SDK/provider, sound assets, or plugin SDK.

## 2. Dependency and network rule

The Stage 0-E.1 build must be self-contained with respect to repository source and the toolchain already present on the build environment.

The baseline CMake configuration must not use:

- `FetchContent`;
- `ExternalProject` downloads;
- package-manager bootstrap scripts;
- `file(DOWNLOAD ...)`;
- remote installer execution;
- `curl | sh` / `wget | sh`;
- implicit network dependency resolution.

A later third-party dependency activation must follow `docs/DEPENDENCIES.md` with an exact version/provenance/licence/security review and an explicit adapter boundary where applicable.

## 3. C++ baseline

ST-owned production code may target ISO C++20 after Stage 0-E is closed.

Baseline compiler requirements:

- `std::int64_t` exists and is exactly 64 bits;
- `std::uint64_t` exists and is exactly 64 bits;
- an 8-bit byte is required for the initial supported toolchain contract;
- C++ language extensions are disabled for ST-owned baseline targets where CMake exposes that setting;
- ST-owned targets use a reviewed warning profile and warnings-as-errors in CI.

Compiler-specific extensions or wider integer primitives may be used later as optimizations only when they do not change authoritative acceptance/rejection semantics.

## 4. Initial CMake baseline

The root `CMakeLists.txt` must:

- require an out-of-source build;
- configure C++20 explicitly;
- disable compiler language extensions for ST-owned targets;
- expose a strict-warning option enabled in CI;
- use CTest for baseline tests;
- avoid third-party dependency resolution;
- avoid production domain/runtime code until its dedicated stage.

The baseline does not yet define install, package, signing, release, plugin, asset, or deployment targets.

## 5. Initial CI platform scope

Stage 0-E.1 introduces an initial **Linux build evidence lane**, not the final product platform-support matrix.

The first CI lane uses an explicitly named GitHub-hosted Ubuntu runner image rather than `ubuntu-latest`. The run records `cmake --version` and the active C++ compiler version for evidence.

This does not declare that Linux is the only target OS, and it does not declare Windows/macOS versions supported by the final product. Additional toolchain/platform lanes require bounded follow-up work after the baseline is proven.

No product/platform support claim may be inferred solely from a green hosted-runner build.

## 6. C1 platform-independent rational envelope

Stage 0-C.1 requires one ST-owned numeric acceptance envelope before production Musical Time types are implemented.

Stage 0-E.1 fixes the initial canonical **rational component envelope** as:

```text
RATIONAL_COMPONENT_MAX = 2^31 - 1 = 2147483647

canonical numerator magnitude <= RATIONAL_COMPONENT_MAX
1 <= canonical denominator    <= RATIONAL_COMPONENT_MAX
```

This applies to authoritative normalized exact-rational components used by the Stage 0-C.1 Musical Time conversion model, including canonical rational representations of musical values, TempoBpm and SampleRate where those values otherwise pass their domain-specific ranges.

Rules:

1. Zero denominator is always invalid.
2. The sign is carried by the numerator; canonical denominator is positive.
3. Values are reduced by greatest common divisor before envelope validation.
4. `MusicalPosition` and non-negative domains retain their stricter sign constraints from Stage 0-C.1.
5. A canonical reduced component outside the envelope returns an explicit representation-limit failure; it is not clamped or approximated.
6. The same mathematical value must receive the same accept/reject decision on every supported platform.
7. A platform-specific wider integer (`__int128`, compiler intrinsic, arbitrary precision helper, etc.) must not silently expand the authoritative accepted value set beyond this contract.
8. A later versioned contract may deliberately expand the envelope before persisted production data depends on it; such a change requires compatibility review.

## 7. Portable exact-arithmetic reference constraints

The envelope is chosen so the basic binary rational operations can be decided using checked signed 64-bit intermediate arithmetic after canonical cross-reduction.

Let `M = 2^31 - 1`.

The contract relies on:

```text
M × M < INT64_MAX
2 × M × M < INT64_MAX
```

For binary addition/subtraction:

1. reduce the denominator relationship using `gcd(b, d)`;
2. form the cross-scaled numerators using checked signed 64-bit arithmetic;
3. add/subtract using checked signed 64-bit arithmetic;
4. reduce the mathematical result;
5. require final canonical components to fit the Stage 0-E.1 envelope.

For multiplication/division:

1. cross-cancel numerator/denominator factors using gcd before multiplication;
2. use checked signed 64-bit multiplication;
3. reject division by zero;
4. normalize sign/reduce;
5. require final canonical components to fit the envelope.

For compound expressions such as musical-position → frame conversion, implementations must cross-cancel exact numerator/denominator factors before checked multiplication and must fail explicitly if the exact canonical result cannot satisfy the envelope/intermediate contract.

No signed integer overflow, wraparound, saturation, binary-floating fallback, epsilon snapping, or platform-dependent widening is permitted as authoritative behavior.

## 8. SampleFrame range

Discrete `SampleFrame` is not constrained by the rational component envelope.

The initial contract requires a non-negative signed 64-bit representable frame index:

```text
0 <= SampleFrame <= INT64_MAX
```

Conversion must fail before overflow. No wraparound or saturation is authoritative behavior.

## 9. Baseline smoke/contract executable

Stage 0-E.1 includes a test-only C++ executable that proves the configured compiler accepts the baseline assumptions.

It checks at compile time at minimum:

- C++20 language mode;
- 8-bit byte;
- exact 64-bit fixed-width integer types;
- `M × M` fits signed 64-bit;
- `2 × M × M` fits signed 64-bit.

This executable is **not** the production Musical Time implementation. It is build-contract evidence only.

## 10. Test strategy

The initial test runner is CTest with a dependency-free executable.

Catch2 v3 remains an approved baseline candidate under `docs/DEPENDENCIES.md`, but it is not activated by Stage 0-E.1. It may be pinned and activated later when production-domain tests justify the dependency.

Later production stages must add unit, negative, boundary, integration, determinism, parser/security, and real-time tests appropriate to their contracts.

A green smoke test does not count as runtime evidence for Project Core, Musical Time, Audio, MIDI, Score, TAB, Plugin or AI stages.

## 11. Warning policy

CI builds ST-owned baseline targets with warnings-as-errors.

Initial reviewed profiles:

- MSVC: `/W4 /WX /permissive-`;
- GCC/Clang/AppleClang: `-Wall -Wextra -Wpedantic -Werror`.

Unknown compilers may configure with a warning that no strict profile is yet reviewed; such a compiler is not automatically promoted to a supported CI/toolchain platform.

## 12. Build-output hygiene

Build output must remain out-of-source and untracked.

Repository security/hygiene rules continue to reject committed build/output directories and selected binaries. CI does not upload build artifacts in Stage 0-E.1.

No binary produced by this baseline is a release artifact.

## 13. Failure semantics

The build/test baseline fails closed for:

- in-source builds;
- unsupported compiler language mode;
- violated fixed-width/byte assumptions;
- compiler warnings under the reviewed strict-warning profile;
- compile/link failure;
- CTest failure;
- repository Security Baseline failure.

CI failure is classified and fixed; tests/security controls are not removed merely to make the run green.

## 14. Reproducibility limits

Stage 0-E.1 does not claim bit-for-bit reproducible binaries.

Hosted runner images, compiler versions and CMake versions may evolve. Each run records tool versions, and future platform/toolchain pinning or containerized/hermetic builds may be introduced through a separate reviewed stage if needed.

The authoritative domain contracts must not depend on unspecified compiler behavior, integer overflow, wall-clock time, pointer layout, unordered iteration order, or platform-specific extension semantics.

## 15. Stage 0-E.1 acceptance criteria

The Stage 0-E.1 baseline is acceptable when:

- C++20/CMake/CTest build configuration exists;
- no third-party runtime/test dependency is activated;
- build configuration performs no network dependency fetch;
- out-of-source build is enforced;
- strict warnings are enabled in CI;
- the rational component envelope is fixed platform-independently before Musical Time production code;
- the build-contract executable compiles and passes through CTest;
- GitHub Actions build/test workflow is least-privilege with immutable checkout;
- repository Security Baseline passes on the reviewed head;
- build/test CI passes on the reviewed head;
- Stage 0-E.1 is not misrepresented as a complete DAW/runtime stage.

## 16. Non-goals

Stage 0-E.1 does not:

- add production DAW/domain code;
- activate Catch2 or any third-party application/runtime dependency;
- choose JUCE licensing tier or activate JUCE;
- define final Windows/macOS/Linux support;
- implement Musical Time rational types;
- implement Project IDs;
- implement audio/MIDI/Score/TAB/plugin/AI behavior;
- create release/package/install/signing targets;
- enable branch protection or required status checks;
- publish artifacts or releases.
