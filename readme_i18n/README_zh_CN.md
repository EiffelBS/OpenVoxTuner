<p align="center">
  <a href="https://opensource.org/license/agpl-v3"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg?color=3F51B5&style=for-the-badge&label=License&logoColor=000000&labelColor=ececec" alt="许可证: AGPLv3"></a>
  <img src="https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=cplusplus&logoColor=000000&labelColor=ececec" alt="C++17">
  <img src="https://img.shields.io/badge/JUCE-8-orange.svg?style=for-the-badge&labelColor=ececec" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Platform-Win%20%7C%20Mac-lightgrey.svg?style=for-the-badge&labelColor=ececec" alt="平台">
  <img src="https://img.shields.io/badge/Format-VST3%20%7C%20AU%20%7C%20Standalone-green.svg?style=for-the-badge&labelColor=ececec" alt="格式">
</p>

<p align="center">
  <img src="../assets/icon.png" width="120" alt="OpenVoxTuner 图标">
</p>

<h1 align="center">OpenVoxTuner</h1>

<h3 align="center">面向人声的实时音高校正与和声生成</h3>

<p align="center">
  VST3 / AU / Standalone &mdash; 使用 JUCE 8 (C++17) 构建
</p>

<p align="center">
  <a href="#功能">功能</a> &bull;
  <a href="#截图">截图</a> &bull;
  <a href="#许可证">许可证</a> &bull;
  <a href="#构建">构建</a>
</p>

<p align="center">
  <a href="../README.md">English</a> &mdash;
  <a href="README_fr_FR.md">Fran&ccedil;ais</a> &mdash;
  <a href="README_de_DE.md">Deutsch</a> &mdash;
  <a href="README_es_ES.md">Espa&ntilde;ol</a> &mdash;
  <a href="README_ja_JP.md">&#26085;&#26412;&#35486;</a> &mdash;
  &#20013;&#25991;
</p>

## 目录

- [截图](#截图)
- [功能](#功能)
- [为什么选择 OpenVoxTuner？](#为什么选择-openvoxtuner)
- [仓库结构](#仓库结构)
- [许可证](#许可证)
- [支持项目](#支持项目)
- [开发者许可证](#开发者许可证)
- [贡献](#贡献)
- [构建](#构建)
- [文档](#文档)

---

## 截图

<!-- 占位图片（项目所有者已授权使用虚拟图片，2026-07-12）。
     等真实截图准备好后，请将 placehold.co 的 URL 替换为 docs/screenshots/ 中的实际截图，例如
     <img src="docs/screenshots/main-view.png" width="80%" alt="OpenVoxTuner 主窗口"> -->

<p align="center">
  <img src="https://placehold.co/960x540/15151f/e0e0e0?text=OpenVoxTuner+Main+View" width="80%" alt="OpenVoxTuner 主窗口 — 占位图">
</p>
<p align="center">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Live+Visualizer" width="45%" alt="实时可视化 — 占位图">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Curve+Editor" width="45%" alt="曲线编辑器 — 占位图">
</p>

## 功能

### 音高校正

- **自动模式** — 选择调性与音阶后量化到音阶（大调、小调、五声、半音、自定义）
- **图形模式** — 绘制你自己的音高曲线（Melodyne 风格编辑器，带吸附、网格、复制/粘贴、撤销/重做）
- **速度控制** — 回拉包络，让校正自然或机器人化
- **强度控制** — 校正信号与干信号之间的混合

### 效果

- **共振峰偏移** — 共振峰独立保持/移调（-12 至 +12 半音）
- **混响** — 内置混响效果，可调节混合电平
- **噪声门** — 输入噪声门，阈值可调（-80 至 0 dB），在音高检测之前应用以获得更干净的结果

### A/B 对比与变形

- **A/B 槽位** — 保存并调用两个完整的插件状态（A 和 B）
- **变形控制** — 在 A 和 B 状态之间连续插值
- **自动保存** — 切换时自动保存当前槽位
- 所有参数平滑变形（旋钮连续 lerp，开关在 50% 切换）
- A/B 状态在会话之间保持

### 和声引擎

- **Use Voice 模式** — 将你的实时人声移调为和声音符（1–4 个移调声部，带立体声平移）
- **Synth 模式** — 合成的和声音色（Choir、Bright、Synth Lead、Strings、Guitar、Vocoder 风格），可调音色
- **和声类型预设** — 上/下三度/五度、Vocal Stack、Power Chord、Parallel 3rd、Drone
- **音量与混音** — 独立的和声电平与 Wet/Dry 混音
- 曲线编辑器上叠加的和声轨迹

### 音高检测

- **YIN** — 时域自相关（快速、低 CPU，唯一使用的检测器）
- SWIPE' 和 PYIN 已被评估并移除（仅保留 YIN 以保证速度与稳定性）

### ARA2 集成

- 完整 ARA2 支持（Audio Random Access）— 与 DAW 时间线的无缝集成
- 从 ARA 音乐上下文自动提取调性与音阶
- 对拍号敏感的小节标尺（3/4、4/4、6/8、12/8）
- 播放时自动滚动跟随 DAW 播放头
- 多拍号支持（项目中途变更拍号）

### 曲线编辑器

- 图形化音高曲线编辑器，可拖拽、添加、删除点
- 吸附到音阶、吸附到网格
- 小节选择器（1、2、4、8、16、32）
- 复制/粘贴与撤销/重做
- 叠加和声轨迹
- 带音符名与 Hz 读数的水平光标线
- 音阶参考线（与实时可视化器相同）
- 波形叠加（Line 或 Mirror，与实时可视化器同步）
- 自动滚动开关（在所有模式下工作）

### 实时可视化器

- 带输入/输出/和声轨迹的实时音高可视化
- 自动高亮音符的钢琴键盘
- 带音符名与 Hz 读数的水平光标线
- 波形叠加（Line 或 Mirror）
- 带统计信息的图例块（准音 %、平均音分）
- 导出 PNG 图像（2 倍分辨率）

### 波形叠加

- 在所有模式（插件、独立、ARA）下从输入捕获的波形
- 菜单中可选择的两种显示类型：
  - **Line** — 简单波形轮廓（40% 不透明度）
  - **Mirror** — 围绕中心对称的条带（默认）
- 显示类型统一应用于实时可视化器和曲线编辑器
- 设置在会话之间保持

### 主题系统

- 带统一调色板的深色与浅色主题
- 带完整 UI 刷新的自动主题切换
- 弹出菜单的一致颜色（汉堡菜单、预设、下拉框）
- 修正的提示框，使用干净的矩形渲染

### 其他

- MIDI 音符输出（由检测到的音高生成）
- 立体声处理
- 低延迟 PSOLA 音高变换
- 旁路开关（独立模式）
- 带内部 120 BPM 走带的独立模式

## 为什么选择 OpenVoxTuner？

- **设计即开源** — 每一行 DSP、UI 与预设逻辑都是公开的。没有黑盒，没有遥测，没有功能付费墙。你可以准确了解你的人声是如何被处理的。
- **AGPLv3 保障自由** — 许可证确保项目保持自由与开放。任何人都可以使用它（即使是商业用途），任何改进都必须与社区分享。
- **原生 ARA2** — 与 DAW 的深度集成意味着调性、音阶和速度直接从你的项目中读取。无需手动设置，无需猜测 — OpenVoxTuner 自动跟随你的编排。
- **为真实人声而建** — YIN 音高检测、保持共振峰的 PSOLA 以及 Melodyne 风格的曲线编辑器，都是针对演唱表演的细微差别调整的，而不仅仅是概念验证演示。

### AI 辅助开发

OpenVoxTuner 使用 AI 编程助手来加速开发，始终在严格的人工监督下进行。每一行代码都经过审查、测试，并完全开放给社区审计。未经核实的 AI 生成代码不会被合并。

## 仓库结构

```text
.
├─ Source/
│  ├─ dsp/                        # DSP 模块
│  │  ├─ IPitchDetector.h         # 音高检测器抽象接口
│  │  ├─ YinPitchDetector.*       # YIN 算法（当前使用）
│  │  ├─ ScaleQuantizer.*         # 音阶量化引擎
│  │  ├─ PitchShifter.*           # PSOLA 音高变换器
│  │  ├─ RetargetEnvelope.*       # 速度包络平滑器
│  │  ├─ FormantPreserver.*       # 共振峰补偿滤波器
│  │  ├─ NoiseGate.h              # 输入噪声门（RMS、迟滞）
│  │  ├─ PitchCurve.*             # 曲线数据模型
│  │  ├─ PresetMorpher.h          # A/B 变形引擎
│  │  ├─ HarmonyEngine.*          # 和声合成引擎
│  │  ├─ PitchDetector.*          # 原始 YIN 参考（未编译）
│  │  └─ NoteUtils.h / IPitchShifter.h
│  ├─ ui/                         # UI 组件
│  │  ├─ PitchCurveEditor.*       # 曲线编辑器组件
│  │  ├─ PitchVisualizer.*        # 音高可视化
│  │  ├─ PianoKeyboard.*          # 钢琴控件
│  │  ├─ ScaleKeyboardComponent.* # 音阶键盘显示
│  │  ├─ LookAndFeel.*            # 自定义外观
│  │  ├─ OVTTheme.h               # 主题颜色与共享波形渲染器
│  │  ├─ OVTFonts.h               # 字体辅助
│  │  └─ OVTLanguages.h           # 多语言翻译
│  ├─ external/presonus/          # PreSonus 扩展（Studio One）
│  ├─ resources/                  # 二进制资源（BuildInfo.h.in）
│  ├─ PluginProcessor.*           # 主音频处理器
│  └─ PluginEditor.*              # 主编辑器 UI
├─ scripts/                       # 构建与开发脚本
│  ├─ build.ps1                   # Windows 构建
│  ├─ build_installer.ps1         # Windows 安装程序（Inno Setup）
│  ├─ build_macos_vst3.sh         # macOS VST3 构建
│  ├─ build_macos_au.sh           # macOS AU 构建
│  ├─ build_macos_pkg.sh          # macOS .pkg 安装程序
│  ├─ build_macos.sh              # macOS 通用构建
│  └─ ...（安装、符号链接、发布辅助）
├─ test/                          # 单元测试（Catch2）
│  ├─ Main.cpp
│  └─ dsp/                        # 按模块划分的测试套件
├─ docs/                        # 文档
│  ├─ releases/                   # 发布说明（latest.json、v0.1.1.md）
│  ├─ architecture.md
│  ├─ default-parameters.md
│  └─ ...
├─ installer/                     # Windows 安装程序资源
│  └─ OpenVoxTuner.iss            # Inno Setup 脚本
├─ .github/                       # CI/CD 与 Issue 模板
│  ├─ workflows/                  # GitHub Actions（CI、发布）
│  └─ ISSUE_TEMPLATE/             # Bug report / Feature request
├─ assets/                        # 二进制资源
│  └─ icon.png
├─ external/ARA_SDK/              # Celemony ARA SDK（v2.2，submodule）
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ roadmap.md
├─ .gitignore
├─ .gitattributes
└─ .gitmodules
```

## 许可证

OpenVoxTuner 在 [AGPLv3](../LICENSE) 许可证下**对所有人免费** — 音乐家、制作人、工作室、教育者。对商业使用没有限制。

### 第三方许可证

| 库 | 许可证 | 兼容性 |
|---------|---------|--------------|
| JUCE 8 | AGPLv3 | 相同许可证 |
| ARA SDK | Apache 2.0 | 完全兼容 |
| PreSonus 扩展 | 公共领域 | 完全兼容 |
| Catch2（测试） | Boost (BSL-1.0) | 完全兼容 |

所有第三方许可证均与 AGPLv3 兼容。

### AGPLv3 对你的意义

| 你是... | 免费？ | 义务？ |
|---|---|---|
| 音乐家 / 制作人 | 是 | 无 — 只需做音乐 |
| 工作室（混音、母带、制作） | 是 | 无 — 你将插件作为工具使用 |
| 教育者 / 学生 | 是 | 无 |
| 开发者（修改并再分发） | 是 | 你必须以 AGPLv3 分享你修改的源代码 |
| 公司（fork 为闭源产品） | 否 | 你需要商业许可证 |

> 实际上，如果你用 OpenVoxTuner 做音乐 — 即使是专业用途 — AGPLv3 许可证完全免费且无任何义务。

### 支持项目

OpenVoxTuner 对所有人免费。如果 OpenVoxTuner 为你节省了时间或帮助了你的音乐，请考虑支持这个项目 — 即使是一笔小小的捐赠也能带来巨大的不同。

| 层级 | 平台 | 价格 | 权益 |
|------|----------|-------|----------|
| **免费** | — | 0 € | 完整插件，所有功能 |
| **Buy me a coffee** | [Ko-fi](https://ko-fi.com/) | 一次性 | 一句简单的感谢 ❤️ |
| **Sponsor** | [GitHub Sponsors](https://github.com/sponsors/) | 按月 | 支持持续开发 |
| **Supporter** | [Patreon](https://patreon.com/) | 5 €/月 | 私密 Discord + 对未来功能的投票 |
| **Gold** | [Patreon](https://patreon.com/) | 20 €/月 | 所有 Supporter 权益 + 抢先体验 / 测试版构建 + 署名 |

每一份贡献都有助于让项目保持活力并免费对所有人开放。

### 开发者许可证

对于希望将 OpenVoxTuner 集成到**闭源产品**中而不承担 AGPLv3 _copyleft_ 义务的开发者或公司，提供商业许可证。

**授予的权利：**
- 在专有软件中使用 OpenVoxTuner 的 DSP、UI 组件和算法的许可
- 无 copyleft 义务 — 你**不**被迫公开你的源代码
- 无将衍生物以 AGPLv3 发布的强制要求

**包含内容：**
- 优先电子邮件支持
- 可选的定制功能与 DSP 咨询
- 所购版本的永久许可证（更新依层级而定）

**联系方式：** 在 GitHub 上开 Issue 或发邮件至 [license@openvoxtuner.com](mailto:license@openvoxtuner.com)。

## 贡献

欢迎贡献！你可以通过以下方式帮助：

- 修复 bug
- 改进 DSP 算法
- 添加翻译
- 改进 UI
- 编写文档
- 测试 DAW 兼容性

详见 [CONTRIBUTING.md](../CONTRIBUTING.md)。

### AI 生成代码的审查

OpenVoxTuner 的某些部分借助 AI 编程代理编写，始终在人工监督下。所有代码在合并前都会经过审查，我们热烈欢迎社区提交 Pull Request 来审计、改进或替换 AI 辅助的部分。

## 构建

### Windows（Visual Studio）

先决条件：
- Visual Studio 2022
- CMake >= 3.22
- JUCE 8（CMake 中默认路径：`C:/JUCE`）
- Git LFS（用于二进制资源）

```powershell
# Debug 构建
.\scripts\build.ps1 -Configuration Debug

# Release 构建
.\scripts\build.ps1 -Configuration Release

# 构建 Windows 安装程序（需要 Inno Setup）
.\scripts\build_installer.ps1
```

### macOS（VST3 / AU / pkg）

先决条件：

```bash
xcode-select --install
brew install cmake ninja
```

构建 VST3：

```bash
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

构建 AU：

```bash
./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install
```

构建 macOS `.pkg` 安装程序。官方发布提供 **VST3 + Standalone**（AU 被省略，因为未签名的 AU 无法被 DAW 加载）：

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

若要在本地构建中同时包含 AU 组件，请将其加入 `--formats`（例如 `VST3,AU,STANDALONE`）。

详细构建指南：
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## 格式

| 格式 | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅*   |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

> \* AU 组件可从源代码构建，但**未包含**在未签名的发布中 — 未签名的 AU 无法在 macOS 上被 DAW 加载。发布的版本在两个平台上都包含 **VST3 + Standalone**。

## 文档

- [docs/architecture.md](../docs/architecture.md) — 软件架构概述
- [docs/default-parameters.md](../docs/default-parameters.md) — 所有插件参数参考
- [docs/implementation-roadmap.md](../docs/implementation-roadmap.md) — 功能路线图
- [docs/ARA_Specifications.md](../docs/ARA_Specifications.md) — ARA2 支持技术规格
- [docs/deployment-and-packaging-guide.md](../docs/deployment-and-packaging-guide.md) — 发布工作流
- [docs/GITHUB_SETUP_AND_RELEASE.md](../docs/GITHUB_SETUP_AND_RELEASE.md) — GitHub 仓库设置
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md) — macOS VST3 构建指南
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md) — macOS AU + 安装程序指南
- [docs/preset-morphing-technical-strategy.md](../docs/preset-morphing-technical-strategy.md) — A/B 预设变形策略
- [docs/releases/v0.1.1.md](../docs/releases/v0.1.1.md) — 发布说明

## 许可证

见 [LICENSE](../LICENSE)。

## Issue 报告

使用 GitHub Issue 模板：
- [Bug report](../.github/ISSUE_TEMPLATE/bug_report.md)
- [Feature request](../.github/ISSUE_TEMPLATE/feature_request.md)

## 星标历史

<a href="https://star-history.com/#EiffelBS/OpenVoxTuner&type=date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" width="100%" />
  </picture>
</a>
