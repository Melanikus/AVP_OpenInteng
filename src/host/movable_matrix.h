// movable_matrix.h: a turn as movable.h gives it (a quaternion x y z w, OBJHEAD1's way) to and from a 3 x 3 of 16.16 numbers, row by row, the way a
// player start's AVPSTART and a Light Sourcing light's STDLIGHT hold theirs. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng, moved out of
// movable_special.cpp so the light's adapter uses the same sums. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE PAIRING is the editor's own for an object (level_view.cpp LevelView_MatrixOfTurn / LevelView_TurnOfMatrix) and for things made at the camera:
// a player start made beside a particle generator (special_objects_edit.h SpecialPlace), a light dropped with D (light_keys.h LightDefaultDrop: the
// camera's right, down and forward as its rows). The matrix is the game's QuatToMat (avpx/3dc/maths.cpp:2374) of the turn with x y z negated, as the
// game reads every quaternion turn (object_setup.cpp:899-902); the turn is the matrix's quaternion with x y z negated back. The sums are in double
// here, the game's in 16.16: a number can be 1 / 65536 apart.
#pragma once

#include <stdint.h>

namespace host
{
	// The matrix of a turn (made 1 long by the caller), row by row: mat11 mat12 mat13 mat21 ... mat33
	void Movable_MatrixOfTurn(float const turn[4], int32_t matrix[9]);

	// The turn of a matrix, by its largest diagonal sum, so no division is by a small number. A matrix that is no turn (a hostile file's zeros) still
	// gives real numbers; one that gives nothing usable gives the identity.
	void Movable_TurnOfMatrix(int32_t const matrix[9], float turn[4]);

	// The three axes the matrix turns a thing to, as the editor's icons take them (editor_icons.h EditorIcon_Add: rows of the game's matrix, unit length): the game takes a player
	// start's AVPSTART matrix as the player's own (projload.cpp setup_start_position), its third row the way it faces (P2-LIGHT-TURN). false, with nothing written, when a row is
	// about 0 long (a hostile file's zeros): the icon is then drawn unturned. Added 2026-11-19 (Bishop's item 4: a player start's icon never turned).
	bool Movable_MatrixAxes(int32_t const matrix[9], double axes[9]);
}
