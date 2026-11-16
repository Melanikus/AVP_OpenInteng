// The test for the text of inteng's Write Asc File (asc_text.h): every line's bytes, as inteng's format strings make them. Written 2026-11-15 by
// Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt. No window, none of the game.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "asc_text.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	puts("The text of a Write Asc File:");

	Check(AscScale() > 0.0025662 && AscScale() < 0.0025663, "the scale is 0.002566257");

	AscLevelObject object;

	object.objectName = "door \"a\"";
	AscVertex const v0 = { 1.5, -2.25, 3.0 };
	AscVertex const v1 = { 0, 0, 0 };

	object.numbers.vertices = { v0, v1 };

	AscFace const face = { 0, 1, 1, 1, 0, 1 };

	object.numbers.faces = { face };

	AscOffset const none = { 0, 0, 0 };
	std::string const text = AscFileText({ object }, none, 1.0);

	Check(text.compare(0, 66, "Ambient light color: Red=0.039216 Green=0.039216 Blue=0.039216\r\n\r\n") == 0, "the file starts with inteng's header");
	Check(text.find("Named object: \"door _a_\"\r\nTri-mesh, Vertices: 2     Faces: 1\r\nVertex list:\r\n") != std::string::npos,
		"an object: its name (a quote can't end the line), the counts with five spaces, the vertex list");
	Check(text.find("Vertex 0:  X: 1.500000     Y: -2.250000     Z: 3.000000\r\nVertex 1:  X: 0.000000     Y: 0.000000     Z: 0.000000\r\nFace list:\r\n") !=
		std::string::npos, "the vertex lines: two spaces after the colon, five before Y and Z, six decimals");
	Check(text.find("Face 0:    A:0 B:1 C:1 AB:1 BC:0 CA:1\r\n\r\nNamed object: \"hudson\"") != std::string::npos,
		"a face line: four spaces after the colon; after the last one a line break of its own, then the marker");
	Check(text.find("Named object: \"hudson\"\r\nTri-mesh, Vertices: 4     Faces: 2\r\nVertex list:\r\n") != std::string::npos, "the marker follows");
	Check(text.size() > 2 && text.compare(text.size() - 2, 2, "\r\n") == 0 && text.find("Smoothing:  17\r\n") != std::string::npos, "and ends with its faces");

	// the marker's first vertex: X = offset.x * scale + (-1.2832...), the line has the same format
	Check(text.find("Vertex 0:  X: -1.283") != std::string::npos, "the marker's first vertex is X: -1.283...");

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
