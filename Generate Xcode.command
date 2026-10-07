#!/bin/zsh
set -e
cd "$(dirname "$0")"
export DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
CMAKE=$(command -v cmake || true)
[[ -n "$CMAKE" ]] || CMAKE=/opt/homebrew/bin/cmake
"$CMAKE" -S . -B Builds -G Xcode -DJUCE_PATH="${JUCE_PATH:-/Applications/JUCE}"
open Builds/SpectralStrip.xcodeproj
