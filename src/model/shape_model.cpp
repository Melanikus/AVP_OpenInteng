// See shape_model.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_model.h"

#include <math.h>
#include <string.h>

namespace model
{
	namespace
	{
		// Where the numbers are in SHPHEAD1's payload (shape_model.h)
		size_t const kHeaderFlagsAt = 0;
		size_t const kHeaderNumberAt = 20;
		size_t const kHeaderPointsAt = 24;
		size_t const kHeaderPolygonsAt = 28;
		size_t const kHeaderRadiusAt = 32;
		size_t const kHeaderExtentsAt = 36;
		size_t const kHeaderFixedBytes = 68;   // the flags to the count of names

		int32_t Int(uint8_t const *at)
		{
			int32_t value;

			memcpy(&value, at, sizeof(value));
			return value;
		}

		float Float(uint8_t const *at)
		{
			float value;

			memcpy(&value, at, sizeof(value));
			return value;
		}

		void PutInt(uint8_t *at, int32_t value)
		{
			memcpy(at, &value, sizeof(value));
		}

		void PutFloat(uint8_t *at, float value)
		{
			memcpy(at, &value, sizeof(value));
		}

		// A payload that has bytes has a pointer; a payload with none may have either
		bool Whole(uint8_t const *bytes, size_t size)
		{
			return bytes != nullptr || size == 0;
		}

		ShapeModelResult ReadNormals(uint8_t const *bytes, size_t size, size_t expected, std::vector<ShapeNormal> *out)
		{
			out->clear();

			if (size == 0)
			{
				return ShapeModelResult::Ok;   // no normals chunk: the shape has none
			}

			if (size % kShapeNormalBytes != 0 || size / kShapeNormalBytes != expected)
			{
				return ShapeModelResult::SizesDisagree;
			}

			out->resize(expected);

			for (size_t i = 0; i < expected; i++)
			{
				ShapeNormal &normal = (*out)[i];

				normal.x = Float(bytes + i * kShapeNormalBytes);
				normal.y = Float(bytes + i * kShapeNormalBytes + 4);
				normal.z = Float(bytes + i * kShapeNormalBytes + 8);

				if (!isfinite(normal.x) || !isfinite(normal.y) || !isfinite(normal.z))
				{
					out->clear();
					return ShapeModelResult::NotFinite;
				}
			}

			return ShapeModelResult::Ok;
		}
	}

	char const *ShapeModelWords(ShapeModelResult result)
	{
		switch (result)
		{
		case ShapeModelResult::Ok: return "the shape was read";
		case ShapeModelResult::BadCall: return "a payload is missing";
		case ShapeModelResult::HeaderTooShort: return "the shape's header is too short";
		case ShapeModelResult::TooBig: return "the shape has more points or polygons than the editor takes";
		case ShapeModelResult::SizesDisagree: return "the shape's chunks don't agree on how many points or polygons it has";
		case ShapeModelResult::BadCorner: return "a polygon has a corner that isn't a point of the shape, or the wrong number of corners";
		case ShapeModelResult::BadNormalNumber: return "a polygon names a normal that isn't there";
		case ShapeModelResult::NotFinite: return "the shape holds a number that isn't a number";
		case ShapeModelResult::WrongCount: return "the numbers don't fit the bytes they would replace";
		}

		return "unknown answer";
	}

	ShapeModelResult ReadShapeModel(ShapeBytes const &bytes, ShapeModel *model)
	{
		if (!model)
		{
			return ShapeModelResult::BadCall;
		}

		*model = ShapeModel();

		if (!Whole(bytes.header, bytes.headerSize) || !Whole(bytes.points, bytes.pointsSize) || !Whole(bytes.polygons, bytes.polygonsSize) ||
			!Whole(bytes.polyNormals, bytes.polyNormalsSize) || !Whole(bytes.ptNormals, bytes.ptNormalsSize))
		{
			return ShapeModelResult::BadCall;
		}

		if (bytes.headerSize < kHeaderFixedBytes)
		{
			return ShapeModelResult::HeaderTooShort;
		}

		int32_t const countPoints = Int(bytes.header + kHeaderPointsAt);
		int32_t const countPolygons = Int(bytes.header + kHeaderPolygonsAt);

		if (countPoints < 0 || countPolygons < 0 || (size_t)countPoints > kMostShapePoints || (size_t)countPolygons > kMostShapePolygons)
		{
			return ShapeModelResult::TooBig;
		}

		size_t const points = (size_t)countPoints;
		size_t const polygons = (size_t)countPolygons;

		// The counts have to be what the payloads' sizes say, whole records, both ways
		if (bytes.pointsSize % kShapePointBytes != 0 || bytes.pointsSize / kShapePointBytes != points ||
			bytes.polygonsSize % kShapePolygonBytes != 0 || bytes.polygonsSize / kShapePolygonBytes != polygons)
		{
			return ShapeModelResult::SizesDisagree;
		}

		ShapeModel shape;

		shape.flags = Int(bytes.header + kHeaderFlagsAt);
		shape.number = Int(bytes.header + kHeaderNumberAt);
		shape.radius = Float(bytes.header + kHeaderRadiusAt);

		if (!isfinite(shape.radius))
		{
			return ShapeModelResult::NotFinite;
		}

		shape.extents.maxX = Int(bytes.header + kHeaderExtentsAt);
		shape.extents.minX = Int(bytes.header + kHeaderExtentsAt + 4);
		shape.extents.maxY = Int(bytes.header + kHeaderExtentsAt + 8);
		shape.extents.minY = Int(bytes.header + kHeaderExtentsAt + 12);
		shape.extents.maxZ = Int(bytes.header + kHeaderExtentsAt + 16);
		shape.extents.minZ = Int(bytes.header + kHeaderExtentsAt + 20);

		shape.points.resize(points);

		for (size_t i = 0; i < points; i++)
		{
			shape.points[i].x = Int(bytes.points + i * kShapePointBytes);
			shape.points[i].y = Int(bytes.points + i * kShapePointBytes + 4);
			shape.points[i].z = Int(bytes.points + i * kShapePointBytes + 8);
		}

		// The normals come before the polygons are checked, so a polygon's normal number can be checked against how many there are
		ShapeModelResult answer = ReadNormals(bytes.polyNormals, bytes.polyNormalsSize, polygons, &shape.polygonNormals);

		if (answer != ShapeModelResult::Ok)
		{
			return answer;
		}

		answer = ReadNormals(bytes.ptNormals, bytes.ptNormalsSize, points, &shape.pointNormals);

		if (answer != ShapeModelResult::Ok)
		{
			return answer;
		}

		shape.polygons.resize(polygons);

		for (size_t i = 0; i < polygons; i++)
		{
			uint8_t const *const at = bytes.polygons + i * kShapePolygonBytes;
			ShapePolygon &polygon = shape.polygons[i];

			polygon.engineType = Int(at);
			polygon.normalIndex = Int(at + 4);
			polygon.flags = Int(at + 8);
			polygon.colour = Int(at + 12);
			polygon.cornerCount = 0;

			bool ended = false;

			for (int c = 0; c < 5; c++)
			{
				polygon.corners[c] = Int(at + 16 + 4 * c);

				if (polygon.corners[c] == -1)
				{
					ended = true;
				}
				else if (ended || polygon.corners[c] < 0 || (size_t)polygon.corners[c] >= points)
				{
					return ShapeModelResult::BadCorner;   // a corner after the end, a negative one that isn't -1, or a point that isn't there
				}
				else
				{
					polygon.cornerCount++;
				}
			}

			if (polygon.cornerCount < 3)
			{
				return ShapeModelResult::BadCorner;
			}

			// A polygon's normal is one of the shape's normals, when the shape has any
			if (!shape.polygonNormals.empty() && (polygon.normalIndex < 0 || (size_t)polygon.normalIndex >= shape.polygonNormals.size()))
			{
				return ShapeModelResult::BadNormalNumber;
			}
		}

		*model = shape;
		return ShapeModelResult::Ok;
	}

	ShapeModelResult WriteShapePoints(ShapeModel const &model, std::vector<uint8_t> *pointsBytes)
	{
		if (!pointsBytes)
		{
			return ShapeModelResult::BadCall;
		}

		if (pointsBytes->size() < model.points.size() * kShapePointBytes || pointsBytes->size() % kShapePointBytes != 0 ||
			pointsBytes->size() / kShapePointBytes != model.points.size())
		{
			return ShapeModelResult::WrongCount;
		}

		for (size_t i = 0; i < model.points.size(); i++)
		{
			PutInt(pointsBytes->data() + i * kShapePointBytes, model.points[i].x);
			PutInt(pointsBytes->data() + i * kShapePointBytes + 4, model.points[i].y);
			PutInt(pointsBytes->data() + i * kShapePointBytes + 8, model.points[i].z);
		}

		return ShapeModelResult::Ok;
	}

	ShapeModelResult WriteShapePolygons(ShapeModel const &model, std::vector<uint8_t> *polygonBytes)
	{
		if (!polygonBytes)
		{
			return ShapeModelResult::BadCall;
		}

		if (polygonBytes->size() % kShapePolygonBytes != 0 || polygonBytes->size() / kShapePolygonBytes != model.polygons.size())
		{
			return ShapeModelResult::WrongCount;
		}

		for (size_t i = 0; i < model.polygons.size(); i++)
		{
			ShapePolygon const &polygon = model.polygons[i];

			if (polygon.cornerCount < 3 || polygon.cornerCount > 5)
			{
				return ShapeModelResult::BadCorner;
			}

			for (int c = 0; c < polygon.cornerCount; c++)
			{
				if (polygon.corners[c] < 0 || (size_t)polygon.corners[c] >= model.points.size())
				{
					return ShapeModelResult::BadCorner;
				}
			}
		}

		// Everything is checked: now the bytes are written, so a refusal above leaves them untouched
		for (size_t i = 0; i < model.polygons.size(); i++)
		{
			ShapePolygon const &polygon = model.polygons[i];
			uint8_t *const at = polygonBytes->data() + i * kShapePolygonBytes;

			PutInt(at, polygon.engineType);
			PutInt(at + 4, polygon.normalIndex);
			PutInt(at + 8, polygon.flags);
			PutInt(at + 12, polygon.colour);

			for (int c = 0; c < 5; c++)
			{
				PutInt(at + 16 + 4 * c, c < polygon.cornerCount ? polygon.corners[c] : -1);
			}
		}

		return ShapeModelResult::Ok;
	}

	ShapeModelResult WriteShapeNormals(std::vector<ShapeNormal> const &normals, std::vector<uint8_t> *normalBytes)
	{
		if (!normalBytes)
		{
			return ShapeModelResult::BadCall;
		}

		if (normalBytes->size() % kShapeNormalBytes != 0 || normalBytes->size() / kShapeNormalBytes != normals.size())
		{
			return ShapeModelResult::WrongCount;
		}

		for (size_t i = 0; i < normals.size(); i++)
		{
			if (!isfinite(normals[i].x) || !isfinite(normals[i].y) || !isfinite(normals[i].z))
			{
				return ShapeModelResult::NotFinite;
			}
		}

		for (size_t i = 0; i < normals.size(); i++)
		{
			PutFloat(normalBytes->data() + i * kShapeNormalBytes, normals[i].x);
			PutFloat(normalBytes->data() + i * kShapeNormalBytes + 4, normals[i].y);
			PutFloat(normalBytes->data() + i * kShapeNormalBytes + 8, normals[i].z);
		}

		return ShapeModelResult::Ok;
	}

	ShapeModelResult WriteShapeHeader(ShapeModel const &model, std::vector<uint8_t> *headerBytes)
	{
		if (!headerBytes)
		{
			return ShapeModelResult::BadCall;
		}

		if (headerBytes->size() < kHeaderFixedBytes)
		{
			return ShapeModelResult::HeaderTooShort;
		}

		if (!isfinite(model.radius))
		{
			return ShapeModelResult::NotFinite;
		}

		// The counts the header already holds have to be the model's: an edit that changes a count is a different chunk
		if ((size_t)Int(headerBytes->data() + kHeaderPointsAt) != model.points.size() || (size_t)Int(headerBytes->data() + kHeaderPolygonsAt) != model.polygons.size())
		{
			return ShapeModelResult::WrongCount;
		}

		uint8_t *const header = headerBytes->data();

		PutInt(header + kHeaderFlagsAt, model.flags);
		PutInt(header + kHeaderNumberAt, model.number);
		PutFloat(header + kHeaderRadiusAt, model.radius);
		PutInt(header + kHeaderExtentsAt, model.extents.maxX);
		PutInt(header + kHeaderExtentsAt + 4, model.extents.minX);
		PutInt(header + kHeaderExtentsAt + 8, model.extents.maxY);
		PutInt(header + kHeaderExtentsAt + 12, model.extents.minY);
		PutInt(header + kHeaderExtentsAt + 16, model.extents.maxZ);
		PutInt(header + kHeaderExtentsAt + 20, model.extents.minZ);
		return ShapeModelResult::Ok;
	}

	ShapeModelResult WriteShapeHeaderSettingCounts(ShapeModel const &model, std::vector<uint8_t> *headerBytes)
	{
		if (!headerBytes)
		{
			return ShapeModelResult::BadCall;
		}

		if (headerBytes->size() < kHeaderFixedBytes)
		{
			return ShapeModelResult::HeaderTooShort;
		}

		if (model.points.size() > kMostShapePoints || model.polygons.size() > kMostShapePolygons)
		{
			return ShapeModelResult::TooBig;
		}

		PutInt(headerBytes->data() + kHeaderPointsAt, (int32_t)model.points.size());
		PutInt(headerBytes->data() + kHeaderPolygonsAt, (int32_t)model.polygons.size());
		return WriteShapeHeader(model, headerBytes);
	}
}
