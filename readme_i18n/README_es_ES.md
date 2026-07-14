<p align="center">
  <a href="https://opensource.org/license/agpl-v3"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg?color=3F51B5&style=for-the-badge&label=License&logoColor=000000&labelColor=ececec" alt="Licencia: AGPLv3"></a>
  <img src="https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=cplusplus&logoColor=000000&labelColor=ececec" alt="C++17">
  <img src="https://img.shields.io/badge/JUCE-8-orange.svg?style=for-the-badge&labelColor=ececec" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Platform-Win%20%7C%20Mac-lightgrey.svg?style=for-the-badge&labelColor=ececec" alt="Plataformas">
  <img src="https://img.shields.io/badge/Format-VST3%20%7C%20AU%20%7C%20Standalone-green.svg?style=for-the-badge&labelColor=ececec" alt="Formatos">
</p>

<p align="center">
  <img src="../assets/icon.png" width="120" alt="Icono de OpenVoxTuner">
</p>

<h1 align="center">OpenVoxTuner</h1>

<h3 align="center">Corrección de tono y generación de armonía en tiempo real para voces</h3>

<p align="center">
  VST3 / AU / Standalone &mdash; construido con JUCE 8 (C++17)
</p>

<p align="center">
  <a href="#funcionalidades">Funcionalidades</a> &bull;
  <a href="#capturas">Capturas</a> &bull;
  <a href="#licencia">Licencia</a> &bull;
  <a href="#build">Build</a>
</p>

<p align="center">
  <a href="../README.md">English</a> &mdash;
  <a href="README_fr_FR.md">Fran&ccedil;ais</a> &mdash;
  <a href="README_de_DE.md">Deutsch</a> &mdash;
  Espa&ntilde;ol &mdash;
  <a href="README_ja_JP.md">&#26085;&#26412;&#35486;</a> &mdash;
  <a href="README_zh_CN.md">&#20013;&#25991;</a>
</p>

## Tabla de contenidos

- [Capturas](#capturas)
- [Funcionalidades](#funcionalidades)
- [¿Por qué OpenVoxTuner?](#¿por-qué-openvoxtuner)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Licencia](#licencia)
- [Apoyar el proyecto](#apoyar-el-proyecto)
- [Licencia de desarrollador](#licencia-de-desarrollador)
- [Contribuir](#contribuir)
- [Build](#build)
- [Documentación](#documentación)

---

## Capturas

<!-- Imágenes de marcador de posición (el propietario del proyecto autorizó imágenes ficticias, 2026-07-12).
     Reemplaza las URL de placehold.co con capturas reales en docs/screenshots/
     cuando estén disponibles, p. ej.
     <img src="docs/screenshots/main-view.png" width="80%" alt="Ventana principal de OpenVoxTuner"> -->

<p align="center">
  <img src="https://placehold.co/960x540/15151f/e0e0e0?text=OpenVoxTuner+Main+View" width="80%" alt="Ventana principal de OpenVoxTuner — placeholder">
</p>
<p align="center">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Live+Visualizer" width="45%" alt="Visualizador en vivo — placeholder">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Curve+Editor" width="45%" alt="Editor de curva — placeholder">
</p>

## Funcionalidades

### Corrección de tono

- **Modo auto** — cuantización a escala con selección de tonalidad y escala (Mayor, Menor, Pentatónica, Cromática, Personalizada)
- **Modo gráfico** — dibuja tu propia curva de tono (editor estilo Melodyne con snap, rejilla, copiar/pegar, deshacer/rehacer)
- **Control de velocidad** — envolvente de redirección para una corrección natural o robótica
- **Control de cantidad** — mezcla entre la señal corregida y la seca

### Efectos

- **Desplazamiento de formante** — preservación/transposición independiente de formantes (-12 a +12 semitonos)
- **Reverb** — efecto de reverberación integrado con nivel de mezcla ajustable
- **Noise Gate** — puerta de ruido de entrada con umbral ajustable (-80 a 0 dB), aplicada antes de la detección de tono para resultados más limpios

### Comparación y morphing A/B

- **Slots A/B** — guarda y recupera dos estados completos del plugin (A y B)
- **Control de morph** — interpola continuamente entre los estados A y B
- **Autoguardado** — el slot actual se guarda automáticamente al cambiar
- Todos los parámetros hacen morph suavemente (lerp continuo para diales, conmutación al 50 % para toggles)
- Estado A/B persistente entre sesiones

### Motor de armonía

- **Modo Use Voice** — desplaza tu voz en vivo a notas de armonía (1–4 voces desplazadas con paneo estéreo)
- **Modo Synth** — tonos de armonía sintetizados (Choir, Bright, Synth Lead, Strings, Guitar, tipo Vocoder) con color ajustable
- **Presets de tipo de armonía** — 3.ª/5.ª por debajo/encima, Vocal Stack, Power Chord, Parallel 3rd, Drone
- **Volumen y mezcla** para nivel de armonía y wet/dry independientes
- Traza de armonía superpuesta en el editor de curva

### Detección de tono

- **YIN** — autoc correlación en el dominio del tiempo (rápido, bajo CPU, el único detector usado)
- SWIPE' y PYIN fueron evaluados y eliminados (se mantiene solo YIN por velocidad y estabilidad)

### Integración ARA2

- Soporte completo de ARA2 (Audio Random Access) — integración transparente con la línea de tiempo del DAW
- Extracción automática de tonalidad/escala desde el contexto musical ARA
- Regla de compás sensible a la métrica (3/4, 4/4, 6/8, 12/8)
- Autodesplazamiento siguiendo el playhead del DAW durante la reproducción
- Soporte multi-firma (cambio de métrica a mitad de proyecto)

### Editor de curva

- Editor gráfico de curva de tono con arrastrar, añadir, eliminar puntos
- Snap to Scale, Snap to Grid
- Selector de compases (1, 2, 4, 8, 16, 32)
- Copiar/pegar y deshacer/rehacer
- Superposición de traza de armonía
- Línea de cursor horizontal con nombre de nota y lectura en Hz
- Líneas de referencia de notas de escala (igual que el visualizador en vivo)
- Overlay de waveform (Line o Mirror, sincronizado con el visualizador en vivo)
- Conmutador de autodesplazamiento (funciona en todos los modos)

### Visualizador en vivo

- Visualización de tono en tiempo real con trazas de entrada/salida/armonía
- Teclado de piano con resaltado automático de notas
- Línea de cursor horizontal con nombre de nota y lectura en Hz
- Overlay de waveform (Line o Mirror)
- Bloque de leyenda con estadísticas (en tono %, cents medios)
- Exportar como imagen PNG (resolución 2x)

### Overlay de waveform

- Waveform capturada desde la entrada en todos los modos (plugin, standalone, ARA)
- Dos tipos de visualización seleccionables en el menú:
  - **Line** — contorno simple de waveform (40 % de opacidad)
  - **Mirror** — barras simétricas alrededor del centro (por defecto)
- El tipo de visualización se aplica uniformemente al visualizador en vivo y al editor de curva
- Preferencia persistente entre sesiones

### Sistema de temas

- Temas Oscuro y Claro con paleta de colores unificada
- Cambio automático de tema con refresco completo de la UI
- Colores coherentes en menús emergentes (menú hamburguesa, presets, combos)
- Tooltips corregidos con render rectangular limpio

### Adicional

- Salida de nota MIDI (generada a partir del tono detectado)
- Procesamiento estéreo
- Pitch shifting PSOLA de baja latencia
- Conmutador Bypass (modo standalone)
- Modo standalone con transporte interno a 120 BPM

## ¿Por qué OpenVoxTuner?

- **Open source por diseño** — cada línea de DSP, UI y lógica de preset es pública. Sin caja negra, sin telemetría, sin paywalls de funciones. Puedes leer exactamente cómo se procesa tu voz.
- **AGPLv3 para la libertad** — la licencia garantiza que el proyecto permanezca libre y abierto. Cualquiera puede usarlo (incluso comercialmente), y cualquier mejora debe compartirse con la comunidad.
- **ARA2 nativo** — la integración profunda con el DAW significa que tonalidad, escala y tempo se leen directamente de tu proyecto. Sin configuración manual, sin conjeturas — OpenVoxTuner sigue tu arreglo automáticamente.
- **Hecho para voces reales** — la detección YIN, el PSOLA preservador de formantes y un editor de curva estilo Melodyne están afinados para los matices de las interpretaciones cantadas, no solo para demos de prueba de concepto.

### Desarrollo asistido por IA

OpenVoxTuner usa asistentes de codificación IA para acelerar el desarrollo, siempre bajo estricta supervisión humana. Cada línea de código se revisa, prueba y queda abierta por completo a la auditoría de la comunidad. No se fusiona código generado por IA sin verificar.

## Estructura del repositorio

```text
.
├─ Source/
│  ├─ dsp/                        # Módulos DSP
│  │  ├─ IPitchDetector.h         # Interfaz abstracta del detector de tono
│  │  ├─ YinPitchDetector.*       # Algoritmo YIN (activo)
│  │  ├─ ScaleQuantizer.*         # Motor de cuantización de escala
│  │  ├─ PitchShifter.*           # Pitch shifter PSOLA
│  │  ├─ RetargetEnvelope.*       # Suavizador de envolvente de velocidad
│  │  ├─ FormantPreserver.*       # Filtro de compensación de formantes
│  │  ├─ NoiseGate.h              # Puerta de ruido de entrada (RMS, histéresis)
│  │  ├─ PitchCurve.*             # Modelo de datos de curva
│  │  ├─ PresetMorpher.h          # Motor de morphing A/B
│  │  ├─ HarmonyEngine.*          # Motor de síntesis de armonía
│  │  ├─ PitchDetector.*          # Referencia YIN original (no compilada)
│  │  └─ NoteUtils.h / IPitchShifter.h
│  ├─ ui/                         # Componentes UI
│  │  ├─ PitchCurveEditor.*       # Componente editor de curva
│  │  ├─ PitchVisualizer.*        # Visualización de tono
│  │  ├─ PianoKeyboard.*          # Widget de teclado de piano
│  │  ├─ ScaleKeyboardComponent.* # Visualización de teclado de escala
│  │  ├─ LookAndFeel.*            # Look and feel personalizado
│  │  ├─ OVTTheme.h               # Colores de tema y renderer de waveform compartido
│  │  ├─ OVTFonts.h               # Helpers de fuentes
│  │  └─ OVTLanguages.h           # Traducciones multilingües
│  ├─ external/presonus/          # Extensiones PreSonus (Studio One)
│  ├─ resources/                  # Recursos binarios (BuildInfo.h.in)
│  ├─ PluginProcessor.*           # Procesador de audio principal
│  └─ PluginEditor.*              # UI del editor principal
├─ scripts/                       # Scripts de build y desarrollo
│  ├─ build.ps1                   # Build Windows
│  ├─ build_installer.ps1         # Instalador Windows (Inno Setup)
│  ├─ build_macos_vst3.sh         # Build VST3 macOS
│  ├─ build_macos_au.sh           # Build AU macOS
│  ├─ build_macos_pkg.sh          # Instalador .pkg macOS
│  ├─ build_macos.sh              # Build universal macOS
│  └─ ... (ayudantes de instalación, symlink, release)
├─ test/                          # Tests unitarios (Catch2)
│  ├─ Main.cpp
│  └─ dsp/                        # Suites de test por módulo
├─ docs/                        # Documentación
│  ├─ releases/                   # Notas de release (latest.json, v0.1.1.md)
│  ├─ architecture.md
│  ├─ default-parameters.md
│  └─ ...
├─ installer/                     # Recursos del instalador Windows
│  └─ OpenVoxTuner.iss            # Script de Inno Setup
├─ .github/                       # CI/CD y plantillas de issues
│  ├─ workflows/                  # GitHub Actions (CI, release)
│  └─ ISSUE_TEMPLATE/             # Bug report / Feature request
├─ assets/                        # Recursos binarios
│  └─ icon.png
├─ external/ARA_SDK/              # Celemony ARA SDK (v2.2, submodule)
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ docs/implementation-roadmap.md
├─ .gitignore
├─ .gitattributes
└─ .gitmodules
```

## Licencia

OpenVoxTuner es **gratis para todos** bajo la licencia [AGPLv3](../LICENSE) — músicos, productores, estudios, educadores. Sin restricciones de uso comercial.

### Licencias de terceros

| Biblioteca | Licencia | Compatibilidad |
|---------|---------|--------------|
| JUCE 8 | AGPLv3 | Misma licencia |
| ARA SDK | Apache 2.0 | Totalmente compatible |
| Extensiones PreSonus | Dominio público | Totalmente compatible |
| Catch2 (tests) | Boost (BSL-1.0) | Totalmente compatible |

Todas las licencias de terceros son compatibles con AGPLv3.

### Qué significa AGPLv3 para ti

| Eres... | ¿Gratis? | ¿Obligación? |
|---|---|---|
| Músico / Productor | Sí | Ninguna — solo haz música |
| Estudio (mix, master, producción) | Sí | Ninguna — usas el plugin como herramienta |
| Educador / Estudiante | Sí | Ninguna |
| Desarrollador (modifica y redistribuye) | Sí | Debes compartir tu código modificado bajo AGPLv3 |
| Empresa (fork a producto cerrado) | No | Necesitas una licencia comercial |

> En la práctica, si usas OpenVoxTuner para hacer música — incluso profesionalmente — la licencia AGPLv3 es totalmente gratuita sin obligaciones.

### Apoyar el proyecto

OpenVoxTuner es gratis para todos. Si OpenVoxTuner te ahorra tiempo o ayuda a tu música, considera apoyar el proyecto — incluso una pequeña donación marca una gran diferencia.

| Nivel | Plataforma | Precio | Beneficios |
|------|----------|-------|----------|
| **Gratis** | — | 0 $ | Plugin completo, todas las funciones |
| **Buy me a coffee** | [Ko-fi](https://ko-fi.com/) | Unico | Un rápido gracias ❤️ |
| **Sponsor** | [GitHub Sponsors](https://github.com/sponsors/) | Mensual | Apoyo al desarrollo continuo |
| **Supporter** | [Patreon](https://patreon.com/) | 5 $/mes | Discord privado + voto en próximas funciones |
| **Gold** | [Patreon](https://patreon.com/) | 20 $/mes | Todos los beneficios Supporter + acceso anticipado / builds beta + nombre en créditos |

Cada contribución ayuda a mantener el proyecto vivo y gratis para todos.

### Licencia de desarrollador

Una licencia comercial está disponible para desarrolladores o empresas que quieran integrar OpenVoxTuner en un **producto cerrado** sin la obligación de copyleft AGPLv3.

**Lo que otorga:**
- Permiso para usar el DSP, los componentes UI y los algoritmos de OpenVoxTuner en software propietario
- Sin obligación de copyleft — **no** estás obligado a publicar tu código fuente
- Sin requisito de publicar derivados bajo AGPLv3

**Lo que incluye:**
- Soporte por email prioritario
- Funciones personalizadas y consultoría DSP opcionales
- Licencia perpetua para la versión comprada (actualizaciones según nivel)

**Contacto:** abre una issue en GitHub o escribe a [license@openvoxtuner.com](mailto:license@openvoxtuner.com).

## Contribuir

¡Las contribuciones son bienvenidas! Puedes ayudar:

- Corrigiendo bugs
- Mejorando algoritmos DSP
- Añadiendo traducciones
- Mejorando la UI
- Escribiendo documentación
- Probando compatibilidad con DAWs

Ver [CONTRIBUTING.md](../CONTRIBUTING.md) para detalles.

### Revisión de código generado por IA

Algunas partes de OpenVoxTuner se escriben con ayuda de agentes de codificación IA, siempre bajo supervisión humana. Todo el código se revisa antes de fusionar, y los pull requests de la comunidad son muy bienvenidos para auditar, mejorar o reemplazar secciones asistidas por IA.

## Build

### Windows (Visual Studio)

Requisitos:
- Visual Studio 2022
- CMake >= 3.22
- JUCE 8 (ruta por defecto en CMake: `C:/JUCE`)
- Git LFS (para recursos binarios)

```powershell
# Build Debug
.\scripts\build.ps1 -Configuration Debug

# Build Release
.\scripts\build.ps1 -Configuration Release

# Build del instalador Windows (requiere Inno Setup)
.\scripts\build_installer.ps1
```

### macOS (VST3 / AU / pkg)

Requisitos:

```bash
xcode-select --install
brew install cmake ninja
```

Build VST3:

```bash
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

Build AU:

```bash
./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install
```

Build del instalador `.pkg` macOS. Los releases oficiales entregan **VST3 + Standalone** (el AU se omite porque un AU sin firmar no puede ser cargado por un DAW):

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

Para incluir también el componente AU en un build local, añádelo a `--formats` (p. ej. `VST3,AU,STANDALONE`).

Guías de build detalladas:
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## Formatos

| Formato   | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅*   |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

> \* El componente AU se puede compilar desde las fuentes, pero **no se incluye** en los releases sin firmar — un AU sin firmar no puede ser cargado por un DAW en macOS. Los releases distribuidos incluyen **VST3 + Standalone** en ambas plataformas.

## Documentación

- [docs/architecture.md](../docs/architecture.md) — visión general de la arquitectura del software
- [docs/default-parameters.md](../docs/default-parameters.md) — referencia de todos los parámetros del plugin
- [docs/implementation-roadmap.md](../docs/implementation-roadmap.md) — hoja de ruta de funcionalidades
- [docs/ARA_Specifications.md](../docs/ARA_Specifications.md) — especificaciones técnicas del soporte ARA2
- [docs/deployment-and-packaging-guide.md](../docs/deployment-and-packaging-guide.md) — flujo de release
- [docs/GITHUB_SETUP_AND_RELEASE.md](../docs/GITHUB_SETUP_AND_RELEASE.md) — configuración del repositorio GitHub
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md) — guía de build VST3 macOS
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md) — guía AU + instalador macOS
- [docs/preset-morphing-technical-strategy.md](../docs/preset-morphing-technical-strategy.md) — estrategia de morphing A/B
- [docs/releases/v0.1.1.md](../docs/releases/v0.1.1.md) — notas de release

## Licencia

Ver [LICENSE](../LICENSE).

## Reporte de issues

Usa las plantillas de issue de GitHub:
- [Bug report](../.github/ISSUE_TEMPLATE/bug_report.md)
- [Feature request](../.github/ISSUE_TEMPLATE/feature_request.md)

## Historial de estrellas

<a href="https://star-history.com/#EiffelBS/OpenVoxTuner&type=date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" width="100%" />
  </picture>
</a>