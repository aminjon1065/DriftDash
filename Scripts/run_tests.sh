#!/bin/zsh
# Runs the automation tests headless and prints a summary.
#   Scripts/run_tests.sh                 # all DriftDash tests
#   Scripts/run_tests.sh DriftDash.Car
source "${0:A:h}/env.sh"
dd_check_fresh || exit 1
FILTER="${1:-DriftDash}"
LOG="$DD_ROOT/Saved/Logs/AutomationTests.log"
mkdir -p "$DD_ROOT/Saved/Logs"
"$UE_EDITOR_CMD" "$DD_PROJECT" -ExecCmds="Automation RunTests $FILTER; Quit" -unattended -nopause -nosplash -nullrhi -NoSound -stdout -FullStdOutLogOutput -TestExit="Automation Test Queue Empty" > "$LOG" 2>&1
CODE=$?
grep -E "Test Completed\. Result=|LogAutomationController: Error|Error: |DD_TEST" "$LOG" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | grep -v "^$"
PASS=$(grep -c "Result={Success}" "$LOG")
FAIL=$(grep -c "Result={Fail}" "$LOG")
echo "----"
echo "Passed: $PASS  Failed: $FAIL  (editor exit code $CODE, full log: $LOG)"
[[ $FAIL -eq 0 && $PASS -gt 0 ]]
