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
  <a href="#构建">构建</a> &bull;
  <a href="https://openvoxtuner.eiffelbs.ovh" target="_blank">网站</a> &bull;
  <a href="https://ovtdocs.eiffelbs.ovh" target="_blank">文档</a>
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

[截图](#截图) &bull;
[功能](#功能) &bull;
[为什么选择 OpenVoxTuner？](#为什么选择-openvoxtuner) &bull;
[仓库结构](#仓库结构) &bull;
[许可证](#许可证) &bull;
[支持项目](#支持项目) &bull;
[开发者许可证](#开发者许可证) &bull;
[贡献](#贡献) &bull;
[构建](#构建) &bull;
[文档](#文档)

---

## 截图

<p align="center">
  <img src="../assets/screenshots/main_screen.png" width="80%" alt="OpenVoxTuner 主窗口">
</p>

<details>
<summary><strong>更多截图...</strong></summary>

<p align="center">
  <img src="../assets/screenshots/curve_editor.png" width="45%" alt="曲线编辑器">
  <img src="../assets/screenshots/curve_pianoroll.png" width="45%" alt="钢琴卷帘视图">
</p>
<p align="center">
  <img src="../assets/screenshots/harmony_types.png" width="80%" alt="和声类型">
</p>

</details>

## 功能

### 音高校正

- **自动模式** — 14 种音阶类型的音阶量化（大调、小调、五声、布鲁斯、多利亚、弗里几亚、利底亚、混合利底亚、洛克利亚、半音、自定义...）
- **图形模式** — 绘制你自己的音高曲线（图形编辑器，带吸附、网格、复制/粘贴、撤销/重做）
- **校正模式** — 现代（紧凑）或透明（柔和）特性
- **速度与强度** — 回拉包络与干湿混合，实现自然或机器人化响应
- **人性化** — 添加微妙的随机变化以获得更自然的声音（0–50 音分）
- **颤音保持** — 在校正过程中保留歌手的自然颤音（0–100%）
- **声部类型** — 限制音高检测范围（通用、男低音、男中音、男高音、女中音、女高音）
- **延迟模式** — Direct Monitoring、Low Latency、Quality、Safe

### 调性检测

- **自动** — 从音频输入实时检测调性（Krumhansl-Schmuckler 模型）
- **OpenVoxKey** — 通过共享内存 IPC 的配套桥接
- **侧链** — 通过专用侧链总线分析伴奏

### 效果

- **共振峰处理** — 3 种模式（Legacy、MultiFormant、Allpass）与多种保持策略（支持 LPC 交叉合成）
- **混响** — 内置混响，可调节混合比例
- **噪声门** — 带阈值控制的输入门（-80 至 0 dB）
- **上行压缩器** — 在音高检测前提升安静段落

### 和声引擎

- **Use Voice 模式** — 将你的实时人声移调为 1–4 个和声声部，带立体声声像
- **Synth 模式** — 合成的和声音色（合唱、管风琴），可调音色
- **22 种和声类型** — 音程（三度/四度/五度/八度 上/下）、Vocal Stack、Power Chord、Drone、齐唱...
- **和声控制** — 增益匹配（自动 RMS 平衡）、跟随主导、各声部起音、和声共振峰移调（-5 至 +5 半音）
- 曲线编辑器上叠加的和声轨迹

### 曲线编辑器与可视化器

- 图形化音高曲线编辑器，支持点拖拽、吸附到音阶/网格、复制/粘贴、撤销/重做
- 带输入/输出/和声轨迹的实时音高可视化
- 自动高亮音符的钢琴键盘
- 波形叠加（Line、Mirror 或 Spectral 显示）
- 导出为 PNG（2 倍分辨率）

### ARA2 集成

- ARA2 支持 — 从 DAW 时间线读取调性、音阶和拍号
- 对拍号敏感的小节标尺与自动滚动
- 多拍号支持（项目中途变更拍号）
- 与 DAW 时间线同步的波形叠加

> ARA2 将插件与 DAW 时间线（调性/音阶、拍号、播放头、波形）同步。OpenVoxTuner 仍然是一个**实时效果器** — 它不提供像专用音高编辑器那样的逐音符离线编辑。

### A/B 对比与变形

- **A/B 槽位** — 保存并调用两个完整的插件状态
- **变形滑块** — 在 A 和 B 之间连续插值（所有参数平滑过渡）
- 状态在会话之间保持

### 其他

- MIDI 音符输出（由检测到的音高生成）
- MIDI 目标（传入 MIDI 控制校正音高）
- 带卡片式画廊 UI 的曲线预设
- 全局撤销/重做（所有可自动化参数）
- 深色/浅色主题
- 立体声处理，低延迟 PSOLA 音高变换
- 带内部 120 BPM 走带的独立模式

## 为什么选择 OpenVoxTuner？

- **设计即开源** — 每一行 DSP、UI 与预设逻辑都是公开的。没有黑盒，没有遥测，没有功能付费墙。你可以准确了解你的人声是如何被处理的。
- **AGPLv3 保障自由** — 许可证确保项目保持自由与开放。任何人都可以使用它（即使是商业用途），任何改进都必须与社区分享。
- **原生 ARA2** — 调性、音阶和拍号直接从你的 DAW 项目读取，让插件无需手动设置即可跟随你的编排。
- **为真实人声而建** — YIN 音高检测、保持共振峰的 PSOLA 以及图形曲线编辑器，都是针对演唱表演的细微差别调整的，而不仅仅是概念验证演示。

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
├─ docs/implementation-roadmap.md
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
| **Supporter** | Patreon | 即将推出 | 私密 Discord + 对未来功能的投票 |
| **Gold** | Patreon | 即将推出 | 所有 Supporter 权益 + 抢先体验 / 测试版构建 + 署名 |

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

**联系方式：** 在 GitHub 上开 Issue。

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

构建 macOS `.pkg` 安装程序。官方发布提供 **已签名并公证** 的安装程序，包含 **VST3 + AU + Standalone**：

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,AU,STANDALONE
```

若要构建不含 AU 的更小安装程序，请覆盖 `--formats`（例如 `VST3,STANDALONE`）。

详细构建指南：
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## 格式

| 格式 | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅    |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

## 文档

- [网站](https://openvoxtuner.eiffelbs.ovh) — 功能概览和下载链接的着陆页
- [在线文档](https://ovtdocs.eiffelbs.ovh) — 完整文档 (MkDocs Material)
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
