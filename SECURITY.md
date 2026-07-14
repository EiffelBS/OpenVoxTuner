# Security Policy

## Supported Versions

OpenVoxTuner is under active development. Security fixes are applied to the
latest released version and to the `main` branch.

| Version | Supported          |
| ------- | ------------------ |
| latest  | :white_check_mark: |
| older   | :x:                |

## Reporting a Vulnerability

If you discover a security vulnerability, please **do not open a public issue**.

Report it privately through one of the following channels:

- **GitHub Security Advisories** (preferred): open a private advisory from the
  *Security* tab of the repository. This keeps the report confidential and lets
  us coordinate a fix and a coordinated disclosure.
- **Email**: send details to security@openvoxtuner.com (use this only if you
  cannot use GitHub advisories).

Please include:

- A description of the vulnerability and its impact.
- Steps to reproduce, or a proof of concept.
- The affected version(s) / commit(s).

We will acknowledge your report as soon as possible, work on a fix, and credit
you (unless you prefer to remain anonymous) once a patched release is published.

## Scope

This policy covers the OpenVoxTuner source code, build scripts, and released
binaries. It does **not** cover third-party dependencies (JUCE, ARA SDK,
PreSonus extensions, Catch2); please report issues with those projects to their
respective maintainers.
