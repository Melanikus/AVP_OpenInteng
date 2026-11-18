// The test for src/model/c_shape_text.cpp: inteng's Write C File text (FUN_00503da0, FUN_00503730). Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every expected line below is worked out by hand from AvP-docs\engine\inteng\write-c-file-facts.md's COMPLETED MAP (the format strings in code order,
// listing-all-3.txt 56508-60215), never from the code under test:
//  1. the flag words, both modes, the repeated 0x8 / 0x40 tests and the bits never tested;
//  2. the item type names and B4 (a type outside the table: its number);
//  3. a plain triangle, no pictures, scale 1: the whole file;
//  4. two textured triangles and a plain one, scale 2, a TEXTANIM of two frames, three pictures (one BMPNAMES lacks, one name to escape): the whole file,
//     with B1 (the second frame's numbers at 2n x f), B2 (the unknown picture defined as ""), B3 (the name escaped), B5 (iflag_txanim once);
//  5. B2 with no BMPNAMES at all, B6 (a list number past the shape's lists).

#include <stdio.h>
#include <string.h>

#include <string>

#include "c_shape_text.h"

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checksRun++;

	if (!good)
	{
		checksFailed++;
	}
}

// The whole text against the expected one; on a difference, where it starts
static void CheckText(std::string const &got, std::string const &want, std::string const &what)
{
	size_t at = 0;

	while (at < got.size() && at < want.size() && got[at] == want[at])
	{
		at++;
	}

	bool const same = got == want;

	Check(same, what + (same ? "" : " (differs at byte " + std::to_string(at) + ": got \"" + got.substr(at, 60) + "\", wanted \"" + want.substr(at, 60) + "\")"));
}

static model::ShapePolygon Polygon(int32_t type, int32_t normal, int32_t flags, int32_t colour, int a, int b, int c)
{
	model::ShapePolygon p = {};

	p.engineType = type;
	p.normalIndex = normal;
	p.flags = flags;
	p.colour = colour;
	p.corners[0] = a;
	p.corners[1] = b;
	p.corners[2] = c;
	p.corners[3] = -1;
	p.corners[4] = -1;
	p.cornerCount = 3;
	return p;
}

static model::UvList List3(float u0, float v0, float u1, float v1, float u2, float v2)
{
	model::UvList l = {};

	l.cornerCount = 3;
	l.corners[0].u = u0;
	l.corners[0].v = v0;
	l.corners[1].u = u1;
	l.corners[1].v = v1;
	l.corners[2].u = u2;
	l.corners[2].v = v2;
	return l;
}

static std::string const kIncludes = "#include <stdio.h>\n\n#include \"system.h\"\n#include \"shape.h\"\n#include \"equates.h\"\n\n";

static std::string Declarations(std::string const &n)
{
	return "\nint * " + n + "_points[];\n" + "int * " + n + "_normals[];\n" + "int * " + n + "_vnormals[];\n" + "int * " + n + "_items[];\n\n" + "char * " + n +
		"_localtextures[];\n" + "int * " + n + "_textures[];\n\n" + "SHAPEINSTR " + n + "_instructions[];\n\n" + "int  " + n + "_points0[];\n" + "int  " + n +
		"_normals0[];\n" + "int  " + n + "_vnormals0[];\n\n";
}

int main()
{
	printf("c_shape_text_test\n");

	// 1. the flag words
	Check(model::CShapeFlagText(0, true) == "0", "flags 0 in the #define: 0");
	Check(model::CShapeFlagText(0, false) == "", "flags 0 in an item: nothing");
	Check(model::CShapeFlagText(0x2, true) == " iflag_nolight", "the #define's first | becomes a space");
	Check(model::CShapeFlagText(0x2, false) == "|iflag_nolight", "an item's words keep their |");
	Check(model::CShapeFlagText(0x8, false) == "|iflag_shadow|iflag_shadingtable", "0x8 is tested twice: shadow, then shadingtable after zbuffer_w");
	Check(model::CShapeFlagText(0x48, false) == "|iflag_shadow|iflag_hazing|iflag_shadingtable|iflag_extended", "0x8 and 0x40, each named twice in inteng's order");
	Check(model::CShapeFlagText(0x100 | 0x200 | 0x400 | 0x10000, true) == "", "bits never tested: no words, not even 0");
	Check(model::CShapeFlagText(0x80000001u, false) == "|iflag_notvis|iflag_txanimavi", "the first and the last bit");
	Check(model::CShapeFlagText(0x40000000, false) == "|iflag_txanim", "0x40000000");

	// 2. the item type names
	Check(model::CShapeItemTypeName(0) == "I_Pixel", "type 0");
	Check(model::CShapeItemTypeName(7) == "I_3dTexturedPolygon", "type 7");
	Check(model::CShapeItemTypeName(17) == "I_Pad3", "type 17");
	Check(model::CShapeItemTypeName(25) == "I_ZB_Gouraud3dTexturedPolygon", "type 25, the last");
	Check(model::CShapeItemTypeName(26) == "26" && model::CShapeItemTypeName(-1) == "-1", "B4: a type outside the table is its number");

	// 3. a plain triangle
	{
		model::CShapeSource s;

		s.name = "box";
		s.users.push_back({ "obj1", 100, -200, 300 });
		s.shape.radius = 10.5f;
		s.shape.extents = { 5, -5, 6, -6, 7, -7 };
		s.shape.points = { { 0, 0, 0 }, { 10, 0, 0 }, { 0, 10, 0 } };
		s.shape.polygons.push_back(Polygon(2, 0, 0x2, 0x01ff8040, 0, 1, 2));
		s.shape.polygonNormals = { { 0.0f, 0.0f, 1.0f } };
		s.shape.pointNormals = { { 0.0f, 0.0f, 1.0f }, { 0.5f, -0.25f, 0.0f }, { 0.0f, 0.0f, -1.0f } };

		std::string const want = std::string("/* object locations:\n") + "{100,-200,300}  obj1\n" + "*/\n" + kIncludes + "#define box_pscale 1\n" +
			"#define box_flags  iflag_nolight\n\n" + "#define box_colour0 col24(255,128,64)\n" + Declarations("box") + "SHAPEHEADER box_header=\n{\n" +
			"\t3,\n\t1,\n\t0,\n" + "\t&box_points[0],\n\t&box_items[0],\n\t&box_normals[0],\n\t&box_vnormals[0],\n" + "\t0,\n\t0,\n" + "\t0,\n\t0,\n\t0,\n\t0,\n" +
			"\t10*box_pscale,\n\t5*box_pscale,\n\t-5*box_pscale,\n\t6*box_pscale,\n\t-6*box_pscale,\n\t7*box_pscale,\n\t-7*box_pscale,\n" +
			"\t&box_instructions[0],\n" + "\t0,\n\t0\n};\n\n" + "SHAPEINSTR box_instructions[]=\n{\n" + "\tI_ShapePoints,3,&box_points[0],\n" +
			"\tI_ShapeNormals,1,&box_normals[0],\n" + "\tI_ShapeProject,3,&box_points[0],\n" + "\tI_ShapeVNormals,3,&box_vnormals[0],\n" +
			"\tI_ShapeItems,1,&box_items[0],\n" + "\tI_ShapeEnd,0,0\n};\n\n" + "int *box_points[]=\n{\n\t&box_points0[0]\n};\n\n" + "int box_points0[]=\n{\n" +
			"\t0*box_pscale,\t0*box_pscale,\t0*box_pscale,\n" + "\t10*box_pscale,\t0*box_pscale,\t0*box_pscale,\n" + "\t0*box_pscale,\t10*box_pscale,\t0*box_pscale,\n" +
			"};\n\n" + "int *box_normals[]=\n{\n\t&box_normals0[0]\n};\n\n" + "int box_normals0[]=\n{\n" + "\t0,0,65536,\n" + "};\n\n" +
			"int *box_vnormals[]=\n{\n\t&box_vnormals0[0]\n};\n\n" + "int box_vnormals0[]=\n{\n" + "\t0,0,65536,\n\t32768,-16384,0,\n\t0,0,-65536,\n" + "};\n\n" +
			"int box_item0[]=\n{\n" + "\tI_Polygon," + "0*vsize," + "box_flags," + "box_colour0,\n" + "\t" + "0*vsize,1*vsize,2*vsize," + "\n\t-1\n};\n" +
			"int *box_items[]=\n{\n" + "\t&box_item0[0],\n" + "};\n\n";

		CheckText(model::CShapeText(s), want, "a plain triangle, scale 1, no pictures: the whole file");
	}

	// 4. textured, animated, scale 2
	{
		model::CShapeSource s;

		s.name = "w";
		s.scale = 2.0;
		s.users.push_back({ "o", 1, 2, 3 });
		s.shape.radius = 1.0f;
		s.shape.extents = { 3, -3, 0, 0, 1, -1 };
		s.shape.points = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		s.shape.polygons.push_back(Polygon(7, 0, 0x40000002, (0 << 16) | 3, 0, 1, 2));
		s.shape.polygons.push_back(Polygon(2, 1, 0x12, 0x000000ff, 2, 1, 0));
		s.shape.polygons.push_back(Polygon(7, 0, 0x2, (1 << 16) | 5, 0, 2, 1));
		s.shape.polygonNormals = { { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } };
		s.shape.pointNormals = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
		s.uvLists = { List3(0, 0, 64, 0, 64, 64), List3(1.9f, 2.1f, 3, 4, -1.5f, 0) };

		model::AnimPolygon animated;
		model::AnimSequence sequence;

		animated.polygon = 0;
		animated.group = 1;
		animated.numVerts = 3;
		sequence.speed = 100;
		sequence.flags = 2;
		sequence.textures = { 3, 7 };
		sequence.uv = { 0, 0, 64, 0, 64, 64, 10, 11, 12, 13, 14, 15 };
		animated.sequences.push_back(sequence);
		s.anim.polygons.push_back(animated);
		s.pictures = { { 3, "WALL.BMP" }, { 5, "a\"b\\c" } };

		std::string const want = std::string("/* object locations:\n") + "{2,4,6}  o\n" + "*/\n" + kIncludes + "#define w_pscale 1\n" + "#define w_flags  iflag_nolight\n\n" +
			"#define w_colour0 col24(0,0,255)\n" + Declarations("w") + "SHAPEHEADER w_header=\n{\n" + "\t3,\n\t3,\n\t0,\n" +
			"\t&w_points[0],\n\t&w_items[0],\n\t&w_normals[0],\n\t&w_vnormals[0],\n" + "\t&w_textures[0],\n\t&w_localtextures[0],\n" + "\t0,\n\t0,\n\t0,\n\t0,\n" +
			"\t2*w_pscale,\n\t6*w_pscale,\n\t-6*w_pscale,\n\t0*w_pscale,\n\t0*w_pscale,\n\t2*w_pscale,\n\t-2*w_pscale,\n" + "\t&w_instructions[0],\n" + "\t0,\n\t0\n};\n\n" +
			"SHAPEINSTR w_instructions[]=\n{\n" + "\tI_ShapePoints,3,&w_points[0],\n" + "\tI_ShapeNormals,3,&w_normals[0],\n" + "\tI_ShapeProject,3,&w_points[0],\n" +
			"\tI_ShapeVNormals,3,&w_vnormals[0],\n" + "\tI_ShapeItems,3,&w_items[0],\n" + "\tI_ShapeEnd,0,0\n};\n\n" + "int *w_points[]=\n{\n\t&w_points0[0]\n};\n\n" +
			"int w_points0[]=\n{\n" + "\t2*w_pscale,\t0*w_pscale,\t0*w_pscale,\n" + "\t0*w_pscale,\t2*w_pscale,\t0*w_pscale,\n" + "\t0*w_pscale,\t0*w_pscale,\t2*w_pscale,\n" +
			"};\n\n" + "int *w_normals[]=\n{\n\t&w_normals0[0]\n};\n\n" + "int w_normals0[]=\n{\n" + "\t0,0,65536,\n\t0,0,-65536,\n\t65536,0,0,\n" + "};\n\n" +
			"int *w_vnormals[]=\n{\n\t&w_vnormals0[0]\n};\n\n" + "int w_vnormals0[]=\n{\n" + "\t65536,0,0,\n\t0,65536,0,\n\t0,0,65536,\n" + "};\n\n" +
			// p0: textured, its list (0) is animated: |iflag_txanim once (B5)
			"int w_item0[]=\n{\n" + "\tI_3dTexturedPolygon," + "0*vsize," + "w_flags|iflag_txanim," + "(0<<16)+TxLocal,\n" + "\t" + "0*vsize,1*vsize,2*vsize," +
			"\n\t-1\n};\n" +
			// p1: plain, its own flag beyond the common one
			"int w_item1[]=\n{\n" + "\tI_Polygon," + "1*vsize," + "w_flags|iflag_transparent," + "w_colour0,\n" + "\t" + "2*vsize,1*vsize,0*vsize," + "\n\t-1\n};\n" +
			// p2: textured, list 1, picture 5 = local 1
			"int w_item2[]=\n{\n" + "\tI_3dTexturedPolygon," + "0*vsize," + "w_flags," + "(1<<16)+1+TxLocal,\n" + "\t" + "0*vsize,2*vsize,1*vsize," + "\n\t-1\n};\n" +
			"int *w_items[]=\n{\n" + "\t&w_item0[0],\n\t&w_item1[0],\n\t&w_item2[0],\n" + "};\n\n" +
			// the animation: frame 1 at 2n x f (B1); picture 7 is local 2
			"int w_Item0Seq0Frame0UVCoords[]=\n{\n\t" + "0,0,64,0,64,64," + "\n};\n" + "int w_Item0Seq0Frame1UVCoords[]=\n{\n\t" + "10,11,12,13,14,15," + "\n};\n" +
			"TXANIMFRAME w_Item0Seq0FrameArray[]=\n{\n" + "\t{\n" + "\t\t0,\n" + "\t\t0,0,0,\n\t\t0,0,0,\n" + "\t\t3,\n" + "\t\tw_Item0Seq0Frame0UVCoords,\n" +
			"\t\t0 + TxLocal,\n\t},\n" + "\t{\n" + "\t\t0,\n" + "\t\t0,0,0,\n\t\t0,0,0,\n" + "\t\t3,\n" + "\t\tw_Item0Seq0Frame1UVCoords,\n" + "\t\t2 + TxLocal,\n\t},\n" +
			"};\n" + "int w_Item0Seq0[]=\n{\n" + "\ttxa_flag_play|2,\n" + "\t0,\n" + "\t3,\n" + "\t(int)w_Item0Seq0FrameArray,\n" + "\t0,\n" + "\t2<<16,\n" + "\t100,\n" +
			"\t0,0,0,\n};\n\n" + "int w_Item0AnimHeaderArray[]=\n{\n\t0,\n" + "\t(int)w_Item0Seq0,\n" + "\t0,\n};\n\n" +
			// the coordinate lists: list 1 (polygon 2's) written, cut toward zero; list 0 is the animation's
			"int w_Item2UVCoordArray[6]=\n{\n\t" + "1,2,3,4,-1,0," + "\n};\n" + "\nint *w_textures[]=\n{\n" + "\tw_Item0AnimHeaderArray,\n" + "\tw_Item2UVCoordArray,\n" +
			"\t0\n};\n\n" +
			// the pictures by local number: 3, 5 (escaped, B3), 7 (BMPNAMES lacks it: "", B2)
			"char w_texture0[]={\"WALL.BMP\"};\n" + "char w_texture1[]={\"a\\\"b\\\\c\"};\n" + "char w_texture2[]={\"\"};\n" + "char *w_localtextures[]=\n{\n" +
			"\tw_texture0,\n\tw_texture1,\n\tw_texture2,\n" + "\t0\n};\n";

		CheckText(model::CShapeText(s), want, "textured, animated, three pictures, scale 2: the whole file");
	}

	// 5. B2 with no BMPNAMES at all; B6 a list number past the lists; a byte outside 32-126 escaped
	{
		model::CShapeSource s;

		s.name = "t";
		s.shape.points = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
		s.shape.polygons.push_back(Polygon(7, 0, 0, (9 << 16) | 4, 0, 1, 2));
		s.shape.polygonNormals = { { 0.0f, 0.0f, 1.0f } };
		s.shape.pointNormals = { { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };

		std::string const text = model::CShapeText(s);

		Check(text.find("(9<<16)+0+TxLocal,\n") != std::string::npos, "B6: a list number past the lists is written as it is, the polygon not animated");
		Check(text.find("\nint *t_textures[]=\n{\n\t0\n};\n\n") != std::string::npos, "B6: no list is listed in t_textures (the shape has none)");
		Check(text.find("char t_texture0[]={\"\"};\nchar *t_localtextures[]=\n{\n\tt_texture0,\n\t0\n};\n") != std::string::npos,
			"B2: with no BMPNAMES the picture is still defined, as \"\"");

		s.pictures = { { 4, std::string("a\nb\x01", 4) } };
		Check(model::CShapeText(s).find("char t_texture0[]={\"a\\012b\\001\"};\n") != std::string::npos, "B3: a new line and a control byte are written as \\ooo");
	}

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
