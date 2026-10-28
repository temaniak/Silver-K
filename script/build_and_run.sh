#!/usr/bin/env bash
set -euo pipefail

# Local JUCE development loop. Hosts and installed plugin bundles are untouched.
silver_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
silver_build="${SILVER_K_BUILD_DIR:-$silver_root/build}"
silver_mode="${1:-run}"
case "$silver_mode" in
  run|--debug|--logs|--telemetry|--verify) ;;
  *) echo "usage: $0 [run|--debug|--logs|--telemetry|--verify]" >&2; exit 2 ;;
esac
if [[ ! -f "$silver_build/CMakeCache.txt" ]]; then
  cmake -S "$silver_root" -B "$silver_build" -DCMAKE_BUILD_TYPE=Release
fi
silver_app="$silver_build/source/novation/silverK/buildStandalone/silverPlugin/silverK_artefacts/Release/Standalone/Silver-K.app"
if [[ -d "$silver_build/silverPlugin" ]]; then
  silver_app="$silver_build/silverPlugin/silverK_artefacts/Release/Standalone/Silver-K.app"
fi
silver_binary="$silver_app/Contents/MacOS/Silver-K"
# Match the exact development executable, never a DAW or another installed app.
while IFS= read -r silver_pid; do
  [[ -z "$silver_pid" ]] || kill -TERM "$silver_pid"
done < <(pgrep -f "^${silver_binary}$" || true)
cmake --build "$silver_build" --config Release --target silverK_Standalone -j "${SILVER_K_BUILD_JOBS:-6}"
if [[ "$silver_mode" == --debug ]]; then exec lldb -- "$silver_binary"; fi
open -n "$silver_app"
case "$silver_mode" in
  --verify) sleep 2; pgrep -f "^${silver_binary}$" >/dev/null ;;
  --logs|--telemetry) exec /usr/bin/log stream --info --style compact --predicate 'process == "Silver-K"' ;;
esac
