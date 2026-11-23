#!/bin/zsh
# Compiles the editor module (needed after any C++ change). -NoHotReload: safe with an Unreal Editor open.
source "${0:A:h}/env.sh"
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DriftDashEditor Mac Development -Project="$DD_PROJECT" -WaitMutex -NoHotReload
