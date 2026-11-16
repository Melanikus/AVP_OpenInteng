// See shape_soften_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "shape_soften_edit.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "shape_edit.h"
#include "shape_soften.h"

namespace host
{
	char const *ShapeSoftenWords(ShapeSoftenResult result)
	{
		switch (result)
		{
		case ShapeSoftenResult::Done: return "done";
		case ShapeSoftenResult::BadCall: return "nothing to change";
		case ShapeSoftenResult::BadAngle: return "the angle is a whole number from 0 to 180";
		case ShapeSoftenResult::NoObject: return "no object has that name";
		case ShapeSoftenResult::NoShape: return "the object has no shape the editor can read";
		case ShapeSoftenResult::LeftAlone: return "this shape is one inteng leaves alone (its header flag 0x1000)";
		case ShapeSoftenResult::NoNormals: return "the shape has no vertex normals to make again";
		case ShapeSoftenResult::BadShape: return "the shape's numbers are not ones the editor takes";
		case ShapeSoftenResult::Refused: return "the shape's bytes wouldn't take the change";
		}

		return "the edges could not be made soft";
	}

	bool ShapeSoften_ReadAngle(std::string const &typed, int *angle)
	{
		if (!angle || typed.size() >= 256)
		{
			return false;
		}

		// sscanf("%d"): leading white space, an optional sign, digits; what follows the digits is not looked at
		size_t at = 0;

		while (at < typed.size() && isspace((unsigned char)typed[at]))
		{
			at++;
		}

		bool negative = false;

		if (at < typed.size() && (typed[at] == '+' || typed[at] == '-'))
		{
			negative = typed[at] == '-';
			at++;
		}

		if (at >= typed.size() || !isdigit((unsigned char)typed[at]))
		{
			return false;
		}

		long long value = 0;

		while (at < typed.size() && isdigit((unsigned char)typed[at]))
		{
			value = value * 10 + (typed[at] - '0');

			if (value > 1000000)
			{
				value = 1000000;
			}

			at++;
		}

		if (negative)
		{
			value = -value;
		}

		if (value < 0 || value > (long long)model::kSoftenMostAngle)
		{
			return false;
		}

		*angle = (int)value;
		return true;
	}

	ShapeSoftenResult ShapeSoften_ByAngle(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, int angle, double levelScale, size_t *changed, std::string *why)
	{
		if (!store || !level || !id)
		{
			return ShapeSoftenResult::BadCall;
		}

		if (angle < 0 || angle > (int)model::kSoftenMostAngle)
		{
			return ShapeSoftenResult::BadAngle;
		}

		model::ShapeModel shape;
		std::string reason;
		ShapeEditResult const read = ShapeEdit_Read(level, store, id, &shape, nullptr, &reason);

		if (read == ShapeEditResult::NoObject)
		{
			return ShapeSoftenResult::NoObject;
		}

		if (read == ShapeEditResult::NoShape || read == ShapeEditResult::NoBytes)
		{
			return ShapeSoftenResult::NoShape;
		}

		if (read != ShapeEditResult::Done)
		{
			if (why)
			{
				*why = reason;
			}

			return ShapeSoftenResult::BadShape;
		}

		if (shape.flags & 0x1000)
		{
			return ShapeSoftenResult::LeftAlone;
		}

		if (shape.pointNormals.size() != shape.points.size() || shape.pointNormals.empty())
		{
			return ShapeSoftenResult::NoNormals;
		}

		if (!model::SoftenShape(&shape, angle, levelScale, changed))
		{
			return ShapeSoftenResult::BadShape;
		}

		std::string writeWhy;

		if (ShapeEdit_WriteSameCounts(store, level, id, shape, &writeWhy) != ShapeEditResult::Done)
		{
			if (why)
			{
				*why = writeWhy;
			}

			return ShapeSoftenResult::Refused;
		}

		return ShapeSoftenResult::Done;
	}
}
