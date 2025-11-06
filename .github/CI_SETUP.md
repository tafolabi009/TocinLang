# GitHub Actions CI/CD Setup

This document describes the continuous integration setup for the Tocin compiler.

## Overview

The Tocin compiler uses GitHub Actions for automated building and testing across multiple platforms:

- **Linux** (Ubuntu Latest) - Debug & Release builds
- **macOS** (Latest) - Release builds
- **Windows** (Latest) - Release builds

## Build Matrix

| Platform | Build Types | LLVM Version | Status |
|----------|-------------|--------------|--------|
| Ubuntu Latest | Debug, Release | 18 | [![Linux](https://github.com/tafolabi009/tocin-compiler/workflows/CI/badge.svg)](https://github.com/tafolabi009/tocin-compiler/actions) |
| macOS Latest | Release | 18 | [![macOS](https://github.com/tafolabi009/tocin-compiler/workflows/CI/badge.svg)](https://github.com/tafolabi009/tocin-compiler/actions) |
| Windows Latest | Release | 18 | [![Windows](https://github.com/tafolabi009/tocin-compiler/workflows/CI/badge.svg)](https://github.com/tafolabi009/tocin-compiler/actions) |

## CI Pipeline Stages

### 1. Environment Setup
- Checkout source code
- Install CMake 3.25+
- Install Python 3.11
- Install Node.js 20
- Cache LLVM and dependencies

### 2. Dependency Installation

**Linux (Ubuntu):**
```bash
apt-get install llvm-18-dev libclang-18-dev clang-18 lld-18 \
  libzstd-dev libxml2-dev zlib1g-dev libffi-dev \
  libssl-dev libreadline-dev ninja-build python3-dev
```

**macOS:**
```bash
brew install llvm@18 zstd libxml2 cmake ninja libffi \
  openssl readline python@3.11
```

**Windows:**
```powershell
choco install llvm --version=18.1.8
choco install cmake
```

### 3. Build Configuration

CMake is configured with the following flags:
- `CMAKE_BUILD_TYPE`: Debug or Release
- `WITH_PYTHON=ON`: Enable Python FFI
- `WITH_V8=OFF`: V8 disabled for CI (not available via package managers)
- `WITH_ZSTD=ON`: Enable compression
- `WITH_XML=ON`: Enable XML support
- `ENABLE_TESTING=ON`: Build test suite
- `ENABLE_SANITIZERS=OFF`: Disabled for Release builds

### 4. Build

```bash
cmake --build build --config $BUILD_TYPE --parallel
```

Uses all available CPU cores for parallel compilation.

### 5. Testing

**Unit Tests:**
```bash
ctest --output-on-failure --parallel
```

**Integration Tests:**
- Run compiler on test files in `tests/` directory
- Allow failures (tests may use advanced syntax not yet implemented)
- Report pass/fail statistics

### 6. Artifacts

The following artifacts are uploaded for each build:

- **Binary artifacts**: `tocin-$OS-$BUILD_TYPE`
  - Contains the compiled `tocin` executable
  - Available for 7 days after build

- **Test results**: `test-results-$OS-$BUILD_TYPE`
  - CTest output and logs
  - Available for 7 days after build

## Running CI Locally

Before pushing to GitHub, you can simulate the CI pipeline locally:

```bash
# Run the local CI simulation script
./scripts/test-ci-locally.sh
```

This script will:
1. Check prerequisites (CMake, LLVM, Python)
2. Clean previous build
3. Configure CMake with CI flags
4. Build the project
5. Run unit tests
6. Run integration tests
7. Generate summary report

## Viewing CI Results

1. **GitHub Actions Tab**: Visit https://github.com/tafolabi009/tocin-compiler/actions
2. **Pull Request Checks**: Status checks appear on PRs automatically
3. **README Badge**: Top of README.md shows current CI status

## Troubleshooting CI Failures

### Build Failures

**LLVM not found:**
- Check LLVM version matches (`LLVM_VERSION` env var)
- Verify installation paths in workflow

**Compiler errors:**
- Review build logs in GitHub Actions
- Test locally with same CMake flags
- Check for C++17 compatibility

### Test Failures

**Unit tests failing:**
- Download test artifacts from GitHub Actions
- Review CTest output logs
- Run specific tests locally: `ctest -R test_name -V`

**Integration tests failing:**
- Expected for advanced syntax not yet implemented
- Check parser error messages
- Verify basic functionality still works

### Timeout Issues

**Build timeout (60 minutes):**
- Check for infinite loops in build scripts
- Verify dependency caching is working
- Consider disabling debug builds temporarily

## Workflow Triggers

The CI pipeline runs on:

1. **Push to main/master branches**
2. **Pull requests to main/master**
3. **Manual trigger** via GitHub Actions UI (workflow_dispatch)

## Disabling CI

To skip CI on a commit:
```bash
git commit -m "Your message [skip ci]"
```

Or for specific jobs:
```bash
git commit -m "Your message [skip ci:windows]"
```

## Future Improvements

- [ ] Add code coverage reporting (Codecov/Coveralls)
- [ ] Enable Address Sanitizer for Debug builds
- [ ] Add performance benchmarking CI job
- [ ] Create nightly builds with extended test suite
- [ ] Add static analysis (clang-tidy, cppcheck) as required checks
- [ ] Docker-based builds for reproducibility
- [ ] Cross-compilation for ARM64
- [ ] Release automation with GitHub Releases

## Contact

For CI/CD issues or questions:
- Open an issue: https://github.com/tafolabi009/tocin-compiler/issues
- Email: tafolabi009@gmail.com
