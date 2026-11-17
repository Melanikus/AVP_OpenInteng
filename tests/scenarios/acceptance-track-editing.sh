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
exe="${1:?the editor exe}"
runner=/c/Users/Melanikus/source/AvP-docs/tools/frost/track-proof-runs.sh
lane=/c/Users/Melanikus/source/AvP-builds-excluded/RUN-LANE-T2.txt
proofs=/c/Users/Melanikus/source/AvP-builds-excluded/frost-proofs
bad=0
for level in Temple Escape Area52 Invasion; do
  LANE_FILE=$lane bash "$runner" "$exe" acc-track "$level" -picture 500 -trackposeproof > /dev/null 2>&1
  out=$proofs/acc-track/$level/run.txt
  if grep -aq "Track Editing main job: WORKS in the running editor (pose: the track followed the moved object" "$out"; then
    echo "  ok      $level: $(grep -a 'Stop Track Edit: the object' "$out" | sed 's/.*track proof: //' | cut -c1-200)"
  else
    echo "  FAILED  $level: $(grep -a 'DOES NOT\|track proof' "$out" | tail -n 1 | cut -c1-300)"
    bad=1
  fi
done
for level in Temple Escape Area52 Invasion; do
  LANE_FILE=$lane bash "$runner" "$exe" acc-sound "$level" -picture 500 -tracksoundproof > /dev/null 2>&1
  out=$proofs/acc-sound/$level/run.txt
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
[ "$bad" = 0 ] && echo "All checks passed" || echo "SOME CHECKS FAILED"
exit $bad
