# Changelog - 2026-08-01

## Added
- **Plugin icon (SVG) integrated into the landing site and documentation**: Added `assets/icon.svg` and reused the existing `assets/icon.png` across the public-facing sites, replacing the previous generic placeholder icons.

## Landing site (Astro)
- **Favicon**: Replaced the placeholder `site/public/favicon.svg` with the plugin icon (browser tab icon).
- **Apple touch icon**: Added `site/public/apple-touch-icon.png` (copied from `assets/icon.png`) to fix the dead `/apple-touch-icon.png` reference in `Layout.astro` (previously a 404 on Apple devices).
- **Footer logo**: Replaced the inline placeholder SVG in `site/src/components/Footer.astro` with the plugin icon (`/assets/icon.svg`).
- **Hero logo**: Added the plugin icon above the title in `site/src/components/Hero.astro`.

## Documentation (MkDocs Material)
- **Logo & favicon**: Set `theme.logo` and `theme.favicon` to `assets/icon.svg` in `mkdocs.yml`, so the docs header and browser tab show the plugin icon (replacing the default "book" icon).

## Technical Details
- Copied `assets/icon.svg` to `site/public/favicon.svg`, `site/public/assets/icon.svg`, and `docs/assets/icon.svg`.
- Copied `assets/icon.png` to `site/public/apple-touch-icon.png`.
- `site/src/components/Footer.astro`: placeholder `<svg>` replaced by `<img src="/assets/icon.svg">`.
- `site/src/components/Hero.astro`: added `<img src="/assets/icon.svg">` with glow styling.
- `mkdocs.yml`: added `theme.logo` / `theme.favicon`.

## Landing page cleanup — end-user oriented (2026-08-01)
- **Removed developer-facing info** from the landing page (it targets end users, not developers). Removed all mentions of `JUCE 8`, `C++17`, and "Built with ..." from:
  - Hero subtitle in all 6 locales (`site/src/i18n/{en,fr,de,es,ja,zh}.json`).
  - Hero tech badges (`site/src/components/Hero.astro`).
  - Marquee strips (`site/src/pages/{index,fr,de,es,ja,zh}/index.astro`).
  - Inline footers (`site/src/pages/{index,fr,de,es,ja,zh}/index.astro`).
- **Kept** end-user-relevant info: plugin formats (VST3 / AU / Standalone / ARA2) and the open-source license (AGPLv3 / Open Source).
- **Plugin icon in the real inline footers**: added `/assets/icon.svg` to the inline footers of all 6 pages (`site/src/pages/{index,fr,de,es,ja,zh}/index.astro`) so the footer icon is actually visible on the rendered landing.
- **Removed unused `site/src/components/Footer.astro`** (dead code — the pages use their own inline footers) and the now-orphaned `footer.builtWith` i18n key across all 6 locales (`site/src/i18n/{en,fr,de,es,ja,zh}.json`).

## Documentation site — English-only + missing pages (2026-08-01)
- **Switched the docs site to English** (`mkdocs.yml`): `theme.language` `fr`→`en`, search `lang` `fr`→`en`, and translated all navigation labels to English (Home, Installation, User Guide, Quick Start, Correction Modes, Curve Editor, Harmony Generation, Default Parameters, Architecture, Overview, DSP Pipeline, User Interface, Development, Build Guide, Tests, Contributing, Reference, DSP API, ARA2, Presets, Changelog, License).
- **Generated the previously missing pages** (they were 404 in the nav):
  - `docs/installation.md`
  - `docs/user-guide/quickstart.md`, `correction-modes.md`, `curve-editor.md`, `harmony.md`
  - `docs/architecture/dsp-pipeline.md`, `ui.md`
  - `docs/build-guide.md`, `testing.md`, `contributing.md`
  - `docs/reference/dsp-api.md`, `ara2.md`, `presets.md`
  - `docs/license.md` (AGPLv3 + third-party licenses)
- **Removed the temporary `docs/changelog.md` index** again: it linked to the daily changelogs under `docs/changelogs/`, which are **excluded from git** (`.gitignore`). Since the deployed docs site is built from the git checkout, those pages would not exist on the live site (404s). Git-excluded changelogs must not be referenced from the public docs.
- **Removed dead config** referencing nonexistent assets (`extra_css: stylesheets/extra.css` and `extra_javascript: javascripts/mermaid.js`) that caused 404s on every page.
- Fixed internal links in `contributing.md` and `license.md` to point to the GitHub repo.
- Docs build now completes with **no nav "file not found" warnings** (the only remaining warnings are pre-existing broken links inside `docs/formant-preservation-analysis-report.md`).

## macOS AU/PKG signing — marked as temporary (2026-08-01)
- The messages about **unsigned** macOS `.pkg` / missing **AU** now explicitly state that it is a **temporary** situation (code signing & notarization are planned for a later release), in:
  - `README.md` (Download table + note, `.pkg` build comment, Formats table note)
  - `docs/installation.md` (AU note, `.pkg` warning, "Building the AU" section)
  - `docs/build-guide.md` (AU note, `.pkg` installer section)
  - `readme_i18n/README_{fr_FR,de_DE,es_ES}.md` (build comment + formats note)
- **Clarified AU local usability**: the AU *is* usable locally — clarified that the unsigned/non-notarized AU is not **distributed** because it is not reliably loadable by all DAWs (esp. Logic Pro), not because it cannot run at all. Added the **ad-hoc signing** step (`codesign --force --deep -s -`) + DAW AU-validation warning to `docs/installation.md` (Building the AU), and reflected the local-vs-official nuance in `README.md` and the FR/DE/ES i18n READMEs.

## Tests
- N/A (static site asset/branding change, no DSP code affected).
