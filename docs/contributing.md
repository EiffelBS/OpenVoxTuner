# Contributing

Thank you for your interest in contributing to OpenVoxTuner! This page summarizes how
you can help. The full details live in [`CONTRIBUTING.md`](https://github.com/EiffelBS/OpenVoxTuner/blob/main/CONTRIBUTING.md).

## Ways to contribute

- **Fixing bugs** — report issues or submit pull requests
- **Improving DSP algorithms** — pitch detection, pitch shifting, formant processing
- **Adding language translations** — the UI supports 5 languages (EN, FR, DE, ES, JA)
- **Enhancing the UI** — visualizers, curve editor, controls
- **Writing documentation** — guides, tutorials, changelogs
- **Testing DAW compatibility** — report working / broken setups

## Getting started

1. Fork the repository
2. Clone your fork
3. Install dependencies:
   - [JUCE 8](https://juce.com/) (Starter Free / AGPLv3)
   - CMake 3.22+
   - A C++17 compiler (MSVC on Windows, Clang on macOS)
4. Build:

   ```bash
   # Windows
   cmake -B build -G "Visual Studio 17 2023"
   cmake --build build --config Release

   # macOS
   cmake -B build -G Ninja
   cmake --build build
   ```

## Development guidelines

- Use **C++17**
- Follow the existing code style (tabs, Allman braces)
- Add a changelog entry in `docs/changelog/` for user-facing changes
- Update `docs/implementation-roadmap.md` for feature additions
- Run the test suite before submitting:

  ```bash
  cmake --build build --config Release --target OpenVoxTunerTests
  ./build/OpenVoxTunerTests_artefacts/Release/OpenVoxTunerTests
  ```

## Code review and AI-assisted code

OpenVoxTuner uses AI coding assistants to accelerate development, **always under
strict human supervision**. Every line of code — whether written by a human or an AI
agent — is reviewed before it is merged. No unverified AI-generated code is merged.

Community pull requests are highly encouraged to audit, improve, or replace
AI-assisted sections.

!!! note "No direct commits unless asked"
    Unless you are explicitly requested to do so, do **not** commit or push changes.
    Open up a pull request instead so the changes can be reviewed before they land.

## Pull request process

1. Create a branch from `main` (e.g. `fix/noise-gate-clicks`)
2. Make your changes with clear, atomic commits
3. Ensure the build passes and tests run
4. Open a pull request with a clear description of:
   - What problem you're solving
   - How you solved it
   - Any UI/behavior changes (with screenshots if applicable)

## License

By contributing, you agree that your contributions will be licensed under the
**AGPLv3** license, the same as the rest of the project.

## Code of conduct

Be respectful, constructive, and welcoming. We want OpenVoxTuner to be a friendly
project for audio engineers, musicians, and developers alike.

## Questions?

Open an issue or start a discussion on [GitHub](https://github.com/EiffelBS/OpenVoxTuner).

## Related pages

- [Build Guide](build-guide.md) — build OpenVoxTuner from source.
- [Testing](testing.md) — build and run the unit test suite.
- [Installation](installation.md) — install the plugin.
