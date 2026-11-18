// state_line.h: the scenario harness's state line, its fields added by the code that owns them (Bishop's ruling D12, 11-17: after this, nobody edits the big printf in editor_main.cpp). Written
// 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each frame the editor forgets the last frame's fields (StateLine_NewFrame, at the top of its loop); the code that owns a field adds it while the frame runs (its own screen's pass, its panels,
// its frame hook); the state line printed after the frame's draw carries them, after the fixed fields, as  name "value"  (drive_scenario's Field reads them by name). A field nobody added this
// frame is not on the line: an owner adds its field every frame it means it (an empty value is fine: the field is then there and empty).

#pragma once

#include <string>

// This frame's value of a field (it replaces one added before in the same frame). The name is letters and digits; a '"' or a line break in the value is written as '?'
void StateLine_Add(char const *name, std::string const &value);

// The editor's loop, at the top of each frame: the last frame's fields are forgotten
void StateLine_NewFrame();

// The fields added this frame, as  ; name "value"  each, in the order of their names: what the printed state line ends with
std::string StateLine_Words();
