#!/usr/bin/env bash
# acceptance-fresh-journey.sh <editor exe>: A NEW PLAYER'S JOURNEY, END TO END (Vasquez 2026-11-24, Bishop's order for beta.0; R-INT-17: "the fresh-install path
# ... tested as a new player meets it"; the owner installs beta.0 as a fresh player: both shape_rifs folders moved away, [ShapesScanned] taken out of his ini).
# Each step has its own acceptance (acceptance-first-launch.sh: the zip's ini in <game>\Tools; acceptance-shapes-found-1/2: the FOUND form and the search;
# acceptance-shape-picker-letgo: the picker on his path; mapper-journey: placing through the picker; acceptance-hub-save: a hidden run's save), but none runs
# them as ONE CHAIN from the zip's own ini (run-shards always writes its own). This does:
#   the editor alone in a fake game's Tools folder with the release zip's AVP_OpenInteng.ini (dist/, no DataFolder=, no list) and the one line a hidden run
#   needs to show the FOUND form (ShapesInTests=1, read only in a hidden run), and NO shape_rifs anywhere in the game; then the shard's own drive_scenario
#   drives fresh-journey/steps.txt: Genshd1 double-clicked, the FOUND form's Yes and the search, Object Placement, the picker filled from what the search
#   made, a pickup's shape (medbox01) and a static shape placed and named, the mode left (Object Placement's Exit saves at once: no question), the level opened again from
#   that file, and both objects still there (verify_objects on a copy saved after the reopen).
# The fake game is made once in the work folder and kept there (its folders from $GAME_FROM, the game as Steam ships it, without avp_rifs, FMVs, obj, bin, MelPyth, licences, shape_rifs or
# Tools; Genshd1.RIF alone in avp_rifs); before each run the editor's own folders, the shape_rifs the last run made, and avp_rifs\OpenedByEditor are taken away.
# No DLL is copied: OpenAL32.dll is found on PATH in $DLLS_FROM (our own vcpkg build), as acceptance-first-launch.sh does.
set -u
exe="${1:?editor exe}"
X=/c/Users/Melanikus/source/AvP-builds-excluded
tree=$(cd "$(dirname "$0")/../.." && pwd)
from="${GAME_FROM:-$X/AvP-play-release}"
dlls="${DLLS_FROM:-$X/libs-31c9b73c/bin}"
harness="${HARNESS:-$(dirname "$(dirname "$(dirname "$exe")")")/drive_scenario.exe}"   # run-shards puts the shard's harness three folders above the exe
# the work folder is the SHARD'S OWN (beside its harness), never one shared folder: two sweeps at once would delete and fill each other's fake game
# ("one acceptance runner at a time": shared run folders corrupt both runs); run-shards deletes the shard, and it, at the end
work="${FRESH_JOURNEY_WORK:-$(dirname "$harness")/fresh-journey}"
game="$work/fake game"
tools="$game/Tools"
zipini="$tree/dist/AVP_OpenInteng.ini"
steps="$tree/tests/scenarios/fresh-journey/steps.txt"   # (in a folder of its own: a full sweep runs every tests/scenarios/*.txt, and these two run only here)
fail=0
ok() { echo "  ok  $*"; }
bad() { echo "  FAILED  $*"; fail=1; }

# THE WORK FOLDER IS DELETED IN PARTS, so it must be inside AvP-builds-excluded FOR REAL (acceptance-first-launch.sh's rule: no "..", no backslash, and each
# folder's real place, links followed, under the real AvP-builds-excluded before anything is made or deleted)
case "$work" in "$X"/*) ;; *) echo "FAIL: the work folder has to be inside AvP-builds-excluded: $work"; exit 1 ;; esac
case "$work" in *..*|*\\*) echo "FAIL: the work folder has a \"..\" or a backslash in it: $work (nothing deleted)"; exit 1 ;; esac
realX=$(cd "$X" && pwd -P) || { echo "FAIL: AvP-builds-excluded is not there: $X"; exit 1; }
real_inside() {
	local at=$1 real
	while [ ! -e "$at" ] && [ "$at" != "/" ] && [ "$at" != "." ]; do at=$(dirname "$at"); done
	real=$(cd "$at" 2>/dev/null && pwd -P)
	case "$real" in
		"$realX"/*) return 0 ;;
		"$realX") [ "$at" != "$1" ] && return 0 ;;
	esac
	echo "FAIL: $1 is not inside AvP-builds-excluded once its links are followed (it is under ${real:-nowhere}): nothing made or deleted"; exit 1
}
real_inside "$work" && mkdir -p "$work" && real_inside "$work"
[ -f "$exe" ] || { echo "FAIL: no editor at $exe"; exit 1; }
[ -f "$harness" ] || { echo "FAIL: no drive_scenario at $harness (HARNESS=<its path> to name one)"; exit 1; }
[ -f "$zipini" ] || { echo "FAIL: no zip ini at $zipini"; exit 1; }
[ -f "$steps" ] || { echo "FAIL: no steps file at $steps"; exit 1; }
[ -f "$dlls/OpenAL32.dll" ] || { echo "FAIL: no OpenAL32.dll in $dlls"; exit 1; }
grep -qi '^DataFolder=\|^Level=\|^\[ShapesScanned\]' "$zipini" && { echo "FAIL: the zip's ini names a DataFolder=, a Level= or a list: it is not a new player's file"; exit 1; }

# The fake game folder, made once and kept: no shape_rifs and no Tools come from the copy
if [ ! -f "$game/avp_rifs/Genshd1.RIF" ] || [ ! -f "$game/made.txt" ]; then
	real_inside "$game"
	rm -rf "$game"; mkdir -p "$game/avp_rifs"
	for p in "$from"/*; do
		case "$(basename "$p")" in avp_rifs|FMVs|obj|bin|MelPyth|licences|shape_rifs|Tools|*.md|*.txt) ;; *) cp -r "$p" "$game/" || { echo "FAIL: copying $p"; exit 1; } ;; esac
	done
	cp "$from/language.txt" "$game/" 2>/dev/null
	cp "$from/avp_rifs/Genshd1.RIF" "$game/avp_rifs/" || { echo "FAIL: no Genshd1.RIF in $from/avp_rifs"; exit 1; }
	date > "$game/made.txt"
fi

# A NEW PLAYER'S STATE before each run: the editor alone in Tools with the zip's ini (+ ShapesInTests=1), no shape_rifs anywhere, no copy of the level made before
docs="$work/documents"   # the run's own Documents folder, OUTSIDE the game, as a player's is (a hidden run's own would be beside the program: inside the game)
for d in "$tools" "$game/shape_rifs" "$game/avp_rifs/OpenedByEditor" "$docs"; do real_inside "$d"; rm -rf "$d"; done
mkdir -p "$docs"
mkdir -p "$tools"
cp "$exe" "$tools/AVP_OpenInteng.exe" || { echo "FAIL: the program could not be copied to $tools"; exit 1; }
cp "$zipini" "$tools/AVP_OpenInteng.ini"
printf '%s\n' "ShapesInTests=1" >> "$tools/AVP_OpenInteng.ini"   # (the zip's ini ends in its [Editor] section: a hidden run asks the FOUND form only with it)
[ ! -e "$game/shape_rifs" ] && [ ! -e "$tools/shape_rifs" ] && ok "a new player's game: no shape_rifs anywhere, the zip's ini beside the editor in Tools" ||
	bad "a shape_rifs is still there before the run"

# the steps, with the fake game's folder written in: forward slashes (C:/...), which a sed replacement never reads as escapes (a backslash path's "\U" made
# the first run's "C:SERS...": GNU sed's upper-case escape); the steps quote it, as the folder's name has a space, as Steam's "Aliens versus Predator Classic" has
gm=$(cygpath -m "$game"); dm=$(cygpath -m "$docs")
reopen="$tree/tests/scenarios/fresh-journey/reopen.txt"
[ -f "$reopen" ] || { echo "FAIL: no second start's steps at $reopen"; exit 1; }
sed -e "s|{game}|$gm|g" -e "s|{docs}|$dm|g" "$steps" > "$work/steps.txt"
sed -e "s|{game}|$gm|g" -e "s|{docs}|$dm|g" "$reopen" > "$work/reopen.txt"

# drive <steps> <record name> <what>: the shard's harness drives the editor in <game>\Tools through those steps, hidden; its whole output kept as <record name>.log
drive() {
	local rec="$work/$2"
	real_inside "$rec"; rm -rf "$rec"; mkdir -p "$rec"
	echo "== $3: $(grep -c '^[a-z]' "$1") steps, hidden, from the zip's ini in <game>\\Tools"
	out=$(cd "$tools" && PATH="$dlls:$PATH" timeout 1200 "$harness" "$(cygpath -w "$tools/AVP_OpenInteng.exe")" "$(cygpath -w "$rec")" "$(cygpath -w "$1")" 2>&1); code=$?
	printf '%s\n' "$out" > "$work/$2.log"
	echo "$out" | grep -E '^  (ok|FAILED|NOTE|ALLOWED)  ' | cut -c1-220 | sed 's/^/  /'
	echo "$out" | grep -q "^All checks passed" && [ "$code" = 0 ] && ok "$3: every step passed" || bad "$3 failed (the harness ended with $code; its log: $work/$2.log)"
}

drive "$work/steps.txt" journey "the first start: the form, the search, the picker, two objects placed, saved"

# what the first start left: the shapes the search made, the list in the ini, the saved level
n=$(ls "$game/shape_rifs"/*.rif 2>/dev/null | wc -l)
[ "$n" -gt 0 ] && ok "the search made $n shape files in the game's shape_rifs" || bad "no shape file in the game's shape_rifs"
grep -q '^\[ShapesScanned\]' "$tools/AVP_OpenInteng.ini" && ok "the ini now holds the list of maps searched ([ShapesScanned])" || bad "no [ShapesScanned] list in the ini"
saved="$docs/AVP_OpenInteng/edited/Genshd1-edited.rif"
[ -s "$saved" ] && ok "the level was saved (a hidden run's stand-in: $saved)" || bad "no saved level at $saved"

# the player starts the editor again on what was saved: both objects there (no form now: the list says the map was searched)
[ -s "$saved" ] && drive "$work/reopen.txt" reopen "the second start, on the saved level"

[ $fail = 0 ] && { echo "acceptance-fresh-journey: all passed"; exit 0; }
echo "acceptance-fresh-journey: FAILED"; exit 1
