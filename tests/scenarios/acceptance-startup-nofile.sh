#!/usr/bin/env bash
# acceptance-startup-nofile.sh <editor exe>: inteng started with no file shows its message box in its own window ("Please load a rif file by double clicking on it", FUN_00447ed0 = FUN_00474030 +
# FUN_0057c800 + FUN_0057d2b0), and after Ok the program ends with exit code 0 (WinMain returns 0). A hidden run (-picture 1 -nofile) saves what the window shows as first-picture.png beside the exe on its
# third frame, says what the dialog holds, presses Ok itself on the fifth, and ends. Look at the picture: the box is in the middle of the window, on its own.
exe="${1:?editor exe}"
dir=$(dirname "$exe")
rm -f "$dir/first-picture.png"
out=$("$exe" -picture 1 -nofile 2>&1); code=$?
echo "$out"
[ "$code" = 0 ] || { echo "FAIL: exit code $code, not 0"; exit 1; }
echo "$out" | grep -q "message: Please load a rif file by double clicking on it (dialog kind message, buttons Ok " || { echo "FAIL: the message dialog is not up with its Ok"; exit 1; }
echo "$out" | grep -q "message answered: Ok" || { echo "FAIL: Ok did not end it"; exit 1; }
[ -s "$dir/first-picture.png" ] || { echo "FAIL: no picture of the window"; exit 1; }
echo "PASS: no file => the message in the window ($dir/first-picture.png), Ok, the program ends"
