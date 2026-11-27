# AAX (Pro Tools) build

juicyboi builds an **unsigned** AAX plug-in on macOS (and Windows) from the same
CMake project as the other formats. Signing for retail Pro Tools is a separate
step with Avid/PACE's tools, after the build.

## What you need

- JUCE 9.x. JUCE bundles the Avid AAX SDK (2.9 in 9.0.5, 2.10 in 9.0.6) and
  compiles its library as part of the plug-in, so **no separate SDK download is
  required**. The default configure already adds AAX on macOS/Windows.
- Xcode or the Command Line Tools (`xcode-select --install`), CMake >= 3.22.
- Optional: your own Avid SDK (accepted licence, developer.avid.com/aax) with
  `-DJUICYBOI_AAX_SDK=/path/to/AAX_SDK`. The path must be the folder that
  contains `Interfaces/`, `Libs/` and `Utilities/`; configure stops with a clear
  message otherwise. Leave it unset to use JUCE's copy.
- `-DJUICYBOI_AAX=OFF` leaves AAX out of a macOS/Windows build.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=$HOME/JUCE
    cmake --build build --config Release --target juicyboi_AAX -j 8

Output: `build/juicyboi_artefacts/Release/AAX/juicyboi Rev3.aaxplugin`
(arm64 on Apple silicon, x86_64 on Intel; add
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for a universal binary).

## Identity

| Field | Value |
|-------|-------|
| AAX manufacturer ID / AU manufacturer | `Jcyb` (`PLUGIN_MANUFACTURER_CODE`) |
| AAX product ID / AU subtype / VST3 | `Jcb3` (`PLUGIN_CODE`) |
| AAX identifier | `com.juicyboi.juicyboi-rev3` |
| Pro Tools menu category | Harmonic |
| Channels | mono in -> mono or stereo out, stereo -> stereo |

The manufacturer code must contain an upper-case letter for AU, so an
all-numeric code is not used. The code is not part of PACE signing: `wraptool`
signs a finished bundle using your iLok account and a PACE-issued `wcguid`.

## Signing (preview; follow PACE's own instructions when they arrive)

    wraptool sign --verbose --account <ilok-account> --wcguid <wcguid> \
      --signid 1 --in "juicyboi Rev3.aaxplugin" --out "juicyboi Rev3.signed.aaxplugin"

Then copy the signed bundle to
`/Library/Application Support/Avid/Audio/Plug-Ins/`. Unsigned builds load only in
Pro Tools Developer.
