# Changelog - 2026-08-17

## GitHub Sponsors: FUNDING.yml created (2026-08-17)
- **What**: created `.github/FUNDING.yml` with `github: [EiffelBS]` to enable the **Sponsor** button on the `OpenVoxTuner` repository (the button is already live on `@EiffelBS` user profile, the FUNDING.yml enables it per-repository).
- **File**: [.github/FUNDING.yml](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/.github/FUNDING.yml)

## Landing page: removed all "Melodyne-style" references (2026-08-17)
- **Problem**: the landing page still compared the Curve Editor to Melodyne, which could mislead users into thinking OpenVoxTuner is affiliated with or equivalent to Celemony Melodyne (it is not).
- **Fix**: rewrote the `curveEditor.description` and `showcase.slides[2].title` in all 6 i18n files to use neutral descriptive phrasing:
  - English: `Curve Editor` → description "Graphical pitch editing: add/move points...", slide title "Graphical Curve Editor"
  - French: `Édition graphique de type Melodyne` → "Édition graphique du pitch : ajouter/déplacer des points...", slide "Éditeur de courbe graphique"
  - German: `Melodyne-Editor-Style grafische Bearbeitung` → "Grafische Tonhöhenbearbeitung: Punkte hinzufügen/verschieben...", slide "Grafischer Kurveneditor"
  - Spanish: `Edición gráfica estilo Melodyne` → "Edición gráfica de tono: añadir/mover puntos...", slide "Editor de curvas gráfico"
  - Japanese: `Melodyneスタイルのグラフィック編集` → "グラフィカルピッチ編集：ポイントの追加/移動...", slide "グラフィカルカーブエディター"
  - Chinese: `Melodyne 式图形编辑` → "图形化音高编辑：添加/移动点...", slide "图形化曲线编辑器"
- **Files changed**:
  - [site/src/i18n/en.json](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/i18n/en.json)
  - [site/src/i18n/fr.json](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/i18n/fr.json)
  - [site/src/i18n/de.json](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/i18n/de.json)
  - [site/src/i18n/es.json](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/i18n/es.json)
  - [site/src/i18n/ja.json](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/i18n/ja.json)
  - [site/src/i18n/zh.json](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/i18n/zh.json)
- **Verification**: `grep Melodyne site/src/i18n/` → **0 matches** (clean).

## Landing page: fixed broken Documentation link (2026-08-17)
- **Problem**: the footer "Documentation" link pointed to `/docs/` (relative path) on `openvoxtuner.eiffelbs.ovh`, giving a 404 because the docs are not a sub-path of the landing page — they are deployed as a **separate** Cloudflare Pages project at **`https://ovtdocs.eiffelbs.ovh/`** (per `mkdocs.yml` → `site_url: https://ovtdocs.eiffelbs.ovh/` and `.github/workflows/deploy-docs.yml` → `project-name=openvoxtuner-docs`).
- **Fix**: changed the footer Documentation link from a relative `/docs/` to an absolute `https://ovtdocs.eiffelbs.ovh/` in **all 6 locale pages**.
- **Files changed**:
  - [site/src/pages/index.astro](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/pages/index.astro)
  - [site/src/pages/fr/index.astro](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/pages/fr/index.astro)
  - [site/src/pages/de/index.astro](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/pages/de/index.astro)
  - [site/src/pages/es/index.astro](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/pages/es/index.astro)
  - [site/src/pages/ja/index.astro](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/pages/ja/index.astro)
  - [site/src/pages/zh/index.astro](file:///c:/Users/User/Documents/trae_projects/OpenVoxTuner/site/src/pages/zh/index.astro)
- **Verification**: `grep /docs/ site/` → **0 matches** (clean).
