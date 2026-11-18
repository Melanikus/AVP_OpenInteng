// write_c_file.h: Construction > File > Write C File (0xc). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// INTENG (FUN_004452b0 0x445867-0x44595f, AvP-docs\engine\inteng\write-c-file-facts.md): only with exactly ONE object selected (DAT_00631234 == 1; else nothing
// happens), the dialog FUN_00447bc0 ("Write .c file", "Type filename for shape (without .c extension)", an edit of 200 letters, Ok / Cancel); Ok with a name writes
// the object's shape as C (FUN_00503da0: model/c_shape_text.h) to <name>.c in the folder inteng runs in.
// OURS: the same question and text; the file goes to <Documents>\AVP_OpenInteng\edited\<name>.c (save_copy.h's SaveExportPlace, as Write Asc File), never into the
// game's folder, and the status line says where it went or why not (inteng says nothing). The shape is the editor's working bytes, as inteng writes the shape it holds.
#pragma once

#include <stdint.h>

#include <string>

#include "c_shape_text.h"

class OpenLevelFile;

namespace host
{
	// The text maker's source for the shape of object `id`: the shape (ShapeEdit_Read), its lists of texture coordinates (SHPUVCRD), its animation (TEXTANIM), the
	// objects using it (SHPHEAD1's list, each with its place), the level's scale (ENVSDSCL's double, 1.0 without one) and pictures (BMPNAMES). False, with *why,
	// when the object or its shape can't be read.
	bool WriteC_Source(OpenLevelFile *open, uint8_t const id[8], std::string const &name, model::CShapeSource *out, std::string *why);
}

// The editor's side: the one object of the selection (false when there isn't exactly one: the command does nothing), and the file written for what was typed
bool WriteC_TheObject(uint8_t id[8]);
void WriteTheC(std::string const &typed);
