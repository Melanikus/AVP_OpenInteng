#!/usr/bin/env bash
# acceptance-save-copy-documents.sh <editor exe>: Melanikus 2026-11-16 ~11:2x (through Bishop): OpenInteng is installed in the game's own Tools folder, as inteng was (<AvP>\Tools\), so the
# program's folder is inside the game's; Save a copy writes to <Documents>\AVP_OpenInteng\edited\ and never inside the game's folder (save_copy.h). This run puts the editor in a FAKE game
# folder's Tools folder (the exe, its DLLs and its ini with DataFolder = the fake game folder, whose name has a space as "Aliens versus Predator Classic" has), opens Invasion from that
# folder's avp_rifs as a double click does, and does Save a copy (the harness's save-copy: File > Save a copy) with a TEST Documents folder (-documents, taken only by a hidden -picture
# run, so a test never writes into the user's own Documents). PASS: the copy is <test Documents>\AVP_OpenInteng\edited\Invasion-edited.rif, the game's loader took it (the editor checks
# every copy with it before it is put in place), and no .rif and no "edited" folder was made anywhere in the fake game folder.
# The harness: $DRIVE, else AvP-builds-excluded\bishop-harness-out\drive_scenario.exe. Written 2026-11-16 by Melanikus (Dietrich).
set -u
exe="${1:?editor exe}"
X=/c/Users/Melanikus/source/AvP-builds-excluded
drive="${DRIVE:-$X/bishop-harness-out/drive_scenario.exe}"
here=$(dirname "$exe")
work="$X/dietrich-harness-out/save-copy-documents"
game="$work/fake game"
docs="$work/test documents"
[ -f "$drive" ] || { echo "FAIL: no harness at $drive"; exit 1; }
data=$(grep -m1 '^DataFolder=' "$here/AVP_OpenInteng.ini" | sed 's/^DataFolder=//; s/\r$//')
[ -n "$data" ] || { echo "FAIL: no DataFolder= in $here/AVP_OpenInteng.ini"; exit 1; }
src=$(cygpath -u "$data")

# The fake game folder: the game's data without its levels, and Invasion (made once and kept: about 65 MB)
if [ ! -f "$game/avp_rifs/Invasion.RIF" ]; then
	mkdir -p "$game/avp_rifs"
	for p in "$src"/*; do [ "$(basename "$p")" = avp_rifs ] || cp -r "$p" "$game/"; done
	cp "$src/avp_rifs/Invasion.RIF" "$game/avp_rifs/" || { echo "FAIL: no Invasion.RIF in $src/avp_rifs"; exit 1; }
fi

# The program in the fake game's Tools folder, as it is installed now
rm -rf "$game/Tools" "$docs" "$work/record"
mkdir -p "$game/Tools" "$docs" "$work/record"
cp "$exe" "$here"/*.dll "$game/Tools/" || { echo "FAIL: the program and its DLLs could not be copied"; exit 1; }
gw=$(cygpath -w "$game")
gwe=$(printf '%s' "$gw" | sed 's/\\/\\\\/g')
grep -v -e '^CameraSpeed=' -e '^KeyLayout=' "$here/AVP_OpenInteng.ini" | sed "s|^DataFolder=.*|DataFolder=$gwe|; s|^Level=.*|Level=Invasion|" > "$game/Tools/AVP_OpenInteng.ini"
grep -q "^DataFolder=$gwe\$" "$game/Tools/AVP_OpenInteng.ini" || { echo "FAIL: the ini's DataFolder is not the fake game folder"; cat "$game/Tools/AVP_OpenInteng.ini"; exit 1; }

marker="$work/marker"; : > "$marker"; sleep 1
dw=$(cygpath -w "$docs")
scenario="$work/save-copy-documents.txt"
printf '%s\n' "# written by acceptance-save-copy-documents.sh" "start-args \"$gw\\avp_rifs\\Invasion.RIF\" -documents \"$dw\"" "pictures off" "state" "save-copy" > "$scenario"
out=$("$drive" "$(cygpath -w "$game/Tools/AVP_OpenInteng.exe")" "$(cygpath -w "$work/record")" "$(cygpath -w "$scenario")" 2>&1); code=$?
echo "$out" | tail -12

want="$dw\\AVP_OpenInteng\\edited\\Invasion-edited.rif"
fail=0
echo "$out" | grep -q "^  FAILED" && { echo "FAIL: the run had a failed step"; fail=1; }
# the harness keeps each save under a stage name beside it (Invasion-edited-stage1.rif) and says that name; the copy itself is checked just below
echo "$out" | grep -qF -- "save-copy -> $dw\\AVP_OpenInteng\\edited\\Invasion-edited" || { printf 'FAIL: the copy was not saved in %s\n' "$dw\\AVP_OpenInteng\\edited"; fail=1; }
[ -s "$docs/AVP_OpenInteng/edited/Invasion-edited.rif" ] || { echo "FAIL: no copy at $docs/AVP_OpenInteng/edited/Invasion-edited.rif"; fail=1; }
made=$(find "$game" -newer "$marker" \( -iname '*.rif' -o -iname 'edited' \) 2>/dev/null)
[ -z "$made" ] || { echo "FAIL: written inside the fake game folder: $made"; fail=1; }
[ "$code" = 0 ] || { echo "FAIL: the harness ended with $code"; fail=1; }
[ "$fail" = 0 ] || exit 1
echo "PASS: the program in <game>\\Tools saved its copy to $want; nothing .rif was written inside the game folder"
