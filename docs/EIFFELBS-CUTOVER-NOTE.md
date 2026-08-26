# OpenVoxTuner × eiffelbs-ui cutover notes

OpenVoxTuner now consumes the shared EiffelBS design system
([eiffelbs-ui](https://github.com/EiffelBS/eiffelbs-ui)) version **v0.3.0**
through CMake `FetchContent`, exactly like OpenTimbre. There are no local
shims: the plugin links the library and calls `ebs::` directly.

## What the link provides

`target_link_libraries(... eiffelbs-ui)` carries onto every consumer target:

- the ABI-critical JUCE defines shared by BOTH plugin binaries inside one
  DAW process (`OpenVoxTuner` **and** its companion `OpenVoxKey`):
  `JUCE_STRING_UTF_TYPE=8`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`;
- `/std:c++20` (library `cxx_std_20`) and MSVC `/utf-8`.

## Offline / vendored builds

Point FetchContent at a local checkout instead of GitHub:

```sh
cmake -B build -DFETCHCONTENT_SOURCE_DIR_EIFFELBS-UI=C:\path\to\eiffelbs-ui
```

The variable name keeps the HYPHEN of the declared dependency name -
this is intentional and is a recurring typo trap.

## Migration shape

| Legacy | Now |
| --- | --- |
| `ovt::` colour/font helpers (~200 references) | `ebs::` equivalents (byte-identical palette, proven by the library smoke suite) |
| `ui::OVTLookAndFeel` (456-line LookAndFeel) | deleted; replaced by ~20-line local subclass `OvtLookAndFeel : ebs::LookAndFeel` |
| Magic-name power buttons (`setName("PowerButton")`) | typed `ebs::PowerToggle` widgets (8 live instances) |
| Name-keyed Morph painting in the L&F | self-painting `ebs::MorphSlider` |
| 23 rotary sliders | `ebs::Knob` (per-site style setters still win where text boxes are used) |
| 11 zero-use theme tokens | deleted (`inputColour`, `harmonyColour`, `piano*`, `headerBg/headerAccent`, `fontTarget`, `fontCents`, `fontCurveHelp`) |

### Kept deliberately local

- i18n system (`ovt::tr` / `ovt::Keys` / languages) - app concern;
- `WaveformDisplayType` + `drawWaveformOverlay` - depends on `juce_dsp` FFT,
  too heavy for a design-system header;
- app-specific font sizes in `OVTFonts.h`;
- widgets that are product UI rather than design-system surface
  (PresetGallery, Piano/Scale keyboards, Curve/Pitch visualisers).

## Pixel-parity strategy

Rendering must be identical to the pre-cutover build. The only legacy
deltas versus the library defaults were restored through the official
theme hook - no painter forks:

```cpp
struct OvtLookAndFeel final : ebs::LookAndFeel
{
    juce::Colour widgetThemeColour (int id) override
    {
        switch (id)
        {
            case tabActiveFillColourId:   return ebs::accent();             // solid pill vs soft tint
            case tabActiveTextColourId:   return juce::Colours::white;
            case checkboxFillColourId:    return juce::Colour (0xff191b1e); // always-dark well
            case checkboxOutlineColourId: return juce::Colour (0xff555555);
            default:                      return {};
        }
    }
};
```

Everything else inherits stock `ebs::LookAndFeel` painting.

## Verification record

- Release builds: `OpenVoxTuner` (VST3/Standalone) and `OpenVoxKey` clean.
- Unit tests: `OpenVoxTunerTests` 111/111 PASS after migration.
- Standalone capture (`PrintWindow`): active Live tab shows the legacy
  solid-accent fill with white caption (exact `#FF1A9AF0` pixels), the
  hook path working end-to-end in the real binary.

## Release gate (do not forget)

The eiffelbs-ui repository must be flipped **PUBLIC** before any public
binary release of OpenVoxTuner that consumes it: external FetchContent
clones and AGPL source availability both depend on it.
