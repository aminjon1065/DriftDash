#!/bin/zsh
# Plays the game in a phone-shaped window using the editor binary (no packaging needed).
#   Scripts/play_mac.sh [WIDTHxHEIGHT] [extra args...]   default 500x960
# Controls: A/Left, D/Right (or hold the left / right half with the mouse), Space/Shift = handbrake, R = restart.
source "${0:A:h}/env.sh"
dd_check_fresh || exit 1
RES="${1:-500x960}"
shift 2>/dev/null
"$UE_EDITOR" "$DD_PROJECT" -game -windowed -ResX="${RES%x*}" -ResY="${RES#*x}" -NoSound "$@" &
