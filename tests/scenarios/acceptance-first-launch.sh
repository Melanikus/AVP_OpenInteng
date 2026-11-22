#!/usr/bin/env bash
# acceptance-first-launch.sh <editor exe>: THE FIRST START WORKS OUT OF THE BOX (Drake 2026-11-22, alpha.5; Bishop 2's order "FIRST LAUNCH MUST WORK OUT OF THE BOX": a fresh
# install stopped at once with "AVP_OpenInteng.ini beside the program needs [Editor] with DataFolder=", as the release zip's ini names no data folder).
# The editor goes in a FAKE game folder's Tools folder ("fake game\Tools", with a space as "Aliens versus Predator Classic" has), with the release zip's AVP_OpenInteng.ini
# beside it byte for byte (dist/AVP_OpenInteng.ini of this script's tree: no DataFolder=, no Level=), and is started hidden (-picture), five ways:
#  1. a level given, as a double-click on a .rif gives it: it starts, opens Genshd1 (the game counts its modules), saves its picture and ends with 0; its log says the data
#     folder is the fake game folder, the one above the editor's
#  2. no file (-nofile: the start someone at the screen gets): inteng's own message "Please load a rif file by double clicking on it", Ok, and it ends with 0
#  3. a DataFolder= written in the ini, naming a folder that isn't there: it stops, saying "DataFolder in AVP_OpenInteng.ini can't be used: there's no folder there",
#     and never takes the game's folder instead (a written line always wins)
#  4. a DataFolder= of 300 letters, too long to read: it stops, saying it is too long, and never takes the game's folder instead
#  5. the editor anywhere else (its folder's folder above holds no avp_rifs), with the zip's ini: it stops, saying how to fix it (the game's Tools folder, or DataFolder=)
# RED on 608dd333 (exe-cache/608dd333.exe): 1, 2, 4 and 5 stop with "needs [Editor] with DataFolder=". 3 is green there too: it proves the new default never covers a
# written line. The game's files come from $GAME_FROM (default AvP-builds-excluded\AvP-play-release, the game as Steam ships it): its folders but FMVs, obj, bin and the
# levels, and Genshd1.RIF, made once and kept (about 105 MB). No DLL is copied (AvP-builds-excluded holds only our builds and the game's files, the owner 11-22):
# Microsoft's D3DX9 and C++ runtime are Windows' own (SysWOW64), and the OpenAL32.dll the zip puts beside the editor is found on the program's PATH, in $DLLS_FROM
# (default AvP-builds-excluded\libs-31c9b73c\bin: our own vcpkg build of it, the one the editor is built against; Bishop 2 checked its Rich header against the
# CI-built one alpha.4 shipped, 11-22). Written 2026-11-22 by Melanikus (Drake).
set -u
exe="${1:?editor exe}"
X=/c/Users/Melanikus/source/AvP-builds-excluded
tree=$(cd "$(dirname "$0")/../.." && pwd)
from="${GAME_FROM:-$X/AvP-play-release}"
dlls="${DLLS_FROM:-$X/libs-31c9b73c/bin}"
work="${FIRST_LAUNCH_WORK:-$X/drake-harness-out/first-launch}"
game="$work/fake game"
tools="$game/Tools"
elsewhere="$work/elsewhere"
zipini="$tree/dist/AVP_OpenInteng.ini"
fail=0
ok() { echo "  ok  $*"; }
bad() { echo "  FAILED  $*"; fail=1; }

# THE WORK FOLDER IS DELETED IN PARTS (the fake game when it is made again, the program's folders before each case), so it must be inside AvP-builds-excluded FOR REAL
# (Vasquez's review, Bishop 2 11-22): written under it, with no ".." and no backslash anywhere in it (FIRST_LAUNCH_WORK=$X/../x would reach outside); and, before
# anything is made or deleted, each folder's real place (its links and junctions followed; for one not made yet, the nearest folder above it that is there) under the
# real AvP-builds-excluded. Anything else stops the run, saying so, with nothing made or deleted
case "$work" in "$X"/*) ;; *) echo "FAIL: the work folder has to be inside AvP-builds-excluded: $work"; exit 1 ;; esac
case "$work" in *..*|*\\*) echo "FAIL: the work folder has a \"..\" or a backslash in it, so it could reach outside AvP-builds-excluded: $work (nothing deleted)"; exit 1 ;; esac
realX=$(cd "$X" && pwd -P) || { echo "FAIL: AvP-builds-excluded is not there: $X"; exit 1; }
real_inside() {
	local at=$1 real
	while [ ! -e "$at" ] && [ "$at" != "/" ] && [ "$at" != "." ]; do at=$(dirname "$at"); done
	real=$(cd "$at" 2>/dev/null && pwd -P)
	case "$real" in
		"$realX"/*) return 0 ;;                       # it, or the nearest folder above it, is inside
		"$realX") [ "$at" != "$1" ] && return 0 ;;    # not made yet, right under AvP-builds-excluded (never AvP-builds-excluded itself)
	esac
	echo "FAIL: $1 is not inside AvP-builds-excluded once its links are followed (it is under ${real:-nowhere}): nothing made or deleted"; exit 1
}
real_inside "$work" && mkdir -p "$work" && real_inside "$work"
[ -f "$exe" ] || { echo "FAIL: no editor at $exe"; exit 1; }
[ -f "$zipini" ] || { echo "FAIL: no zip ini at $zipini"; exit 1; }
[ -f "$dlls/OpenAL32.dll" ] || { echo "FAIL: no OpenAL32.dll in $dlls"; exit 1; }
grep -qi '^DataFolder=\|^Level=' "$zipini" && { echo "FAIL: the zip's ini names a DataFolder= or a Level=: it is not the out-of-the-box file"; exit 1; }

# The fake game folder, made once and kept
if [ ! -f "$game/avp_rifs/Genshd1.RIF" ] || [ ! -f "$game/made.txt" ]; then
	real_inside "$game"
	rm -rf "$game"; mkdir -p "$game/avp_rifs"
	for p in "$from"/*; do
		case "$(basename "$p")" in avp_rifs|FMVs|obj|bin|MelPyth|licences|*.md|*.txt) ;; *) cp -r "$p" "$game/" || { echo "FAIL: copying $p"; exit 1; } ;; esac
	done
	cp "$from/language.txt" "$game/" 2>/dev/null
	cp "$from/avp_rifs/Genshd1.RIF" "$game/avp_rifs/" || { echo "FAIL: no Genshd1.RIF in $from/avp_rifs"; exit 1; }
	date > "$game/made.txt"
fi
[ ! -e "$work/avp_rifs" ] || { echo "FAIL: $work holds an avp_rifs, so case 5 would find a game there"; exit 1; }
gw=$(cygpath -w "$game")

# put <folder> [extra ini line]: the program alone in <folder>, with the zip's ini (and the line after it), and no settings, log or picture from before
put() {
	real_inside "$1"
	rm -rf "$1"; mkdir -p "$1"
	cp "$exe" "$1/AVP_OpenInteng.exe" || { echo "FAIL: the program could not be copied to $1"; exit 1; }
	cp "$zipini" "$1/AVP_OpenInteng.ini"
	if [ $# -ge 2 ]; then printf '%s\n' "$2" >> "$1/AVP_OpenInteng.ini"; fi
}
# run <folder> <arguments...>: the program there, hidden, 300 s at most; its output in $out and its exit code in $code
run() {
	local where=$1; shift
	out=$(cd "$where" && PATH="$dlls:$PATH" timeout 300 ./AVP_OpenInteng.exe "$@" 2>&1); code=$?
}

echo "== 1. in <the game>\\Tools with the zip's ini, a level given (a double-click)"
put "$tools"
run "$tools" -picture 20 "$gw\\avp_rifs\\Genshd1.RIF"
echo "$out" | grep -v "^AVP_OpenInteng: object list\|^AVP_OpenInteng: model objects: bounds" | tail -6 | sed 's/^/    /'
[ "$code" = 0 ] && ok "it ended with 0" || bad "it ended with $code, not 0"
modules=$(echo "$out" | sed -n 's/^AVP_OpenInteng: model objects: the game has \([0-9]*\) modules.*/\1/p' | head -1)
[ -n "$modules" ] && [ "$modules" -gt 0 ] && ok "Genshd1 opened: the game has $modules modules" || bad "Genshd1 was not opened (no modules line)"
rm -f "$work/case-1-level.png" "$work/case-2-no-file.png"
[ -s "$tools/first-picture.png" ] && cp "$tools/first-picture.png" "$work/case-1-level.png" && ok "its picture was saved (kept as $work/case-1-level.png: look at it)" || bad "no picture saved"
logline=$(grep -h "the data folder: " "$tools"/settings/avp_log*.txt 2>/dev/null | head -1 | tr -d '\r')
# the game's log writes the user's own folder as %USERPROFILE%, never its name (a log is shared to report a bug)
logged="$gw"; [ -n "${USERPROFILE:-}" ] && logged="%USERPROFILE%${gw#"$USERPROFILE"}"
want="the data folder: $logged (the game's folder, the one above the editor's: AVP_OpenInteng.ini names no DataFolder)"
case "$logline" in *"$want"*) ok "its log: $want" ;; *) bad "its log does not say \"$want\" (it says: ${logline:-nothing})" ;; esac

echo "== 2. in <the game>\\Tools with the zip's ini, no file (as with someone at the screen)"
put "$tools"
run "$tools" -picture 1 -nofile
echo "$out" | tail -3 | sed 's/^/    /'
[ "$code" = 0 ] && ok "it ended with 0" || bad "it ended with $code, not 0"
echo "$out" | grep -q "message: Please load a rif file by double clicking on it (dialog kind message, buttons Ok " && ok "inteng's message is up, with its Ok" || bad "inteng's no-file message is not up"
echo "$out" | grep -q "message answered: Ok" && ok "and Ok ended it" || bad "Ok did not end it"
[ -s "$tools/first-picture.png" ] && cp "$tools/first-picture.png" "$work/case-2-no-file.png" && ok "its picture was saved (kept as $work/case-2-no-file.png: look at it)" || bad "no picture of the message"

echo "== 3. a written DataFolder= naming a folder that isn't there: it wins, and stops in words"
put "$tools" "DataFolder=$(cygpath -w "$work")\\no such folder"
run "$tools" -picture 20 "$gw\\avp_rifs\\Genshd1.RIF"
echo "$out" | tail -2 | sed 's/^/    /'
[ "$code" != 0 ] && ok "it stopped (exit $code)" || bad "it ended with 0: the game's folder was taken over a written line"
echo "$out" | grep -q "DataFolder in AVP_OpenInteng.ini can't be used: there's no folder there" && ok "saying the written folder isn't there" || bad "not saying the written folder isn't there"
echo "$out" | grep -q "model objects: the game has" && bad "a level was opened all the same" || ok "and nothing was opened"

echo "== 4. a written DataFolder= too long to read (300 letters): it stops, saying so"
put "$tools" "DataFolder=C:\\$(printf 'a%.0s' $(seq 300))"
run "$tools" -picture 20 "$gw\\avp_rifs\\Genshd1.RIF"
echo "$out" | tail -2 | cut -c1-200 | sed 's/^/    /'
[ "$code" != 0 ] && ok "it stopped (exit $code)" || bad "it ended with 0: the game's folder was taken over a written line"
echo "$out" | grep -q "DataFolder= in AVP_OpenInteng.ini beside the program is too long" && ok "saying DataFolder= is too long" || bad "not saying DataFolder= is too long"

echo "== 5. anywhere else, with the zip's ini: it stops, saying how to fix it"
put "$elsewhere"
run "$elsewhere" -picture 20 "$gw\\avp_rifs\\Genshd1.RIF"
echo "$out" | tail -2 | sed 's/^/    /'
[ "$code" != 0 ] && ok "it stopped (exit $code)" || bad "it ended with 0 with no data folder"
echo "$out" | grep -q "the editor can't find the game's files. Put it in the game's own Tools folder" && echo "$out" | grep -q "as DataFolder= under \[Editor\]" \
	&& ok "saying: put it in the game's Tools folder, or name the game's folder as DataFolder=" || bad "not saying how to fix it"

[ "$fail" = 0 ] || { echo "FAIL: the first start does not work out of the box"; exit 1; }
echo "PASS: the editor in <the game>\\Tools with the zip's own ini starts and opens a level, and a written DataFolder= still wins and stops in words"
