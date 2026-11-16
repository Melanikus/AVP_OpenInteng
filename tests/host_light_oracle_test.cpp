// The ORACLE test for inteng's lighting calculation (Light Sourcing; src/host/light_maths.*, light_scene.*): the level's lights are run through the calculation inteng's Save Intensities uses, and what it gives for
// every vertex of every module is compared with the SHPVTINT chunks the stock level holds (they were made by exactly this calculation). Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_light_oracle_test.exe <a level .rif> [a mismatch the test may print for each of N objects]. With no arguments it says so and passes. The level is only read.
// WHAT IT SAYS (a MATCH %, not a gate to the digit): the vertices whose three colour parts are exactly the stored ones; the vertices within 1 / within 4 of a part; the vertices where the stored value is black and
// ours is not and the other way round; the objects that match completely.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "light_scene.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static int Part(uint32_t value, int shift)
{
	return (int)((value >> shift) & 0xff);
}

static int Worst(uint32_t a, uint32_t b)
{
	int worst = 0;

	for (int shift = 0; shift <= 16; shift += 8)
	{
		int const d = abs(Part(a, shift) - Part(b, shift));
		worst = d > worst ? d : worst;
	}

	return worst;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("host_light_oracle_test: give a level .rif; with none there is nothing to run.\n");
		printf("passed host_light_oracle_test (0 ok)\n");
		return 0;
	}

	OpenLevelFile level;

	if (level.Open(argv[1]) != LevelOpenResult::Opened || !level.Level())
	{
		printf("  FAILED  the game's loader takes the level\n");
		return 1;
	}

	host::LightSceneLights lights;
	std::string why;

	Check(host::LightScene_ReadLights(&level, &lights, &why), "the level's lights are read: " + why);
	printf("  info  %zu lights, ambience %d, scale %g\n", lights.lights.size(), lights.ambience, lights.scale);

	std::vector<host::LitObject> lit;
	bool const doorsClosed = argc <= 4 || atoi(argv[4]) != 0;   // the stock levels were saved with Doors > Close done: the default
	bool const flatNormals = argc > 5 && atoi(argv[5]) != 0;   // the vertex normals made again from the polygons, as after Hierarchy / Sequence (FUN_004b1720)
	host::LightScene_LightModules(&level, lights, false, doorsClosed, &lit, flatNormals);

	size_t objects = 0, objectsExact = 0, vertices = 0, exact = 0, within1 = 0, within4 = 0, within16 = 0, blackStored = 0, blackOurs = 0, sizeMismatch = 0;
	double sumDiff = 0;

	int const showMost = argc > 2 ? atoi(argv[2]) : 0;
	int shown = 0;
	int const listMost = argc > 3 ? atoi(argv[3]) : 0;
	size_t listed = 0;

	size_t modulesWithout = 0, withoutButLit = 0, reachWrong = 0;

	for (host::LitObject const &one : lit)
	{
		if (!one.hasStored)
		{
			modulesWithout++;
			reachWrong += one.reached;

			for (uint32_t value : one.computed)
			{
				if (value != 0)
				{
					withoutButLit++;
					break;
				}
			}

			continue;
		}

		objects++;
		reachWrong += !one.reached;

		if (one.stored.size() != one.computed.size())
		{
			sizeMismatch++;
		}

		size_t const count = one.stored.size() < one.computed.size() ? one.stored.size() : one.computed.size();
		bool all = count > 0;

		for (size_t v = 0; v < count; v++)
		{
			vertices++;
			int const worst = Worst(one.computed[v], one.stored[v]);
			sumDiff += worst;

			if (worst == 0)
			{
				exact++;
			}
			else
			{
				all = false;

				if (shown < showMost)
				{
					shown++;
					printf("  DIFF object %d '%s' vertex %zu: ours %06x stored %06x, vertex %d %d %d normal %d %d %d\n", one.number, one.name.c_str(), v, one.computed[v], one.stored[v], one.vertices[3 * v], one.vertices[3 * v + 1], one.vertices[3 * v + 2],
						one.normals[3 * v], one.normals[3 * v + 1], one.normals[3 * v + 2]);

					for (size_t l = 0; l < lights.lights.size(); l++)
					{
						host::light::Light const &ln = lights.lights[l];
						int32_t const vx[3] = { one.vertices[3 * v], one.vertices[3 * v + 1], one.vertices[3 * v + 2] };
						int32_t const nx[3] = { one.normals[3 * v], one.normals[3 * v + 1], one.normals[3 * v + 2] };
						int32_t const intensity = host::light::Intensity(vx, nx, ln);

						if (intensity != 0 || host::light::Distance(vx, ln.place) <= ln.range * 2)
						{
							printf("      light %zu flags %x noPre %d colour %06x brightness %d spread %d range %d place %d %d %d dir %d %d %d distance %d intensity %d\n", l, ln.flags, ln.noPreLight, ln.colour, ln.brightness, ln.spread, ln.range,
								ln.place[0], ln.place[1], ln.place[2], ln.direction[0], ln.direction[1], ln.direction[2], host::light::Distance(vx, ln.place), intensity);
						}
					}
				}
			}

			within1 += worst <= 1;
			within4 += worst <= 4;
			within16 += worst <= 16;
			blackStored += (one.stored[v] & 0xffffff) == 0 && (one.computed[v] & 0xffffff) != 0;
			blackOurs += (one.computed[v] & 0xffffff) == 0 && (one.stored[v] & 0xffffff) != 0;
		}

		objectsExact += all;

		if (!all && listMost > 0)
		{
			size_t bad = 0;

			for (size_t v = 0; v < count; v++)
			{
				bad += Worst(one.computed[v], one.stored[v]) != 0;
			}

			if (listed++ < (size_t)listMost)
			{
				printf("  INEXACT object %d '%s': %zu of %zu vertices differ\n", one.number, one.name.c_str(), bad, count);
			}
		}
	}

	printf("  info  %zu modules have no stored SHPVTINT, %zu of them would be lit by the calculation\n", modulesWithout, withoutButLit);
	printf("  info  the reach rule (a light on and nearer than the shape's radius + its range) agrees with the stored chunks for all but %zu modules\n", reachWrong);
	Check(objects > 0, "the level has modules with a stored SHPVTINT");
	printf("  info  %zu objects with stored values (%zu with another number of vertices than the shape's points), %zu vertices\n", objects, sizeMismatch, vertices);

	if (vertices)
	{
		printf("  MATCH  exact %.2f%%  within 1 of a part %.2f%%  within 4 %.2f%%  within 16 %.2f%%  mean worst-part difference %.3f  black in the file but lit by us %zu  lit in the file but black for us %zu  objects exact %zu of %zu\n",
			100.0 * exact / vertices, 100.0 * within1 / vertices, 100.0 * within4 / vertices, 100.0 * within16 / vertices, sumDiff / vertices, blackStored, blackOurs, objectsExact, objects);
	}

	if (checksFailed)
	{
		printf("FAILED host_light_oracle_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_light_oracle_test\n");
	return 0;
}
