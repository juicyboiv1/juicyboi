#!/usr/bin/env bash
# Mac Mini / macOS recovery when juicyboi Standalone is silent with a Focusrite
# Scarlett even though device routing "looks" correct.
#
# What this does:
#   1) Quits any running juicyboi Standalone
#   2) Deletes the Standalone settings file (device routing + Mute + plugin state)
#   3) Verifies / re-applies the JUCE Standalone unmute + mono-input patches
#   4) Reconfigures + rebuilds Release Standalone
#   5) Opens the new binary
#
# Usage (from anywhere):
#   bash ~/src/juicyboi/scripts/recover-standalone-mac.sh
#
# Env overrides:
#   JUICYBOI_DIR   default: ~/src/juicyboi
#   JUCE_DIR       default: ~/src/JUCE
set -euo pipefail

JUICYBOI_DIR="${JUICYBOI_DIR:-$HOME/src/juicyboi}"
JUCE_DIR="${JUCE_DIR:-$HOME/src/JUCE}"
SETTINGS="$HOME/Library/Application Support/juicyboi.settings"
# Older JUCE advice put settings under Preferences — clear both to be safe.
SETTINGS_PREFS="$HOME/Library/Preferences/juicyboi.settings"
HEADER="$JUCE_DIR/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h"
APP="$JUICYBOI_DIR/build/juicyboi_artefacts/Release/Standalone/juicyboi.app"

echo "== juicyboi Standalone recovery =="
echo "  repo:     $JUICYBOI_DIR"
echo "  JUCE:     $JUCE_DIR"
echo "  settings: $SETTINGS"

if [[ ! -d "$JUICYBOI_DIR/.git" ]]; then
  echo "ERROR: $JUICYBOI_DIR is not a juicyboi git checkout" >&2
  exit 1
fi
if [[ ! -f "$HEADER" ]]; then
  echo "ERROR: JUCE Standalone header not found: $HEADER" >&2
  echo "  Set JUCE_DIR to your JUCE 9 tree (expected ~/src/JUCE)." >&2
  exit 1
fi

echo "-- quitting juicyboi (if running)"
osascript -e 'tell application "juicyboi" to quit' >/dev/null 2>&1 || true
pkill -x juicyboi >/dev/null 2>&1 || true
sleep 1

echo "-- removing Standalone settings (forces fresh device + unmute defaults)"
rm -f "$SETTINGS" "$SETTINGS_PREFS"
# Some macOS versions nest the file under a juicyboi folder
rm -f "$HOME/Library/Application Support/juicyboi/juicyboi.settings" 2>/dev/null || true

echo "-- pulling latest main"
cd "$JUICYBOI_DIR"
git pull --ff-only origin main

echo "-- checking JUCE unmute markers (before configure)"
if grep -q 'juicyboiMuteInput' "$HEADER" && grep -q 'muteInput { false }' "$HEADER"; then
  echo "   header already has unmute markers"
else
  echo "   header missing unmute markers — cmake configure will patch (or FATAL)"
fi

echo "-- reconfigure (applies JUCE Standalone patches) + rebuild"
# Always reconfigure so PatchJuceStandaloneMonoInputs.cmake runs, even if
# ninja would otherwise skip it. JUCE_DIR must match the tree you compile against.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR="$JUCE_DIR"
cmake --build build -j8 --target juicyboi_Standalone

if [[ ! -d "$APP" ]]; then
  echo "ERROR: Standalone app missing after build: $APP" >&2
  exit 1
fi

echo "-- verifying built-tree header still has unmute markers"
if ! grep -q 'juicyboiMuteInput' "$HEADER" || ! grep -q 'muteInput { false }' "$HEADER"; then
  echo "ERROR: $HEADER still lacks unmute markers after configure." >&2
  echo "  Standalone would boot with Mute audio input ON (silent guitar)." >&2
  exit 1
fi

echo "-- opening $APP"
open "$APP"

cat << 'TIP'

After launch — Audio/MIDI Settings checklist:
  1. Audio device = Scarlett / Focusrite (input AND output)
  2. Tick Input 1 (guitar INST jack) — mono checkbox, not only a stereo pair
  3. Outputs 1+2 enabled
  4. "Mute audio input" must be UNTICKED
  5. No yellow "Audio input is muted..." banner (if present: Settings → unmute)
  6. Scarlett Direct Monitor OFF (so you hear Playback / software, not dry DI)
  7. System Settings → Privacy & Security → Microphone → juicyboi allowed

Settings keys (inside juicyboi.settings): audioSetup, juicyboiMuteInput, filterState.
Deleted above so the next launch starts unmuted with fresh device state.
TIP
