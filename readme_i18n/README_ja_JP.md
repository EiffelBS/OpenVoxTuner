<p align="center">
  <a href="https://opensource.org/license/agpl-v3"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg?color=3F51B5&style=for-the-badge&label=License&logoColor=000000&labelColor=ececec" alt="ライセンス: AGPLv3"></a>
  <img src="https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=cplusplus&logoColor=000000&labelColor=ececec" alt="C++17">
  <img src="https://img.shields.io/badge/JUCE-8-orange.svg?style=for-the-badge&labelColor=ececec" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Platform-Win%20%7C%20Mac-lightgrey.svg?style=for-the-badge&labelColor=ececec" alt="プラットフォーム">
  <img src="https://img.shields.io/badge/Format-VST3%20%7C%20AU%20%7C%20Standalone-green.svg?style=for-the-badge&labelColor=ececec" alt="フォーマット">
</p>

<p align="center">
  <img src="../assets/icon.png" width="120" alt="OpenVoxTuner アイコン">
</p>

<h1 align="center">OpenVoxTuner</h1>

<h3 align="center">ボーカルのためのリアルタイムピッチ補正・ハーモニー生成</h3>

<p align="center">
  VST3 / AU / Standalone &mdash; JUCE 8 (C++17) で構築
</p>

<p align="center">
  <a href="#機能">機能</a> &bull;
  <a href="#スクリーンショット">スクリーンショット</a> &bull;
  <a href="#ライセンス">ライセンス</a> &bull;
  <a href="#ビルド">ビルド</a>
</p>

<p align="center">
  <a href="../README.md">English</a> &mdash;
  <a href="README_fr_FR.md">Fran&ccedil;ais</a> &mdash;
  <a href="README_de_DE.md">Deutsch</a> &mdash;
  <a href="README_es_ES.md">Espa&ntilde;ol</a> &mdash;
  &#26085;&#26412;&#35486; &mdash;
  <a href="README_zh_CN.md">&#20013;&#25991;</a>
</p>

## 目次

- [スクリーンショット](#スクリーンショット)
- [機能](#機能)
- [なぜ OpenVoxTuner なのか](#なぜ-openvoxtuner-なのか)
- [リポジトリ構造](#リポジトリ構造)
- [ライセンス](#ライセンス)
- [プロジェクトを支援する](#プロジェクトを支援する)
- [開発者ライセンス](#開発者ライセンス)
- [コントリビュート](#コントリビュート)
- [ビルド](#ビルド)
- [ドキュメント](#ドキュメント)

---

## スクリーンショット

<!-- プレースホルダー画像（プロジェクト所有者がダミー画像を許可、2026-07-12）。
     用意でき次第、placehold.co の URL を docs/screenshots/ の実際のスクリーンショットに差し替えてください。
     例：
     <img src="docs/screenshots/main-view.png" width="80%" alt="OpenVoxTuner メインウィンドウ"> -->

<p align="center">
  <img src="https://placehold.co/960x540/15151f/e0e0e0?text=OpenVoxTuner+Main+View" width="80%" alt="OpenVoxTuner メインウィンドウ — プレースホルダー">
</p>
<p align="center">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Live+Visualizer" width="45%" alt="ライブビジュアライザー — プレースホルダー">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Curve+Editor" width="45%" alt="カーブエディター — プレースホルダー">
</p>

## 機能

### ピッチ補正

- **オートモード** — キーとスケールを選択して音階に量子化（メジャー、マイナー、ペンタトニック、クロマティック、カスタム）
- **グラフィックモード** — 独自のピッチカーブを描画（Melodyne 風エディター：スナップ、グリッド、コピー/ペースト、Undo/Redo）
- **スピードコントロール** — 補正を自然にもロボット風にもする戻りエンベロープ
- **アマウントコントロール** — 補正信号とドライ信号のミックス

### エフェクト

- **フォルマントシフト** — フォルマントの独立した保持/移調（-12 〜 +12 半音）
- **リバーブ** — ミックスレベル調整可能な内蔵リバーブ
- **ノイズゲート** — しきい値調整可能な入力ノイズゲート（-80 〜 0 dB）。ピッチ検出の前に適用し、よりクリーンな結果を得る

### A/B 比較とモーフィング

- **A/B スロット** — 2 つの完全なプラグイン状態（A と B）を保存・呼び出し
- **モーフコントロール** — A と B の状態間を連続的に補間
- **オートセーブ** — 切り替え時に現在のスロットを自動保存
- すべてのパラメーターがスムーズにモーフ（ダイヤルは連続 lerp、トグルは 50 % で切り替え）
- A/B 状態はセッション間で維持

### ハーモニーエンジン

- **Use Voice モード** — 生ボイスをハーモニーノートにリアルタイム移調（1〜4 声のシフト、ステレオパン）
- **Synth モード** — 合成ハーモニートーン（Choir、Bright、Synth Lead、Strings、Guitar、Vocoder 風）と調整可能なカラー
- **ハーモニータイププリセット** — 3 度/5 度 上/下、Vocal Stack、Power Chord、Parallel 3rd、Drone
- **ボリュームとミックス** — 独立したハーモニーレベルと Wet/Dry ミックス
- カーブエディター上のハーモニートレース重ね合わせ

### ピッチ検出

- **YIN** — 時間領域の自己相関（高速、低 CPU、現在使用中の唯一の検出器）
- SWIPE' と PYIN は評価の上削除（速度と安定性のため YIN のみを維持）

### ARA2 統合

- 完全な ARA2 サポート（Audio Random Access）— DAW タイムラインとのシームレスな統合
- ARA 音楽コンテキストからのキー/スケール自動抽出
- 拍子に敏感な小節ルーラー（3/4、4/4、6/8、12/8）
- 再生中は DAW のプレイヘッドに追従して自動スクロール
- マルチ拍子対応（プロジェクト中盤の拍子変更）

### カーブエディター

- ポイントのドラッグ・追加・削除ができるグラフィックピッチカーブエディター
- Snap to Scale、Snap to Grid
- 小節セレクター（1、2、4、8、16、32）
- コピー/ペーストと Undo/Redo
- ハーモニートレース重ね合わせ
- 音符名と Hz 表示付きの水平カーソルライン
- スケール基準線（ライブビジュアライザーと同じ）
- 波形オーバーレイ（Line または Mirror、ライブビジュアライザーと同期）
- 自動スクロール切り替え（すべてのモードで動作）

### ライブビジュアライザー

- 入力/出力/ハーモニーのトレース付きリアルタイムピッチ可視化
- 音符の自動ハイライト付きピアノキーボード
- 音符名と Hz 表示付きの水平カーソルライン
- 波形オーバーレイ（Line または Mirror）
- 統計付き凡例ブロック（音程内 %、平均セント）
- PNG 画像書き出し（2 倍解像度）

### 波形オーバーレイ

- すべてのモード（プラグイン、スタンドアロン、ARA）で入力からキャプチャした波形
- メニューで選択可能な 2 種類の表示：
  - **Line** — シンプルな波形輪郭（不透明度 40 %）
  - **Mirror** — 中心に対称なバー（デフォルト）
- 表示タイプはライブビジュアライザーとカーブエディターの両方に統一して適用
- セッション間で設定が維持される

### テーマシステム

- 統一されたカラーパレットを持つダークとライトのテーマ
- UI を完全に更新する自動テーマ切り替え
- ポップアップメニューの一貫した色（ハンバーガーメニュー、プリセット、コンボ）
- 清潔な矩形レンダリングに修正されたツールチップ

### その他

- MIDI ノート出力（検出されたピッチから生成）
- ステレオ処理
- 低レイテンシの PSOLA ピッチシフト
- バイパス切り替え（スタンドアロンモード）
- 内部 120 BPM トランスポート付きスタンドアロンモード

## なぜ OpenVoxTuner なのか

- **設計によるオープンソース** — DSP、UI、プリセットロジックのすべての行が公開されています。ブラックボックスもテレメトリも機能のペイウォールもなし。あなたの声がどう処理されるかを正確に読むことができます。
- **自由のための AGPLv3** — ライセンスはプロジェクトが自由かつオープンであり続けることを保証します。誰もが（商用含め）利用でき、改善はコミュニティと共有されなければなりません。
- **ネイティブ ARA2** — DAW との深い統合により、キー、スケール、テンポがプロジェクトから直接読み取られます。手動設定も推測も不要 — OpenVoxTuner はアレンジメントに自動的に追従します。
- **本物の歌声のために設計** — YIN ピッチ検出、フォルマント保持 PSOLA、Melodyne 風カーブエディターは、単なるコンセプト実証デモではなく、歌の演技のニュアンスに合わせて調整されています。

### AI 支援開発

OpenVoxTuner は開発を加速するために AI コーディングアシスタントを使用していますが、常に厳格な人間の監督下です。すべてのコード行はレビュー・テストされ、コミュニティの監査に完全に開かれています。未検証の AI 生成コードはマージされません。

## リポジトリ構造

```text
.
├─ Source/
│  ├─ dsp/                        # DSP モジュール
│  │  ├─ IPitchDetector.h         # ピッチ検出器の抽象インターフェース
│  │  ├─ YinPitchDetector.*       # YIN アルゴリズム（現役）
│  │  ├─ ScaleQuantizer.*         # スケール量子化エンジン
│  │  ├─ PitchShifter.*           # PSOLA ピッチシフター
│  │  ├─ RetargetEnvelope.*       # スピードエンベロープ平滑化
│  │  ├─ FormantPreserver.*       # フォルマント補償フィルター
│  │  ├─ NoiseGate.h              # 入力ノイズゲート（RMS、ヒステリシス）
│  │  ├─ PitchCurve.*             # カーブデータモデル
│  │  ├─ PresetMorpher.h          # A/B モーフィングエンジン
│  │  ├─ HarmonyEngine.*          # ハーモニー合成エンジン
│  │  ├─ PitchDetector.*          # オリジナル YIN リファレンス（未コンパイル）
│  │  └─ NoteUtils.h / IPitchShifter.h
│  ├─ ui/                         # UI コンポーネント
│  │  ├─ PitchCurveEditor.*       # カーブエディターコンポーネント
│  │  ├─ PitchVisualizer.*        # ピッチ可視化
│  │  ├─ PianoKeyboard.*          # ピアノウィジェット
│  │  ├─ ScaleKeyboardComponent.* # スケールキーボード表示
│  │  ├─ LookAndFeel.*            # カスタム Look and Feel
│  │  ├─ OVTTheme.h               # テーマカラーと共有波形レンダラー
│  │  ├─ OVTFonts.h               # フォントヘルパー
│  │  └─ OVTLanguages.h           # 多言語翻訳
│  ├─ external/presonus/          # PreSonus 拡張（Studio One）
│  ├─ resources/                  # バイナリリソース（BuildInfo.h.in）
│  ├─ PluginProcessor.*           # メインオーディオプロセッサー
│  └─ PluginEditor.*              # メインエディター UI
├─ scripts/                       # ビルド・開発スクリプト
│  ├─ build.ps1                   # Windows ビルド
│  ├─ build_installer.ps1         # Windows インストーラー（Inno Setup）
│  ├─ build_macos_vst3.sh         # macOS VST3 ビルド
│  ├─ build_macos_au.sh           # macOS AU ビルド
│  ├─ build_macos_pkg.sh          # macOS .pkg インストーラー
│  ├─ build_macos.sh              # macOS ユニバーサルビルド
│  └─ ...（インストール、シンボリックリンク、リリースヘルパー）
├─ test/                          # 単体テスト（Catch2）
│  ├─ Main.cpp
│  └─ dsp/                        # モジュールごとのテストスイート
├─ docs/                        # ドキュメント
│  ├─ releases/                   # リリースノート（latest.json、v0.1.1.md）
│  ├─ architecture.md
│  ├─ default-parameters.md
│  └─ ...
├─ installer/                     # Windows インストーラーリソース
│  └─ OpenVoxTuner.iss            # Inno Setup スクリプト
├─ .github/                       # CI/CD と Issue テンプレート
│  ├─ workflows/                  # GitHub Actions（CI、リリース）
│  └─ ISSUE_TEMPLATE/             # Bug report / Feature request
├─ assets/                        # バイナリリソース
│  └─ icon.png
├─ external/ARA_SDK/              # Celemony ARA SDK（v2.2、submodule）
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ docs/implementation-roadmap.md
├─ .gitignore
├─ .gitattributes
└─ .gitmodules
```

## ライセンス

OpenVoxTuner は [AGPLv3](../LICENSE) ライセンスの下で**すべての人に無料**です — ミュージシャン、プロデューサー、スタジオ、教育者。商用利用の制限はありません。

### サードパーティライセンス

| ライブラリ | ライセンス | 互換性 |
|---------|---------|--------------|
| JUCE 8 | AGPLv3 | 同一ライセンス |
| ARA SDK | Apache 2.0 | 完全互換 |
| PreSonus 拡張 | Public Domain | 完全互換 |
| Catch2（テスト） | Boost (BSL-1.0) | 完全互換 |

すべてのサードパーティライセンスは AGPLv3 と互換性があります。

### AGPLv3 があなたに意味するもの

| あなたは... | 無料？ | 義務？ |
|---|---|---|
| ミュージシャン / プロデューサー | はい | なし — ただ音楽を作るだけ |
| スタジオ（ミックス、マスタリング、制作） | はい | なし — プラグインをツールとして使用 |
| 教育者 / 学生 | はい | なし |
| 開発者（改変して再配布） | はい | 改変したソースを AGPLv3 で共有する義務 |
| 企業（クローズドソース製品へのフォーク） | いいえ | 商用ライセンスが必要 |

> 実際には、OpenVoxTuner を音楽制作（プロ利用含む）に使う分には、AGPLv3 ライセンスは義務なく完全に無料です。

### プロジェクトを支援する

OpenVoxTuner はすべての人に無料です。OpenVoxTuner があなたの時間を節約したり音楽を助けたりするなら、プロジェクトを支援することを検討してください — 小さな寄付でも大きな違いになります。

| ティア | プラットフォーム | 価格 | 特典 |
|------|----------|-------|----------|
| **無料** | — | 0 € | 全機能のプラグイン |
| **Buy me a coffee** | [Ko-fi](https://ko-fi.com/) | 単発 | ささやかな感謝 ❤️ |
| **Sponsor** | [GitHub Sponsors](https://github.com/sponsors/) | 月額 | 継続的な開発の支援 |
| **Supporter** | [Patreon](https://patreon.com/) | 5 €/月 | プライベート Discord + 今後の機能への投票 |
| **Gold** | [Patreon](https://patreon.com/) | 20 €/月 | Supporter のすべての特典 + 先行アクセス / ベータビルド + クレジット記載 |

すべての貢献が、プロジェクトを生かしすべての人に無料で提供し続ける助けになります。

### 開発者ライセンス

開発者や企業が OpenVoxTuner を AGPLv3 のコピーレフト義務なしで**クローズドソース製品**に統合したい場合のために、商用ライセンスが用意されています。

**付与されるもの：**
- 独自のソフトウェア内で OpenVoxTuner の DSP、UI コンポーネント、アルゴリズムを利用する許可
- コピーレフトの義務なし — ソースコードを公開する義務は**ありません**
- 派生版を AGPLv3 で公開する要件なし

**含まれるもの：**
- 優先的なメールサポート
- オプションのカスタム機能と DSP コンサルティング
- 購入したバージョンの永久ライセンス（ティアに応じたアップデート）

**連絡先：** GitHub で Issue を開くか、[license@openvoxtuner.com](mailto:license@openvoxtuner.com) まで。

## コントリビュート

コントリビュートを歓迎します！次の方法で協力できます：

- バグの修正
- DSP アルゴリズムの改善
- 翻訳の追加
- UI の改善
- ドキュメントの執筆
- DAW 互換性のテスト

詳細は [CONTRIBUTING.md](../CONTRIBUTING.md) を参照。

### AI 生成コードのレビュー

OpenVoxTuner の一部は AI コーディングエージェントの助けを借りて記述されていますが、常に人間の監督下です。すべてのコードはマージ前にレビューされ、コミュニティのプルリクエストは AI 支援セクションの監査・改善・置換のために大歓迎です。

## ビルド

### Windows（Visual Studio）

前提条件：
- Visual Studio 2022
- CMake >= 3.22
- JUCE 8（CMake のデフォルトパス：`C:/JUCE`）
- Git LFS（バイナリリソース用）

```powershell
# Debug ビルド
.\scripts\build.ps1 -Configuration Debug

# Release ビルド
.\scripts\build.ps1 -Configuration Release

# Windows インストーラーをビルド（Inno Setup が必要）
.\scripts\build_installer.ps1
```

### macOS（VST3 / AU / pkg）

前提条件：

```bash
xcode-select --install
brew install cmake ninja
```

VST3 をビルド：

```bash
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

AU をビルド：

```bash
./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install
```

macOS の `.pkg` インストーラーをビルド。公式リリースは **VST3 + Standalone** を配信します（AU は未署名のため DAW に読み込めないので除外）：

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

ローカルビルドに AU コンポーネントも含めるには、`--formats` に追加します（例：`VST3,AU,STANDALONE`）。

詳細なビルドガイド：
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## フォーマット

| フォーマット | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅*   |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

> \* AU コンポーネントはソースからビルド可能ですが、未署名のリリースには**同梱されません** — 未署名の AU は macOS 上の DAW に読み込めません。配布されるリリースには両プラットフォームとも **VST3 + Standalone** が含まれます。

## ドキュメント

- [docs/architecture.md](../docs/architecture.md) — ソフトウェアアーキテクチャの概要
- [docs/default-parameters.md](../docs/default-parameters.md) — すべてのプラグインパラメーターのリファレンス
- [docs/implementation-roadmap.md](../docs/implementation-roadmap.md) — 機能ロードマップ
- [docs/ARA_Specifications.md](../docs/ARA_Specifications.md) — ARA2 サポートの技術仕様
- [docs/deployment-and-packaging-guide.md](../docs/deployment-and-packaging-guide.md) — リリースワークフロー
- [docs/GITHUB_SETUP_AND_RELEASE.md](../docs/GITHUB_SETUP_AND_RELEASE.md) — GitHub リポジトリのセットアップ
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md) — macOS VST3 ビルドガイド
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md) — macOS AU + インストーラーガイド
- [docs/preset-morphing-technical-strategy.md](../docs/preset-morphing-technical-strategy.md) — A/B プリセットモーフィング戦略
- [docs/releases/v0.1.1.md](../docs/releases/v0.1.1.md) — リリースノート

## ライセンス

[LICENSE](../LICENSE) を参照。

## Issue 報告

GitHub の Issue テンプレートを使用してください：
- [Bug report](../.github/ISSUE_TEMPLATE/bug_report.md)
- [Feature request](../.github/ISSUE_TEMPLATE/feature_request.md)

## スター履歴

<a href="https://star-history.com/#EiffelBS/OpenVoxTuner&type=date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" width="100%" />
  </picture>
</a>
