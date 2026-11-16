// See asc_text.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "asc_text.h"

#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace host
{
	// The sprintf at 0x506eb7 pushes 0x5ec788: the bytes 0d 0a 00 00 (run 27, p11-shape-handlers.md section 39)
	char const kAscAfterFaces[] = "\r\n";

	namespace
	{
		// inteng's format strings, byte for byte (inteng-strings.txt:2825-2837)
		char const *const kHeader = "Ambient light color: Red=0.039216 Green=0.039216 Blue=0.039216\r\n\r\n";
		char const *const kNamedObject = "Named object: \"%s\"\r\n";
		char const *const kTriMesh = "Tri-mesh, Vertices: %d     Faces: %d\r\n";
		char const *const kVertexList = "Vertex list:\r\n";
		char const *const kVertex = "Vertex %d:  X: %f     Y: %f     Z: %f\r\n";
		char const *const kFaceList = "Face list:\r\n";
		char const *const kFace = "Face %d:    A:%d B:%d C:%d AB:%d BC:%d CA:%d\r\n";
		char const *const kMarkerStart = "Named object: \"hudson\"\r\nTri-mesh, Vertices: 4     Faces: 2\r\nVertex list:\r\n";
		char const *const kMarkerFaces =
			"Face list:\r\nFace 0:    A:0 B:1 C:2 AB:1 BC:1 CA:1\r\nMaterial:\"RED PLASTIC\"\r\nSmoothing:  17\r\nFace 1:    A:2 B:3 C:0 AB:1 BC:1 CA:1\r\nMaterial:\"RED PLASTIC\"\r\nSmoothing:  17\r\n";

		// One formatted line, with the "C" locale's number format. The buffer is big enough for any line made here (a name is at most 64 letters).
		std::string Line(char const *format, ...)
		{
			static _locale_t const cLocale = _create_locale(LC_NUMERIC, "C");
			char text[1024];
			va_list args;

			va_start(args, format);
			int const length = _vsnprintf_s_l(text, sizeof(text), _TRUNCATE, format, cLocale, args);
			va_end(args);

			return length < 0 ? std::string(text) : std::string(text, (size_t)length);
		}

		// The object's name written only with the letters a level can have in one: no quote or line break can end the line early
		std::string SafeName(std::string const &name)
		{
			std::string safe;

			for (char const letter : name)
			{
				safe += (letter >= 0x20 && letter < 0x7F && letter != '"') ? letter : '_';
			}

			return safe;
		}
	}

	double AscScale()
	{
		uint64_t const bits = 0x3f6505d4bc6b91daull;
		double scale = 0;

		memcpy(&scale, &bits, sizeof(scale));
		return scale;
	}

	std::string AscFileText(std::vector<AscLevelObject> const &objects, AscOffset const &offset, double scale)
	{
		std::string text = kHeader;

		for (AscLevelObject const &object : objects)
		{
			text += Line(kNamedObject, SafeName(object.objectName).c_str());
			text += Line(kTriMesh, (int)object.numbers.vertices.size(), (int)object.numbers.faces.size());
			text += kVertexList;

			for (size_t i = 0; i < object.numbers.vertices.size(); i++)
			{
				AscVertex const &vertex = object.numbers.vertices[i];

				text += Line(kVertex, (int)i, vertex.x, vertex.y, vertex.z);
			}

			text += kFaceList;

			for (size_t i = 0; i < object.numbers.faces.size(); i++)
			{
				AscFace const &face = object.numbers.faces[i];

				text += Line(kFace, (int)i, face.a, face.b, face.c, face.ab, face.bc, face.ca);
			}

			text += kAscAfterFaces;
		}

		AscMarker const marker = MakeAscMarker(offset, scale);

		text += kMarkerStart;

		for (int i = 0; i < 4; i++)
		{
			text += Line(kVertex, i, marker.vertices[i].x, marker.vertices[i].y, marker.vertices[i].z);
		}

		text += kMarkerFaces;
		return text;
	}
}
