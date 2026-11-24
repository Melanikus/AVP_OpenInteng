#!/usr/bin/env bash
# ACCEPTANCE: Track Editing's hand-over of the object, the track in memory, the end marker and the sounds (Frost, 2026-11-16, Bishop's orders #6, #5, #1, #2, #4). This file says what FINISHED is for these items;
# each line names the inteng code it is traced from.
#
#   B6  Edit Track LOADS the track with FUN_004f0490 run on it (FUN_004eff90 0x4f01d2, third argument 0): the track is turned and moved so that it starts at the object's place and turn. An object the user
#       moved since the track was made brings its track along.
#   B6  Stop Track Edit lets the track go (FUN_004eff90(0,0) 0x4dcaf0): FUN_004f1300 writes the track into the OBJTRAK2 / TRAKSOUN chunks, and FUN_004f0870 (0x4effba) puts the track's pose at its timer start into
#       the object's matrix and place: the object's record in the level IS that pose.
#   B5  The track is kept in memory while it is edited: until Stop Track Edit the level still holds the track it had.
#   B1  The end marker flies (F) and is carried by the camera (the flying object follows the camera: DAT_008f4b60 / DAT_008f4bb8, 0x4dc9a6); the camera's turn turns it; Finish End Pos Edit (FUN_004f0cd0) makes
#   B2  its place and its turn the section's end (the object stays at the marker's place).
#   B4  Play Track plays the track's start, middle and end sounds, by the GAME's own sound code (track.cpp Start_Track_Sound / Update_Track_Sound, psnd.cpp, OpenAL), found under the level's SOUNDDIR as the
#       game finds them; a loop sound sounds only within its outer range of the listener (the game's rule), so the camera is put at the track's start. The sound system must start, the game must load the
#       track's sounds and at least one must be sounding; Stop track play lets them go. (A run with no sound device says SKIPPED, not ok.)
#   SILENCE  These runs are HIDDEN (-picture): nobody is at the screen, so nothing may be heard (Melanikus 11-16: "been hearing them all afternoon"). The output volume, OpenAL's listener gain, is read on
#       every frame from Play to the check and must never be above the game's volume 0 (0.00001, openal_audio.cpp vol_to_gain_table[0]): SoundSys_Management sets the volume back to the effects' volume
#       on every call (psnd.cpp:133-139), so the silence must hold the effects' volume at 0 too.
#
# HOW: the editor's own hidden proof runs (-trackposeproof, -tracksoundproof, through the bar and the pop-up the way a user does it, on the stock levels that have tracks).
#
# RUN: bash tests/scenarios/acceptance-track-editing.sh <the editor exe built from the tree>   (lane T2; AvP-docs\tools\frost\track-proof-runs.sh does the run; the exit code is 0 only if nothing FAILED)
#
# EVERY CHECK READS ONLY WHAT THIS RUN MADE (Frost 2026-11-24, Bishop's order; Drake's two asks): until then the runner's exit was thrown away and the proofs were read from one
# shared folder (frost-proofs/acc-track/<level>), so when the runner refused (lane T2 held: "LANE NOT FREE", exit 1, before it writes anything) the LAST run's proofs were read and
# said ok: a green in 4-5 s that ran nothing (11-24 13:3x). Now:
#   - the exe this run uses is named on the first line, with its SHA-256;
#   - each run of the script has its own proofs folders (acc-track-<this run's number>, acc-sound-<...>), so neither an older run nor one going on at the same time can be read;
#   - a run.txt counts only if it was written after its own call started (a mark file made just before), and only if the editor itself ended well: the runner writes the
#     editor's exit as run.txt's last line and exits 0 itself (track-proof-runs.sh), so that line must be exactly "exit 0" (Drake's MUST 1: a crash at teardown, or a sanitizer
#     report, after the WORKS lines is FAILED, and its run.txt kept, never deleted with a pass);
#   - the runner's exit is read: "LANE NOT FREE" waits (30 s at a time, TRACK_EDITING_LANE_WAIT seconds in all, 1800 unless set), and any other refusal, or a lane never free,
#     is FAILED, never ok.
# The private folders are deleted when every check passed (they hold a picture each), and kept when one failed, to be looked at.
exe="${1:?the editor exe}"
runner=/c/Users/Melanikus/source/AvP-docs/tools/frost/track-proof-runs.sh
lane=/c/Users/Melanikus/source/AvP-builds-excluded/RUN-LANE-T2.txt
proofs=/c/Users/Melanikus/source/AvP-builds-excluded/frost-proofs
wait_limit=${TRACK_EDITING_LANE_WAIT:-1800}
[[ "$wait_limit" =~ ^[0-9]+$ ]] || { echo "  FAILED  TRACK_EDITING_LANE_WAIT is a number of seconds"; exit 1; }
[ -f "$exe" ] || { echo "  FAILED  no editor at $exe"; exit 1; }
echo "exe: $exe sha256 $(sha256sum "$exe" | cut -c1-16)"
own=$$-$(date +%s)
bad=0

# proof <run name> <level> <editor arguments...>: the runner's run, waiting while lane T2 is held; gives back 0 only when the runner ran and wrote a run.txt after this call began
proof() {
  local run=$1 level=$2 waited=0 out rc
  shift 2
  local mark="$proofs/$run.mark-$level"
  mkdir -p "$proofs"; : > "$mark"; sleep 1
  while true; do
    out=$(LANE_FILE=$lane bash "$runner" "$exe" "$run" "$level" "$@" 2>&1); rc=$?
    [ "$rc" = 0 ] && break
    if [ "$out" = "LANE NOT FREE" ] && [ "$waited" -lt "$wait_limit" ]; then sleep 30; waited=$((waited + 30)); continue; fi
    echo "  FAILED  $level: the proof run did not run (the runner: exit $rc, \"$(echo "$out" | head -1 | cut -c1-120)\"$( [ "$out" = "LANE NOT FREE" ] && echo ", lane T2 held for $waited s"))"
    rm -f "$mark"; return 1
  done
  if [ ! -f "$proofs/$run/$level/run.txt" ] || [ ! "$proofs/$run/$level/run.txt" -nt "$mark" ]; then
    echo "  FAILED  $level: no run.txt written by this run in $proofs/$run/$level"
    rm -f "$mark"; return 1
  fi
  local ended
  ended=$(tail -n 1 "$proofs/$run/$level/run.txt" | tr -d '\r')
  if [ "$ended" != "exit 0" ]; then
    echo "  FAILED  $level: the editor ended with \"$ended\" (a crash or a sanitizer report: $proofs/$run/$level/run.txt)"
    rm -f "$mark"; return 1
  fi
  rm -f "$mark"; return 0
}

for level in Temple Escape Area52 Invasion; do
  proof "acc-track-$own" "$level" -picture 500 -trackposeproof || { bad=1; continue; }
  out=$proofs/acc-track-$own/$level/run.txt
  if grep -aq "Track Editing main job: WORKS in the running editor (pose: the track followed the moved object" "$out"; then
    echo "  ok      $level: $(grep -a 'Stop Track Edit: the object' "$out" | sed 's/.*track proof: //' | cut -c1-200)"
  else
    echo "  FAILED  $level: $(grep -a 'DOES NOT\|track proof' "$out" | tail -n 1 | cut -c1-300)"
    bad=1
  fi
done
for level in Temple Escape Area52 Invasion; do
  proof "acc-sound-$own" "$level" -picture 500 -tracksoundproof || { bad=1; continue; }
  out=$proofs/acc-sound-$own/$level/run.txt
  if grep -aq "NOT PROVEN here" "$out"; then
    echo "  SKIPPED $level sounds: no sound device in this run"
  elif ! grep -aq "SILENT on every frame (a hidden run)" "$out"; then
    echo "  FAILED  $level silence: $(grep -a 'output volume' "$out" | sed 's/.*track proof: //' | cut -c1-200)"
    bad=1
  elif grep -aq "Track sounds: WORKS in the running editor" "$out" && grep -aq "the sounds are let go: yes" "$out"; then
    echo "  ok      $level sounds: $(grep -a 'sound system' "$out" | sed 's/.*track proof: //' | cut -c1-160); $(grep -a 'output volume' "$out" | sed 's/.*track proof: //' | cut -c1-120)"
  else
    echo "  FAILED  $level sounds: $(grep -a 'Track sounds' "$out" | tail -n 1 | cut -c1-200)"
    bad=1
  fi
done
if [ "$bad" = 0 ]; then
  rm -rf -- "${proofs:?}/acc-track-${own:?}" "${proofs:?}/acc-sound-${own:?}"
  echo "All checks passed"
else
  echo "SOME CHECKS FAILED (this run's proofs kept: $proofs/acc-track-$own, $proofs/acc-sound-$own)"
fi
exit $bad
