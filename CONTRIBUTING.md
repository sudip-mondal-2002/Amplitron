# Contributing to Amplitron

Thank you for your interest in contributing to Amplitron! This document provides guidelines and instructions for contributing.

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and grow

## How to Contribute

### Reporting Bugs

1. Check if the bug has already been reported in [Issues](https://github.com/sudip-mondal-2002/Amplitron/issues)
2. If not, create a new issue with:
   - Clear title and description
   - Steps to reproduce
   - Expected vs actual behavior
   - Your OS, audio interface, and Amplitron version
   - Any error messages or logs

### Suggesting Features

1. Check existing issues and discussions
2. Create a new issue with the `enhancement` label
3. Describe the feature and its use case
4. Explain how it would benefit users

### Pull Requests

1. **Fork** the repository
2. **Create a branch** from `develop`:
   ```bash
   git checkout -b feature/your-feature-name develop
   ```
2.5. **Set Up Pre-commit Hooks (Crucial for C++ & Formatting)**
   We enforce strict formatting rules using `pre-commit` and `clang-format`. You must set this up before committing code.
   ```bash
   # Install the pre-commit tool (requires Python)
   pip install pre-commit

   # Install the git hook scripts in your local repository
   pre-commit install
   ```
   Now, `clang-format` will automatically check and format your C++ files every time you run `git commit`.

3. **Make your changes**:
   - Follow the existing code style
   - Add tests for new functionality
   - Update documentation as needed
4. **Test your changes**:
   ```bash
   cd build
   ./amplitron-tests
   ```
5. **Commit** with clear messages:
   ```bash
   git commit -m "Add feature: description"
   ```
6. **Push** to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```
7. **Create a Pull Request** on GitHub

## Development Setup

### Prerequisites

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+
- PortAudio
- SDL2
- OpenGL

### Building

See [README.md](README.md) for platform-specific build instructions.

### Running Tests

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make amplitron-tests
./amplitron-tests
```

All tests must pass before submitting a PR (105+ unit tests + Playwright e2e tests in `tests/web/`).

## Code Style

### 🛠️ C++ Standards (Core Engine)
- Follow **Google C++ Style Guide** for all `.cpp`, `.h`, `.cc`, and `.hpp` files.
- Ensure strict adherence to our formatting rules using `clang-format`.
- Use modern C++ features where applicable but maintain performance and memory safety.

- **Indentation**: 4 spaces (no tabs)
- **Naming**:
  - Classes: `PascalCase`
  - Functions/methods: `snake_case`
  - Variables: `snake_case`
  - Constants: `UPPER_SNAKE_CASE`
  - Private members: `trailing_underscore_`
- **Braces**: Opening brace on same line
- **Comments**: Use `//` for single-line, `/* */` for multi-line
- **Headers**: Use `#pragma once`

### Example

```cpp
class AudioEffect {
public:
    void process(float* buffer, int num_samples);
    void set_enabled(bool enabled) { enabled_ = enabled; }

private:
    bool enabled_ = true;
    float mix_ = 1.0f;
};
```

## Testing Guidelines

- Write tests for all new features
- Ensure existing tests still pass
- Test edge cases and error conditions
- Use the test framework in `tests/test_framework.h`

### Test Example

```cpp
TEST(effect_processes_without_nan) {
    Overdrive od;
    od.set_sample_rate(48000);
    od.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    od.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
}
```

## Adding New Effects

1. Create header and source in `src/audio/effects/`
2. Inherit from `Effect` base class
3. Implement required methods:
   - `process(float* buffer, int num_samples)`
   - `set_sample_rate(int sample_rate)`
   - `reset()`
   - `name()`
   - `params()`
4. Add effect color to `src/gui/theme.h`
5. Register in `PresetManager::create_effect()` (`src/preset_manager.cpp`)
6. Add source files to `APP_SOURCES` and `CORE_SOURCES` in `CMakeLists.txt`
7. Add tests in `tests/test_effects.cpp`
8. Include the header in `src/main.cpp` if it should appear in the default chain

## Documentation

- Update README.md for user-facing changes
- Add inline comments for complex algorithms
- Document public APIs with clear descriptions
- Update CHANGELOG.md (if exists)

## Commit Messages

Use clear, descriptive commit messages:

- `Add: New feature description`
- `Fix: Bug description`
- `Update: Change description`
- `Refactor: Code improvement description`
- `Test: Test addition/modification`
- `Docs: Documentation update`
- `ci: setup clang-format pre-commit hook and workflows – CI/CD or automation changes.`

## Questions?

- Open a [Discussion](https://github.com/sudip-mondal-2002/Amplitron/discussions)
- Email: sudmondal2002@gmail.com

Thank you for contributing to Amplitron! 🎸
