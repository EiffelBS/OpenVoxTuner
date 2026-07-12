# OpenVoxTuner — Homebrew formula (builds from source, no code signing required)
#
# This formula compiles the plugin from source, so it does NOT need a Developer
# ID certificate and works for an open-source / free distribution. It produces:
#   - VST3  -> $(brew --prefix)/lib/VST3/OpenVoxTuner.vst3
#   - Standalone app -> $(brew --prefix)/OpenVoxTuner.app
#
# The Audio Unit (AU) is intentionally NOT built: an unsigned AU cannot be
# loaded by a DAW on modern macOS. Build/sign the AU separately if needed.
#
# Usage (this repo acts as a local tap):
#   brew install --HEAD ./Formula/openvoxtuner.rb
#
# Or, once this repo is PUBLIC, publish it as a tap (a repo named
# `homebrew-openvoxtuner`) containing this file, then:
#   brew tap EiffelBS/openvoxtuner
#   brew install --HEAD openvoxtuner
#
# NOTE: this is a `head`-only formula (it clones the repo with submodules, which
# is required because the ARA SDK is a git submodule). To add a `stable` block
# for a public tagged release, add e.g.:
#   url "https://github.com/EiffelBS/OpenVoxTuner/archive/refs/tags/v0.1.46.tar.gz"
#   sha256 "<tarball sha256>"
# and keep `submodules: true` so the ARA SDK is fetched.

class Openvoxtuner < Formula
  desc "Open-source autotune / pitch-correction audio plugin (VST3 + Standalone)"
  homepage "https://github.com/EiffelBS/OpenVoxTuner"
  license "AGPL-3.0-only"
  head "https://github.com/EiffelBS/OpenVoxTuner.git", branch: "main", submodules: true

  depends_on "cmake" => :build
  depends_on "ninja" => :build

  # JUCE 8.0.8 — the exact version this codebase compiles against.
  # (Newer JUCE removed Font::getStringWidth, which the source still uses.)
  resource "juce" do
    url "https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.8.tar.gz"
    sha256 "08abd711eb0345972974d589648f5c8829cb478bc513ddb34836664b0b36d152"
  end

  def install
    # Stage the JUCE source into ./JUCE (resource extracts to JUCE-8.0.8/).
    resource("juce").stage do
      (buildpath/"JUCE").install Dir["*"]
    end

    # Build for the host architecture only (fast, local install).
    arch = Hardware::CPU.arm? ? "arm64" : "x86_64"

    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DJUCE_PATH=#{buildpath}/JUCE",
           "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0",
           "-DCMAKE_OSX_ARCHITECTURES=#{arch}",
           "-DOVT_ENABLE_AU=OFF"

    system "cmake", "--build", "build", "--config", "Release", "--target", "OpenVoxTuner_VST3"
    system "cmake", "--build", "build", "--config", "Release", "--target", "OpenVoxTuner_Standalone"

    vst3 = "build/OpenVoxTuner_artefacts/Release/VST3/OpenVoxTuner.vst3"
    app  = "build/OpenVoxTuner_artefacts/Release/Standalone/OpenVoxTuner.app"

    (lib/"VST3").install vst3
    prefix.install app

    <<~EOS
      OpenVoxTuner has been installed to:
        #{opt_lib}/VST3/OpenVoxTuner.vst3        (link it to your DAW folder below)
        /Applications/OpenVoxTuner.app           (auto-linked by the postinstall step)

      To make the VST3 available to your DAW, symlink it:
        ln -s "#{opt_lib}/VST3/OpenVoxTuner.vst3" ~/Library/Audio/Plug-Ins/VST3/

      (If macOS flags the binaries as unsigned, allow them once with:
        xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3
        xattr -rd com.apple.quarantine /Applications/OpenVoxTuner.app)
    EOS
  end

  # Auto-link the Standalone app into /Applications (best-effort).
  #  - If /Applications/OpenVoxTuner.app is already our symlink: nothing to do.
  #  - If a stale real copy sits there (e.g. left by a .pkg install), replace it
  #    with the brew-managed symlink so future `brew upgrade` keeps it in sync.
  #  - Falls back to a manual instruction if write access is denied.
  def postinstall
    app = prefix/"OpenVoxTuner.app"
    return unless app.exist?

    destination = Pathname("/Applications/OpenVoxTuner.app")
    return if destination.symlink? && destination.readlink == app

    destination.rm_r if destination.exist?

    begin
      destination.make_symlink(app)
    rescue
      opoo "Auto-link to /Applications failed (insufficient permissions). " \
           "Link it manually: ln -s #{app} /Applications/"
    end
  end

  test do
    assert_path_exists lib/"VST3/OpenVoxTuner.vst3"
    assert_path_exists prefix/"OpenVoxTuner.app"
  end
end
