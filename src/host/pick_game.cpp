// See pick_game.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"         // Player
#include "view.h"            // Global_VDB_Ptr
#include "los.h"             // FindDrawnPolygonInLineOfSight, LOS_ObjectHitPtr, LOS_Lambda
#include "dynamics.h"        // ColPolyTag, SetupPolygonAccess: the game's own walk through a shape's polygons
#include "d3_func.h"         // d3d.matProjection

#include <string.h>

// the frame's list of what is on screen, which the game's line of sight walks (los.cpp declares them the same way)
extern int NumOnScreenBlocks;
extern DISPLAYBLOCK *OnScreenBlockList[];

#include "pick_ray.h"
#include "pick_game.h"
#include "hierarchy_view.h"   // ModelObjects_IdOfBlock: the objects of a file with no modules

// The corner of the polygon the line hit that is nearest the hit point, in the world. It walks the shape's polygons the way the line of sight did
// (SetupPolygonAccess, AccessNextPolygon: CheckForRayIntersectionWithObject in los.cpp), up to the hit one, and reads its corners (GetPolygonVertices) in the shape's own space; a
// module's shape is already in the world apart from its place, anything else is turned by the object's matrix first (the same function).
static bool NearestCorner(const DISPLAYBLOCK *hit, int polygon, const VECTORCH &point, int32_t corner[3])
{
	DISPLAYBLOCK *const block = const_cast<DISPLAYBLOCK *>(hit);

	if (polygon < 0 || SetupPolygonAccess(block) <= polygon)
	{
		return false;
	}

	for (int walked = 0; walked <= polygon; walked++)
	{
		AccessNextPolygon();
	}

	ColPolyTag polyData;
	memset(&polyData, 0, sizeof(polyData));
	GetPolygonVertices(&polyData);

	if (polyData.NumberOfVertices < 1 || polyData.NumberOfVertices > 4)
	{
		return false;
	}

	int64_t best = -1;

	for (int i = 0; i < polyData.NumberOfVertices; i++)
	{
		VECTORCH world = polyData.PolyPoint[i];

		if (!block->ObMyModule)
		{
			MATRIXCH matrix = block->ObMat;
			RotateVector(&world, &matrix);
		}

		world.vx += block->ObWorld.vx;
		world.vy += block->ObWorld.vy;
		world.vz += block->ObWorld.vz;

		int64_t const dx = (int64_t)world.vx - point.vx, dy = (int64_t)world.vy - point.vy, dz = (int64_t)world.vz - point.vz;
		int64_t const away = dx * dx + dy * dy + dz * dz;

		if (best < 0 || away < best)
		{
			best = away;
			corner[0] = world.vx;
			corner[1] = world.vy;
			corner[2] = world.vz;
		}
	}

	return best >= 0;
}

bool PickGame_Camera(int32_t viewWidth, int32_t viewHeight, host::PickCamera *out)
{
	// The camera the frame was drawn from. Before the first level, there's none. Reading it is safe even after a level has closed: the
	// game's camera blocks are one fixed list it hands out and takes back (FreeVDBData, vdb.cpp:20), never memory that's given back.
	// And closing a level empties the list of what's on screen (Destroy_CurrentEnvironment, system.cpp:565), so the line of sight then
	// meets nothing at all, rather than a block that's gone.
	VIEWDESCRIPTORBLOCK const *const viewBlock = Global_VDB_Ptr;

	if (!viewBlock)
	{
		return false;
	}

	host::PickCamera &camera = *out;
	camera.place[0] = viewBlock->VDB_World.vx;
	camera.place[1] = viewBlock->VDB_World.vy;
	camera.place[2] = viewBlock->VDB_World.vz;
	camera.turn[0][0] = viewBlock->VDB_Mat.mat11;
	camera.turn[0][1] = viewBlock->VDB_Mat.mat12;
	camera.turn[0][2] = viewBlock->VDB_Mat.mat13;
	camera.turn[1][0] = viewBlock->VDB_Mat.mat21;
	camera.turn[1][1] = viewBlock->VDB_Mat.mat22;
	camera.turn[1][2] = viewBlock->VDB_Mat.mat23;
	camera.turn[2][0] = viewBlock->VDB_Mat.mat31;
	camera.turn[2][1] = viewBlock->VDB_Mat.mat32;
	camera.turn[2][2] = viewBlock->VDB_Mat.mat33;
	camera.xScale = d3d.matProjection._11;
	camera.yScale = d3d.matProjection._22;
	camera.viewWidth = viewWidth;
	camera.viewHeight = viewHeight;
	return true;
}

bool PickGame_RayAtClick(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight, host::PickRay *ray)
{
	host::PickCamera camera;

	return PickGame_Camera(viewWidth, viewHeight, &camera) && host::PickRay_FromClick(camera, clickX, clickY, ray) == host::PickRayMade::Made;
}

host::PickReport PickGame_AtClick(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight)
{
	return PickGame_AtClickLeavingOut(clickX, clickY, viewWidth, viewHeight, nullptr);
}

host::PickReport PickGame_AtClickLeavingOut(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight, void const *leaveOut)
{
	host::PickReport report;
	memset(&report, 0, sizeof(report));
	report.found = host::PickFound::NoLine;
	report.polygonNumber = -1;

	host::PickRay ray;

	if (!PickGame_RayAtClick(clickX, clickY, viewWidth, viewHeight, &ray))
	{
		return report;
	}

	// The game's line of sight, over only what was on screen this frame (its 1), and never the player itself. It's handed copies: its
	// own notes say it may change what it's given.
	// ONLY THE POLYGONS THAT ARE DRAWN count, as in inteng's one pick for every mode (FUN_00559750: it walks the polygons it drew last frame,
	// DAT_00703cf4 / DAT_009ba168, never one with item flag 1, iflag_notvis): a face seen from behind is culled, so it is not there to be clicked
	// unless it is double-sided (iflag_no_bfc). The game's own line of sight takes every face from both sides ("treat all polys as no bfc",
	// los.cpp), which stopped our clicks on faces nobody sees (a Hierarchy click on the left arm gave RArm, a section seen through).
	VECTORCH direction = { ray.direction[0], ray.direction[1], ray.direction[2] };
	VECTORCH from = { ray.from[0], ray.from[1], ray.from[2] };
	// The block leaveOut (the live Snap To Vertex's copy of the moving object) is taken out of the frame's list of what is on screen for this one search, and put back
	// in its place after it: inteng leaves the moving block out of its pick the same way (FUN_004f7000 0x4f7020, block +0x64 bit 0x10000000)
	int leftOutAt = -1;

	for (int i = 0; leaveOut && i < NumOnScreenBlocks && leftOutAt < 0; i++)
	{
		leftOutAt = OnScreenBlockList[i] == (DISPLAYBLOCK const *)leaveOut ? i : -1;
	}

	if (leftOutAt >= 0)
	{
		OnScreenBlockList[leftOutAt] = OnScreenBlockList[NumOnScreenBlocks - 1];
		OnScreenBlockList[NumOnScreenBlocks - 1] = (DISPLAYBLOCK *)leaveOut;
		NumOnScreenBlocks--;
	}

	FindDrawnPolygonInLineOfSight(&direction, &from, 1, Player);

	if (leftOutAt >= 0)
	{
		NumOnScreenBlocks++;
		OnScreenBlockList[NumOnScreenBlocks - 1] = OnScreenBlockList[leftOutAt];
		OnScreenBlockList[leftOutAt] = (DISPLAYBLOCK *)leaveOut;
	}

	DISPLAYBLOCK const *const hit = LOS_ObjectHitPtr;

	if (!hit)
	{
		report.found = host::PickFound::Nothing;
		return report;
	}

	report.distance = LOS_Lambda < 0 ? 0 : (LOS_Lambda > host::kSelectionMostDistance ? host::kSelectionMostDistance : LOS_Lambda);

	// Where, and on which face, the line met it (P1): copied before anything else can move them
	report.hasHit = true;
	report.hitPoint[0] = LOS_Point.vx;
	report.hitPoint[1] = LOS_Point.vy;
	report.hitPoint[2] = LOS_Point.vz;
	report.hitNormal[0] = LOS_ObjectNormal.vx;
	report.hitNormal[1] = LOS_ObjectNormal.vy;
	report.hitNormal[2] = LOS_ObjectNormal.vz;
	report.polygonNumber = LOS_PolygonNumber;
	report.hierarchySection = LOS_HModel_Section;   // the posed part of a hierarchical model whose polygons were met (Hierarchy's section click, hierarchy-closeout-frost.md TB-1)

	if (!LOS_HModel_Section)
	{
		report.hasCorner = NearestCorner(hit, LOS_PolygonNumber, LOS_Point, report.nearestCorner);
	}

	// An object the level file names: its strategy's ID, 8 bytes, which the game's own "no name" (NAME_ISNULL) has all 0
	STRATEGYBLOCK const *const strategy = hit->ObStrategyBlock;

	if (strategy)
	{
		static_assert(sizeof(strategy->SBname) == host::kSelectionIdBytes, "an object's ID is SB_NAME_LENGTH bytes");
		memcpy(report.objectId, strategy->SBname, host::kSelectionIdBytes);

		for (size_t i = 0; i < host::kSelectionIdBytes; i++)
		{
			if (report.objectId[i] != 0)
			{
				report.found = host::PickFound::Object;
				return report;
			}
		}

		memset(report.objectId, 0, sizeof(report.objectId));
	}

	// An object of a file with no modules (hierarchy_view.h, ModelObjects_Build): its block has no strategy and no module, and the file's own 8-byte ID is what names it
	{
		uint8_t modelId[host::kSelectionIdBytes];

		if (!strategy && !hit->ObMyModule && ModelObjects_IdOfBlock(hit, modelId))
		{
			memcpy(report.objectId, modelId, host::kSelectionIdBytes);
			report.found = host::PickFound::Object;
			return report;
		}
	}

	// A module: its number is checked against the game's own list before anything uses it
	MODULE const *const module = hit->ObMyModule;

	if (module)
	{
		if (module->m_index < 0 || module->m_index >= ModuleArraySize)
		{
			report.found = host::PickFound::NumberRefused;
			return report;
		}

		static_assert(sizeof(module->m_name) == host::kSelectionModuleNameBytes, "a module's own name is MODULE.m_name's bytes");
		report.found = host::PickFound::Module;
		report.moduleIndex = module->m_index;
		memcpy(report.moduleName, module->m_name, host::kSelectionModuleNameBytes);
		return report;
	}

	report.found = host::PickFound::NotInTheFile;
	return report;
}
