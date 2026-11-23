#!/bin/zsh
# Runs an editor Python script headless: Scripts/run_editor_python.sh Scripts/build_content.py
source "${0:A:h}/env.sh"
SCRIPT="${1:A}"
"$UE_EDITOR_CMD" "$DD_PROJECT" -run=pythonscript -script="$SCRIPT" -unattended -nopause -nosplash -nullrhi -stdout -FullStdOutLogOutput 2>&1 | grep -E "DD_|Error|LogPython: Error|Traceback|Exception"
exit ${pipestatus[1]}
