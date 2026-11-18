// c_shape_text.cpp: see c_shape_text.h. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "c_shape_text.h"

#include <math.h>
#include <stdio.h>

#include <map>

namespace model
{
	namespace
	{
		// __ftol: toward zero into 64 bits, the low 32 kept; NaN or past 64 bits: the x87's indefinite, whose low 32 bits are 0
		int32_t Ftol(double value)
		{
			if (!(value > -9.2e18 && value < 9.2e18))
			{
				return 0;
			}

			return (int32_t)(uint32_t)(uint64_t)(int64_t)value;
		}

		std::string Number(int64_t value)
		{
			return std::to_string(value);
		}

		// FUN_0042bce0: the textured item types
		bool Textured(int32_t type)
		{
			return type == 5 || type == 6 || type == 7 || (type >= 0x15 && type <= 0x19);
		}

		// 0x5eb968: 26 names of 0x28 bytes, by polygon +0
		char const *const kItemTypeNames[26] = {
			"I_Pixel", "I_Line", "I_Polygon", "I_GouraudPolygon", "I_PhongPolygon", "I_2dTexturedPolygon", "I_Gouraud2dTexturedPolygon", "I_3dTexturedPolygon",
			"I_UnscaledSprite", "I_ScaledSprite", "I_SimpleShadedSphere", "I_ShadedSphere", "I_Polygon_ZBuffer", "I_GouraudPolygon_ZBuffer", "I_Polyline",
			"I_FilledPolyline", "I_Wireframe", "I_Pad3", "I_ZB_Polygon", "I_ZB_GouraudPolygon", "I_ZB_PhongPolygon", "I_ZB_2dTexturedPolygon",
			"I_ZB_Gouraud2dTexturedPolygon", "I_ZB_3dTexturedPolygon", "I_Gouraud3dTexturedPolygon", "I_ZB_Gouraud3dTexturedPolygon",
		};

		// B3: a picture name between C quotes: ", \ and every byte outside 32-126 escaped (inteng writes it as it is)
		std::string Quoted(std::string const &name)
		{
			std::string out;

			for (unsigned char const c : name)
			{
				if (c == '"' || c == '\\')
				{
					out += '\\';
					out += (char)c;
				}
				else if (c < 32 || c > 126)
				{
					char octal[8];

					snprintf(octal, sizeof(octal), "\\%03o", (unsigned)c);
					out += octal;
				}
				else
				{
					out += (char)c;
				}
			}

			return out;
		}
	}

	std::string CShapeFlagText(uint32_t flags, bool forDefine)
	{
		if (flags == 0 && forDefine)
		{
			return "0";   // 0x503735
		}

		// 0x50375c-0x503d43, in the code's order: 0x8 and 0x40 are each tested twice
		struct Word
		{
			uint32_t bit;
			char const *text;
		};

		static Word const words[] = {
			{ 0x1, "|iflag_notvis" }, { 0x2, "|iflag_nolight" }, { 0x4, "|iflag_ignore0" }, { 0x8, "|iflag_shadow" }, { 0x10, "|iflag_transparent" },
			{ 0x20, "|iflag_no_bfc" }, { 0x40, "|iflag_hazing" }, { 0x80, "|iflag_zbuffer_w" }, { 0x8, "|iflag_shadingtable" }, { 0x40, "|iflag_extended" },
			{ 0x800, "|iflag_verticaledges" }, { 0x1000, "|iflag_dormant" }, { 0x2000, "|iflag_viewdotpos" }, { 0x4000, "|iflag_hue_per_vertex" },
			{ 0x8000, "|iflag_no_mip" }, { 0x20000, "|iflag_linear" }, { 0x40000, "|iflag_sortnearz" }, { 0x80000, "|iflag_detail" },
			{ 0x100000, "|iflag_dtest_not_done" }, { 0x200000, "|iflag_augz_planetest" }, { 0x400000, "|iflag_tx2dor3d" }, { 0x800000, "|iflag_linear_s" },
			{ 0x1000000, "|iflag_gsort_ptest" }, { 0x2000000, "|iflag_drawtx3das2d" }, { 0x4000000, "|iflag_sortfarz" }, { 0x8000000, "|iflag_bufferxy" },
			{ 0x10000000, "|iflag_clampz" }, { 0x20000000, "|iflag_cwise" }, { 0x40000000, "|iflag_txanim" }, { 0x80000000u, "|iflag_txanimavi" },
		};
		std::string out;

		for (Word const &word : words)
		{
			if (flags & word.bit)
			{
				out += word.text;
			}
		}

		if (forDefine && !out.empty())
		{
			out[0] = ' ';   // 0x503d7e
		}

		return out;
	}

	std::string CShapeItemTypeName(int32_t type)
	{
		return type >= 0 && type < 26 ? std::string(kItemTypeNames[type]) : Number(type);   // B4
	}

	std::string CShapeText(CShapeSource const &source)
	{
		std::string const &n = source.name;
		ShapeModel const &shape = source.shape;
		size_t const polygonCount = shape.polygons.size();
		std::string out;

		// the pre-passes: the common flags, the plain colours, each list's polygon (tbl), the pictures' local numbers
		uint32_t common = 0xffffffffu;

		for (ShapePolygon const &p : shape.polygons)
		{
			common &= (uint32_t)p.flags;
		}

		std::vector<uint32_t> colours;
		std::vector<size_t> colourOf(polygonCount, 0);

		for (size_t i = 0; i < polygonCount; i++)
		{
			if (Textured(shape.polygons[i].engineType))
			{
				continue;
			}

			uint32_t const c = (uint32_t)shape.polygons[i].colour & 0xffffff;   // 0x50464f: the top byte dropped
			size_t at = 0;

			while (at < colours.size() && colours[at] != c)
			{
				at++;
			}

			if (at == colours.size())
			{
				colours.push_back(c);
			}

			colourOf[i] = at;
		}

		auto const animated = [&](size_t polygon)
		{
			for (AnimPolygon const &a : source.anim.polygons)
			{
				if (a.polygon == (int32_t)polygon)
				{
					return true;
				}
			}

			return false;
		};

		std::vector<int64_t> tbl(source.uvLists.size(), 0);   // polygon + 1, or -(polygon + 1) when animated; the last polygon using the list wins

		for (size_t i = 0; i < polygonCount; i++)
		{
			uint32_t const list = (uint32_t)shape.polygons[i].colour >> 16;

			if (Textured(shape.polygons[i].engineType) && list < tbl.size())   // B6: a list past the lists is left out of the table
			{
				tbl[list] = animated(i) ? -(int64_t)(i + 1) : (int64_t)(i + 1);
			}
		}

		std::map<int32_t, size_t> localOf;   // a picture's local number, in first use: the polygons, then the frames
		std::vector<int32_t> localPictures;
		auto const local = [&](int32_t picture)
		{
			auto const found = localOf.find(picture);

			if (found != localOf.end())
			{
				return found->second;
			}

			localOf[picture] = localPictures.size();
			localPictures.push_back(picture);
			return localPictures.size() - 1;
		};

		for (ShapePolygon const &p : shape.polygons)
		{
			if (Textured(p.engineType))
			{
				local((int32_t)((uint32_t)p.colour & 0xffff));
			}
		}

		for (AnimPolygon const &a : source.anim.polygons)
		{
			for (AnimSequence const &s : a.sequences)
			{
				for (int32_t const picture : s.textures)
				{
					local(picture);
				}
			}
		}

		bool const pictures = !localPictures.empty();
		double const scale = source.scale;

		// 1. the objects that use the shape
		out += "/* object locations:\n";

		for (CShapeUser const &user : source.users)
		{
			out += "{" + Number(Ftol(user.x * scale)) + "," + Number(Ftol(user.y * scale)) + "," + Number(Ftol(user.z * scale)) + "}  " + user.name + "\n";
		}

		out += "*/\n";

		// 2-6. the includes, the defines, the declarations
		out += "#include <stdio.h>\n\n#include \"system.h\"\n#include \"shape.h\"\n#include \"equates.h\"\n\n";
		out += "#define " + n + "_pscale 1\n";
		out += "#define " + n + "_flags " + CShapeFlagText(common, true) + "\n\n";

		for (size_t i = 0; i < colours.size(); i++)
		{
			out += "#define " + n + "_colour" + Number((int64_t)i) + " col24(" + Number((colours[i] >> 16) & 0xff) + "," + Number((colours[i] >> 8) & 0xff) + "," +
				Number(colours[i] & 0xff) + ")\n";
		}

		out += "\nint * " + n + "_points[];\n" + "int * " + n + "_normals[];\n" + "int * " + n + "_vnormals[];\n" + "int * " + n + "_items[];\n\n";
		out += "char * " + n + "_localtextures[];\n" + "int * " + n + "_textures[];\n\n" + "SHAPEINSTR " + n + "_instructions[];\n\n";
		out += "int  " + n + "_points0[];\n" + "int  " + n + "_normals0[];\n" + "int  " + n + "_vnormals0[];\n\n";

		// 7. the header
		std::string const pointCount = Number((int64_t)shape.points.size()), polygonCountText = Number((int64_t)polygonCount);

		out += "SHAPEHEADER " + n + "_header=\n{\n";
		out += "\t" + pointCount + ",\n\t" + polygonCountText + ",\n\t0,\n";
		out += "\t&" + n + "_points[0],\n\t&" + n + "_items[0],\n\t&" + n + "_normals[0],\n\t&" + n + "_vnormals[0],\n";
		out += pictures ? "\t&" + n + "_textures[0],\n\t&" + n + "_localtextures[0],\n" : std::string("\t0,\n\t0,\n");
		out += "\t0,\n\t0,\n\t0,\n\t0,\n";

		ShapeExtents const &e = shape.extents;
		double const header[7] = { (double)shape.radius, (double)e.maxX, (double)e.minX, (double)e.maxY, (double)e.minY, (double)e.maxZ, (double)e.minZ };

		for (double const value : header)
		{
			out += "\t" + Number(Ftol(value * scale)) + "*" + n + "_pscale,\n";
		}

		out += "\t&" + n + "_instructions[0],\n\t0,\n\t0\n};\n\n";

		// 8. the instructions
		out += "SHAPEINSTR " + n + "_instructions[]=\n{\n";
		out += "\tI_ShapePoints," + pointCount + ",&" + n + "_points[0],\n";
		out += "\tI_ShapeNormals," + polygonCountText + ",&" + n + "_normals[0],\n";
		out += "\tI_ShapeProject," + pointCount + ",&" + n + "_points[0],\n";
		out += "\tI_ShapeVNormals," + pointCount + ",&" + n + "_vnormals[0],\n";
		out += "\tI_ShapeItems," + polygonCountText + ",&" + n + "_items[0],\n";
		out += "\tI_ShapeEnd,0,0\n};\n\n";

		// 9. the points
		out += "int *" + n + "_points[]=\n{\n\t&" + n + "_points0[0]\n};\n\n";
		out += "int " + n + "_points0[]=\n{\n";

		for (ShapePoint const &p : shape.points)
		{
			out += "\t" + Number(Ftol(p.x * scale)) + "*" + n + "_pscale,\t" + Number(Ftol(p.y * scale)) + "*" + n + "_pscale,\t" + Number(Ftol(p.z * scale)) + "*" + n +
				"_pscale,\n";
		}

		out += "};\n\n";

		// 10. the normals (65536ths), the polygons' then the points'
		auto const normals = [&](char const *what, std::vector<ShapeNormal> const &list)
		{
			out += "int *" + n + "_" + what + "[]=\n{\n\t&" + n + "_" + what + "0[0]\n};\n\n";
			out += "int " + n + "_" + what + "0[]=\n{\n";

			for (ShapeNormal const &v : list)
			{
				out += "\t" + Number(Ftol((double)v.x * 65536.0)) + "," + Number(Ftol((double)v.y * 65536.0)) + "," + Number(Ftol((double)v.z * 65536.0)) + ",\n";
			}

			out += "};\n\n";
		};

		normals("normals", shape.polygonNormals);
		normals("vnormals", shape.pointNormals);

		// 11. one item a polygon
		for (size_t i = 0; i < polygonCount; i++)
		{
			ShapePolygon const &p = shape.polygons[i];
			uint32_t const own = (uint32_t)p.flags & ~common;

			out += "int " + n + "_item" + Number((int64_t)i) + "[]=\n{\n";
			out += "\t" + CShapeItemTypeName(p.engineType) + ",";
			out += Number(p.normalIndex) + "*vsize,";

			if (!Textured(p.engineType))
			{
				out += n + "_flags" + CShapeFlagText(own, false) + ",";
				out += n + "_colour" + Number((int64_t)colourOf[i]) + ",\n";
			}
			else
			{
				uint32_t const list = (uint32_t)p.colour >> 16;

				if (list < tbl.size() && tbl[list] < 0)
				{
					out += n + "_flags|iflag_txanim" + CShapeFlagText(own & ~0x40000000u, false) + ",";   // B5: once
					out += "(" + Number(list) + "<<16)+TxLocal,\n";
				}
				else
				{
					out += n + "_flags" + CShapeFlagText(own, false) + ",";
					out += "(" + Number(list) + "<<16)+" + Number((int64_t)localOf[(int32_t)((uint32_t)p.colour & 0xffff)]) + "+TxLocal,\n";
				}
			}

			out += "\t";

			for (int c = 0; c < p.cornerCount && c < 5; c++)
			{
				out += Number(p.corners[c]) + "*vsize,";
			}

			out += "\n\t-1\n};\n";
		}

		out += "int *" + n + "_items[]=\n{\n";

		for (size_t i = 0; i < polygonCount; i++)
		{
			out += "\t&" + n + "_item" + Number((int64_t)i) + "[0],\n";
		}

		out += "};\n\n";

		if (!pictures)
		{
			return out;   // 0x505e2a: no picture used, the rest is not written
		}

		// 13. the animations
		for (AnimPolygon const &a : source.anim.polygons)
		{
			std::string const item = n + "_Item" + Number(a.polygon);
			int64_t const corners = a.numVerts > 0 ? a.numVerts : 0;

			for (size_t s = 0; s < a.sequences.size(); s++)
			{
				AnimSequence const &sequence = a.sequences[s];
				std::string const seq = item + "Seq" + Number((int64_t)s);
				size_t const frames = sequence.Frames();

				for (size_t f = 0; f < frames; f++)
				{
					out += "int " + seq + "Frame" + Number((int64_t)f) + "UVCoords[]=\n{\n\t";

					for (int64_t k = 0; k < 2 * corners; k++)
					{
						size_t const at = (size_t)(2 * corners) * f + (size_t)k;   // B1: 2n numbers a frame

						out += Number(at < sequence.uv.size() ? sequence.uv[at] : 0) + ",";
					}

					out += "\n};\n";
				}

				out += "TXANIMFRAME " + seq + "FrameArray[]=\n{\n";

				for (size_t f = 0; f < frames; f++)
				{
					out += "\t{\n\t\t0,\n\t\t0,0,0,\n\t\t0,0,0,\n";
					out += "\t\t" + Number(corners) + ",\n";
					out += "\t\t" + seq + "Frame" + Number((int64_t)f) + "UVCoords,\n";
					out += "\t\t" + Number((int64_t)localOf[sequence.textures[f]]) + " + TxLocal,\n\t},\n";
				}

				out += "};\n";
				out += "int " + seq + "[]=\n{\n";
				out += "\ttxa_flag_play|" + Number(sequence.flags) + ",\n";
				out += "\t0,\n";
				out += "\t" + Number((int64_t)frames + 1) + ",\n";
				out += "\t(int)" + seq + "FrameArray,\n";
				out += "\t0,\n";
				out += "\t" + Number((int64_t)frames) + "<<16,\n";
				out += "\t" + Number(sequence.speed) + ",\n";
				out += "\t0,0,0,\n};\n\n";
			}

			out += "int " + item + "AnimHeaderArray[]=\n{\n\t0,\n";

			for (size_t s = 0; s < a.sequences.size(); s++)
			{
				out += "\t(int)" + item + "Seq" + Number((int64_t)s) + ",\n";
			}

			out += "\t0,\n};\n\n";
		}

		// 14. the lists of texture coordinates, and the table of them
		for (size_t i = 0; i < tbl.size(); i++)
		{
			if (tbl[i] <= 0)
			{
				continue;
			}

			UvList const &list = source.uvLists[i];
			int const count = list.cornerCount > 0 && list.cornerCount <= kMostUvCorners ? list.cornerCount : 0;

			out += "int " + n + "_Item" + Number(tbl[i] - 1) + "UVCoordArray[" + Number(2 * count) + "]=\n{\n\t";

			for (int c = 0; c < count; c++)
			{
				out += Number(Ftol(list.corners[c].u)) + "," + Number(Ftol(list.corners[c].v)) + ",";
			}

			out += "\n};\n";
		}

		out += "\nint *" + n + "_textures[]=\n{\n";

		for (int64_t const entry : tbl)
		{
			out += entry > 0 ? "\t" + n + "_Item" + Number(entry - 1) + "UVCoordArray,\n" : entry < 0 ? "\t" + n + "_Item" + Number(-entry - 1) + "AnimHeaderArray,\n" :
				std::string("\t0,\n");
		}

		out += "\t0\n};\n\n";

		// 15. the pictures' names, by local number (B2: every one defined, "" when BMPNAMES does not name it or there is none)
		for (size_t j = 0; j < localPictures.size(); j++)
		{
			std::string name;

			for (CShapePicture const &picture : source.pictures)
			{
				if (picture.number == localPictures[j])
				{
					name = picture.name;
					break;
				}
			}

			out += "char " + n + "_texture" + Number((int64_t)j) + "[]={\"" + Quoted(name) + "\"};\n";
		}

		out += "char *" + n + "_localtextures[]=\n{\n";

		for (size_t j = 0; j < localPictures.size(); j++)
		{
			out += "\t" + n + "_texture" + Number((int64_t)j) + ",\n";
		}

		out += "\t0\n};\n";
		return out;
	}
}
