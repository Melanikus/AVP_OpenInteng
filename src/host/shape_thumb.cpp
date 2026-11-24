// See shape_thumb.h. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shape_thumb.h"

#include <math.h>

#include <map>

#include "level_file_name.h"   // LevelFileName_WhyRefused: the level open's own rule for a texture's name (avpx)
#include "shape_import.h"   // shape_chunks: the hardened chunk walk, and the texture list's reader
#include "shape_model.h"    // model::ReadShapeModel
#include "uv_model.h"       // model::ReadUvLists, the polygon's colour word

namespace host
{
	using namespace shape_chunks;

	namespace
	{
		uint8_t const *PayloadAt(Bytes const &bytes, Span const &span)
		{
			return (uint8_t const *)bytes.data() + span.at + kHeadingBytes;
		}

		size_t PayloadSize(Span const &span)
		{
			return span.size - kHeadingBytes;
		}

		void Fail(std::string *why, std::string const &words)
		{
			if (why)
			{
				*why = words;
			}
		}
	}

	bool ShapeThumb_Read(std::vector<char> const &file, ShapeThumbModel *model, std::string *why, bool *noGeometry)
	{
		*model = ShapeThumbModel();

		bool ignored = false;

		noGeometry = noGeometry ? noGeometry : &ignored;
		*noGeometry = false;   // said true only at the three refusals below that mean "nothing to draw", never for a broken or too big file

		size_t total = 0;
		std::vector<Span> top;

		if (file.size() < kHeadingBytes || !NameIs(file, 0, "REBINFF2") || !ChunkSizeAt(file, 0, file.size(), &total) || !Children(file, kHeadingBytes, total, &top))
		{
			Fail(why, "not a rif file (its chunks can't be walked: a size runs past its end)");
			return false;
		}

		size_t shapeAt = top.size(), envAt = top.size();

		for (size_t i = 0; i < top.size(); i++)
		{
			if (NameIs(file, top[i].at, "REBSHAPE"))
			{
				shapeAt = shapeAt == top.size() ? i : shapeAt;
				model->parts++;
			}
			else if (NameIs(file, top[i].at, "REBENVDT") && envAt == top.size())
			{
				envAt = i;
			}
		}

		if (shapeAt == top.size())
		{
			Fail(why, "the file holds no shape");
			*noGeometry = true;
			return false;
		}

		std::vector<Span> parts;

		if (!Children(file, top[shapeAt].at + kHeadingBytes, top[shapeAt].at + top[shapeAt].size, &parts))
		{
			Fail(why, "its shape's chunks can't be walked (a size runs past its shape)");
			return false;
		}

		size_t const head = FirstNamed(file, parts, "SHPHEAD1"), points = FirstNamed(file, parts, "SHPRAWVT"), polygons = FirstNamed(file, parts, "SHPPOLYS");
		size_t const polyNormals = FirstNamed(file, parts, "SHPPNORM"), pointNormals = FirstNamed(file, parts, "SHPVNORM"), uvs = FirstNamed(file, parts, "SHPUVCRD");

		if (head == parts.size() || points == parts.size() || polygons == parts.size())
		{
			Fail(why, "its shape has no header, points or polygons");
			*noGeometry = true;
			return false;
		}

		// the limits first, from the chunks' own sizes (the model's reader then checks the header's counts against them)
		size_t const pointCount = PayloadSize(parts[points]) / model::kShapePointBytes, polygonCount = PayloadSize(parts[polygons]) / model::kShapePolygonBytes;

		if (pointCount > kShapeThumbMostPoints || polygonCount > kShapeThumbMostPolygons)
		{
			Fail(why, "it is too big to draw small: " + std::to_string((unsigned long long)pointCount) + " points and " + std::to_string((unsigned long long)polygonCount) +
				" polygons (a picture draws at most " + std::to_string((unsigned long long)kShapeThumbMostPoints) + " and " +
				std::to_string((unsigned long long)kShapeThumbMostPolygons) + ")");
			return false;
		}

		model::ShapeBytes b = {};

		b.header = PayloadAt(file, parts[head]);
		b.headerSize = PayloadSize(parts[head]);
		b.points = PayloadAt(file, parts[points]);
		b.pointsSize = PayloadSize(parts[points]);
		b.polygons = PayloadAt(file, parts[polygons]);
		b.polygonsSize = PayloadSize(parts[polygons]);

		if (polyNormals != parts.size())
		{
			b.polyNormals = PayloadAt(file, parts[polyNormals]);
			b.polyNormalsSize = PayloadSize(parts[polyNormals]);
		}

		if (pointNormals != parts.size())
		{
			b.ptNormals = PayloadAt(file, parts[pointNormals]);
			b.ptNormalsSize = PayloadSize(parts[pointNormals]);
		}

		model::ShapeModel shape;
		model::ShapeModelResult const read = model::ReadShapeModel(b, &shape);

		if (read != model::ShapeModelResult::Ok)
		{
			Fail(why, std::string("its shape is refused: ") + model::ShapeModelWords(read));
			return false;
		}

		if (shape.points.empty())
		{
			Fail(why, "its shape has no points");
			*noGeometry = true;
			return false;
		}

		model->shapeFlags = shape.flags;   // (what the game draws the shape by: part of the look, ShapeThumb_LookBytes)

		// the texture coordinates and the texture list: a fault in either draws polygons shaded, it never refuses the file
		std::vector<model::UvList> lists;

		if (uvs != parts.size() && model::ReadUvLists(PayloadAt(file, parts[uvs]), PayloadSize(parts[uvs]), &lists) != model::UvModelResult::Ok)
		{
			lists.clear();
		}

		std::vector<BitmapEntry> entries;
		std::string riffName;

		if (envAt != top.size())
		{
			std::vector<Span> env;

			if (Children(file, top[envAt].at + kHeadingBytes, top[envAt].at + top[envAt].size, &env))
			{
				size_t const names = FirstNamed(file, env, "BMPNAMES"), rif = FirstNamed(file, env, "RIFFNAME");
				int32_t headWord = 0;

				if (names == env.size() || !ReadBitmaps(file, env[names], &headWord, &entries))
				{
					entries.clear();
				}

				if (rif != env.size() && env[rif].size > kHeadingBytes)
				{
					char const *const from = file.data() + env[rif].at + kHeadingBytes;
					void const *const zero = memchr(from, 0, PayloadSize(env[rif]));

					riffName = zero ? std::string(from, (char const *)zero) : std::string();
				}
			}
		}

		// a list entry's own index -> its place in entries, as the game's loop leaves it (setup_tex_conv_array: each entry in order sets its index's picture, so
		// the LAST of an index wins; an entry not for the PC sets nothing, so its polygons are drawn without a picture)
		std::map<int32_t, size_t> entryOfIndex;
		std::map<int32_t, int> pictureOfIndex;    // -> its place in model->pictures, once used

		for (size_t i = 0; i < entries.size(); i++)
		{
			if (!(entries[i].flags & kShapeThumbFlagNotInPc))
			{
				entryOfIndex[entries[i].index] = i;
			}
		}

		// the points, and their box for the camera
		model->points.reserve(shape.points.size() * 3);

		float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };

		for (model::ShapePoint const &p : shape.points)
		{
			float const c[3] = { (float)p.x, (float)p.y, (float)p.z };

			for (int a = 0; a < 3; a++)
			{
				model->points.push_back(c[a]);
				lo[a] = c[a] < lo[a] ? c[a] : lo[a];
				hi[a] = c[a] > hi[a] ? c[a] : hi[a];
			}
		}

		for (int a = 0; a < 3; a++)
		{
			model->centre[a] = (lo[a] + hi[a]) / 2.0f;
		}

		for (size_t i = 0; i < shape.points.size(); i++)
		{
			float const dx = model->points[i * 3] - model->centre[0], dy = model->points[i * 3 + 1] - model->centre[1], dz = model->points[i * 3 + 2] - model->centre[2];
			float const d = sqrtf(dx * dx + dy * dy + dz * dz);

			model->radius = d > model->radius ? d : model->radius;
		}

		// the polygons, each with its picture when it has one the game would find
		for (model::ShapePolygon const &p : shape.polygons)
		{
			ShapeThumbPolygon out;

			out.cornerCount = p.cornerCount;
			out.engineType = p.engineType;   // (what the game draws it by: part of the look, ShapeThumb_LookBytes)
			out.flags = p.flags;

			for (int c = 0; c < p.cornerCount && c < 5; c++)
			{
				out.corners[c] = p.corners[c];
			}

			int32_t const number = model::PolygonPictureNumber(p.colour), list = model::PolygonUvListNumber(p.colour);
			auto const entry = entryOfIndex.find(number);

			if (model::EngineTypeIsTextured(p.engineType) && entry != entryOfIndex.end() && list >= 0 && (size_t)list < lists.size() &&
				lists[(size_t)list].cornerCount >= p.cornerCount)
			{
				auto known = pictureOfIndex.find(number);

				if (known == pictureOfIndex.end())
				{
					BitmapEntry const &e = entries[entry->second];
					ShapeThumbPicture picture;

					picture.index = e.index;
					picture.flags = e.flags;
					picture.name = e.name;

					// THE PATH IS THE FILE'S OWN BYTES (Frost's review of 4f0db774, MUST 1): held to the very rule the level's open holds every texture name to
					// (avpx level_file_name.h, as projload.cpp load_rif_bitmap does), so a stranger's shape can't name "..\..\x", "C:\x", "a.rim:stream" or a
					// "\\server\share" path (opening a network path can hand the player's Windows sign-in to that server). A refused one is never read: its
					// polygons are drawn shaded, and why is kept for the tile's words.
					std::string const path = (e.flags & kShapeThumbFlagIff) ? e.name : riffName + "/" + e.name;
					char const *const refused = LevelFileName_WhyRefused(path.c_str());

					if (refused)
					{
						picture.refused = refused;
					}
					else
					{
						picture.path = path;
					}

					known = pictureOfIndex.emplace(number, (int)model->pictures.size()).first;
					model->pictures.push_back(picture);
				}

				out.picture = known->second;

				for (int c = 0; c < p.cornerCount && c < model::kMostUvCorners; c++)
				{
					out.texels[c][0] = lists[(size_t)list].corners[c].u;
					out.texels[c][1] = lists[(size_t)list].corners[c].v;
				}
			}

			model->polygons.push_back(out);
		}

		return true;
	}

	std::string ShapeThumb_LookBytes(ShapeThumbModel const &model)
	{
		if (model.parts != 1)
		{
			return std::string();   // a fragment file: only its first part is drawn, so two that look the same here may differ in the rest
		}

		std::string out;

		// a number's exact bytes (a float's own bits: 1 mm off is another look)
		auto add = [&out](void const *what, size_t bytes) { out.append((char const *)what, bytes); };
		uint32_t const pointValues = (uint32_t)model.points.size();

		out += "shape";
		add(&model.shapeFlags, sizeof(model.shapeFlags));   // what the game draws the shape by (Dietrich's SHOULD 1 on 7ad5b295)
		out += "points";
		add(&pointValues, sizeof(pointValues));
		add(model.points.data(), model.points.size() * sizeof(float));

		uint32_t const polygonCount = (uint32_t)model.polygons.size();

		out += "polygons";
		add(&polygonCount, sizeof(polygonCount));

		for (ShapeThumbPolygon const &p : model.polygons)
		{
			int32_t const count = p.cornerCount;

			add(&p.engineType, sizeof(p.engineType));   // and each polygon by its item type and flags: see-through, translucent, not culled ...
			add(&p.flags, sizeof(p.flags));
			add(&count, sizeof(count));

			for (int c = 0; c < p.cornerCount && c < 5; c++)
			{
				add(&p.corners[c], sizeof(p.corners[c]));
				add(p.texels[c], sizeof(p.texels[c]));
			}

			// its picture by its path in small letters (Windows and the game's archives read a name in any letters as the same file), never by the list's own
			// number, which another level's copy may give otherwise; "" for a shaded polygon or a refused path, each ended with a 0 so no two run together
			std::string path = p.picture >= 0 && (size_t)p.picture < model.pictures.size() && model.pictures[(size_t)p.picture].refused.empty() ?
				model.pictures[(size_t)p.picture].path : std::string();

			for (char &letter : path)
			{
				letter = letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter;
			}

			out += path;
			out += '\0';
		}

		return out;
	}

	void ShapeThumb_Light(float toLight[3])
	{
		// the camera's way out (0.5, -0.707, 0.5: up is -y), a little to its left (0.707, 0, -0.707) and higher
		float const v[3] = { 0.5f + 0.35f, -0.7071068f - 0.3f, 0.5f - 0.35f };
		float const length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

		for (int a = 0; a < 3; a++)
		{
			toLight[a] = v[a] / length;
		}
	}

	std::vector<ShapeThumbBatch> ShapeThumb_Triangles(ShapeThumbModel const &model, std::vector<std::pair<uint32_t, uint32_t> > const &sizes)
	{
		std::vector<ShapeThumbBatch> batches;
		std::map<int, size_t> batchOf;
		float light[3];
		size_t const pointCount = model.points.size() / 3;

		ShapeThumb_Light(light);

		for (ShapeThumbPolygon const &p : model.polygons)
		{
			bool good = p.cornerCount >= 3 && p.cornerCount <= 5;

			for (int c = 0; good && c < p.cornerCount; c++)
			{
				good = p.corners[c] >= 0 && (size_t)p.corners[c] < pointCount;   // (the model's reader checked each already: held here too, as this is what reads them)
			}

			if (!good)
			{
				continue;
			}

			// its face's turn, by Newell's sum over its corners (any corner count, even a slightly bent quad), both sides alike
			float n[3] = { 0, 0, 0 };

			for (int c = 0; c < p.cornerCount; c++)
			{
				float const *const a = &model.points[(size_t)p.corners[c] * 3];
				float const *const b = &model.points[(size_t)p.corners[(c + 1) % p.cornerCount] * 3];

				n[0] += (a[1] - b[1]) * (a[2] + b[2]);
				n[1] += (a[2] - b[2]) * (a[0] + b[0]);
				n[2] += (a[0] - b[0]) * (a[1] + b[1]);
			}

			float const length = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
			float const facing = length > 0 ? fabsf((n[0] * light[0] + n[1] * light[1] + n[2] * light[2]) / length) : 0.0f;
			float const shade = 0.35f + 0.65f * facing;

			int const picture = p.picture >= 0 && (size_t)p.picture < sizes.size() && sizes[(size_t)p.picture].first > 0 && sizes[(size_t)p.picture].second > 0 ? p.picture : -1;
			auto known = batchOf.find(picture);

			if (known == batchOf.end())
			{
				known = batchOf.emplace(picture, batches.size()).first;
				batches.push_back(ShapeThumbBatch());
				batches.back().picture = picture;
			}

			ShapeThumbBatch &batch = batches[known->second];
			float const w = picture >= 0 ? (float)sizes[(size_t)picture].first : 1.0f, h = picture >= 0 ? (float)sizes[(size_t)picture].second : 1.0f;

			for (int c = 1; c + 1 < p.cornerCount; c++)
			{
				for (int k : { 0, c, c + 1 })
				{
					float const *const at = &model.points[(size_t)p.corners[k] * 3];
					ShapeThumbCorner corner;

					corner.x = at[0];
					corner.y = at[1];
					corner.z = at[2];
					corner.shade = shade;
					corner.u = picture >= 0 ? p.texels[k][0] / w : 0.0f;
					corner.v = picture >= 0 ? p.texels[k][1] / h : 0.0f;
					batch.corners.push_back(corner);
				}
			}
		}

		return batches;
	}

	void ShapeThumb_Camera(ShapeThumbModel const &model, float fieldOfView, float eye[3], float at[3])
	{
		// the way out from the middle to the camera: 45 degrees round from +x toward +z, and 45 up (up is -y): (cos 45 cos 45, -sin 45, sin 45 cos 45)
		float const out[3] = { 0.5f, -0.7071068f, 0.5f };
		float const half = fieldOfView > 0.01f && fieldOfView < 3.0f ? fieldOfView / 2.0f : 0.35f;
		float const radius = model.radius > 1.0f ? model.radius : 1.0f;
		float const distance = radius / sinf(half) * 1.05f;

		for (int a = 0; a < 3; a++)
		{
			at[a] = model.centre[a];
			eye[a] = model.centre[a] + out[a] * distance;
		}
	}
}
