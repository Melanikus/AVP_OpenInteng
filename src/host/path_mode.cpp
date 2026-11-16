// See path_mode.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "path_mode.h"

#include <algorithm>

#include <string.h>

#include <vector>

#include "level_file.h"
#include "level_image.h"

namespace host
{
	namespace
	{
		using namespace image;

		int32_t const kMostPathNumber = 255;

		// The number an AVPPATH2 chunk holds: after its name (the text up to its 0, padded to 4 with the end: (length + 4) & ~3). False when the chunk is too short.
		bool NumberOf(Bytes const &bytes, Span const &chunk, int32_t *number)
		{
			size_t const payload = chunk.at + kHeadingBytes;
			size_t const length = chunk.size - kHeadingBytes;
			char const *const zero = (char const *)memchr(bytes.data() + payload, 0, length);

			if (!zero)
			{
				return false;
			}

			size_t const nameBytes = (size_t)(zero - (bytes.data() + payload));
			size_t const at = (nameBytes + 4) & ~(size_t)3;

			if (at + sizeof(int32_t) > length)
			{
				return false;
			}

			*number = IntAt(bytes, payload + at);
			return true;
		}

		// REBENVDT (the first top chunk of that name) and SPECLOBJ inside it (the first), as indexes. envAt == top.size() when there is no REBENVDT; specAt == inside.size() when no SPECLOBJ.
		bool Find(Bytes const &image, std::vector<Span> const &top, size_t *envAt, std::vector<Span> *inside, size_t *specAt)
		{
			*envAt = top.size();

			for (size_t i = 0; i < top.size() && *envAt == top.size(); i++)
			{
				if (NameIs(image, top[i].at, "REBENVDT"))
				{
					*envAt = i;
				}
			}

			*specAt = 0;
			inside->clear();

			if (*envAt == top.size())
			{
				return true;
			}

			if (!Children(image, top[*envAt].at + kHeadingBytes, top[*envAt].at + top[*envAt].size, inside))
			{
				return false;
			}

			*specAt = FirstNamed(image, *inside, "SPECLOBJ");
			return true;
		}
	}

	char const *PathModeWords(PathModeResult result)
	{
		switch (result)
		{
		case PathModeResult::Done: return "done";
		case PathModeResult::BadCall: return "nothing to change";
		case PathModeResult::BadNumber: return "a path's number is 0 to 255";
		case PathModeResult::NoEnvironment: return "the level has no environment data";
		case PathModeResult::NothingToDo: return "there is no such path to delete";
		case PathModeResult::TooMany: return "too many nodes";
		case PathModeResult::CantBuild: return "the level couldn't be put together from its bytes";
		case PathModeResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	bool PathMode_NextNumber(OpenLevelFile *file, int32_t *number)
	{
		*number = 0;

		if (!file || !file->Level())
		{
			return false;
		}

		Bytes image;
		std::vector<Span> top, inside, paths;
		size_t envAt = 0, specAt = 0;

		if (!Walk(file, &image, &top) || !Find(image, top, &envAt, &inside, &specAt))
		{
			return false;
		}

		if (envAt == top.size() || specAt == inside.size())
		{
			return true;   // no path: the first number is 0
		}

		if (!Children(image, inside[specAt].at + kHeadingBytes, inside[specAt].at + inside[specAt].size, &paths))
		{
			return false;
		}

		std::vector<int32_t> numbers;

		for (Span const &chunk : paths)
		{
			int32_t held = 0;

			if (NameIs(image, chunk.at, "AVPPATH2") && NumberOf(image, chunk, &held))
			{
				numbers.push_back(held);
			}
		}

		// The smallest number NO path uses, whatever the order of the chunks. (inteng's own loop, FUN_004fb300 0x4fb62a-0x4fb666, passes a chunk whose number is not n and never looks at it
		// again, so with the chunks out of order it returns a number a path already has -- stock Breakout gives 2 -- and Create Path then REPLACES that path. A named deviation, Melanikus'
		// ruling 2026-11-15: AvP-docs/engine/inteng/unfinished-in-inteng.md "Path mode".)
		int32_t n = 0;

		while (std::find(numbers.begin(), numbers.end(), n) != numbers.end())
		{
			n++;
		}

		*number = n;
		return true;
	}

	PathModeResult PathMode_Write(OpenLevelFile *file, int32_t number, char const *name, int32_t flags, int32_t spare, PathNode const *nodes, size_t count)
	{
		if (!file || !file->Level() || (count != 0 && !nodes))
		{
			return PathModeResult::BadCall;
		}

		if (number < 0 || number > kMostPathNumber)
		{
			return PathModeResult::BadNumber;
		}

		if (count > kPathModeMostNodes)
		{
			return PathModeResult::TooMany;
		}

		Bytes image;
		std::vector<Span> top, inside, specInside;
		size_t envAt = 0, specAt = 0;

		if (!Walk(file, &image, &top) || !Find(image, top, &envAt, &inside, &specAt))
		{
			return PathModeResult::CantBuild;
		}

		if (envAt == top.size())
		{
			return PathModeResult::NoEnvironment;
		}

		// SPECLOBJ's children, and which AVPPATH2 goes (the first with that number)
		size_t goes = 0;
		bool haveSpec = specAt != inside.size();

		if (haveSpec)
		{
			if (!Children(image, inside[specAt].at + kHeadingBytes, inside[specAt].at + inside[specAt].size, &specInside))
			{
				return PathModeResult::CantBuild;
			}
		}

		goes = specInside.size();

		for (size_t i = 0; i < specInside.size() && goes == specInside.size(); i++)
		{
			int32_t held = 0;

			if (NameIs(image, specInside[i].at, "AVPPATH2") && NumberOf(image, specInside[i], &held) && held == number)
			{
				goes = i;
			}
		}

		if (count == 0 && goes == specInside.size() && haveSpec)
		{
			return PathModeResult::NothingToDo;
		}

		// The new chunk
		Bytes made;

		if (count != 0)
		{
			std::string const text = name ? name : "";
			Bytes payload;

			payload.insert(payload.end(), text.begin(), text.end());
			payload.resize((text.size() + 4) & ~(size_t)3, 0);   // (length + 4) & ~3 bytes: the text, its end, and the padding (path_chunk.cpp:71-72)

			AddInt(&payload, number);
			AddInt(&payload, flags);
			AddInt(&payload, spare);
			AddInt(&payload, (int32_t)count);

			for (size_t i = 0; i < count; i++)
			{
				AddInt(&payload, nodes[i].module);
				AddInt(&payload, nodes[i].place[0]);
				AddInt(&payload, nodes[i].place[1]);
				AddInt(&payload, nodes[i].place[2]);
				AddInt(&payload, nodes[i].flags);
				AddInt(&payload, nodes[i].spare);
			}

			made = MakeChunk("AVPPATH2", payload);
		}

		// The new SPECLOBJ: its children but the one that goes, then the new chunk; a SPECLOBJ that was missing is made (empty, or with the new chunk)
		Bytes spec(kHeadingBytes, 0);

		memcpy(spec.data(), "SPECLOBJ", 8);

		for (size_t i = 0; i < specInside.size(); i++)
		{
			if (i != goes)
			{
				AddCopy(&spec, image, specInside[i]);
			}
		}

		spec.insert(spec.end(), made.begin(), made.end());
		PutIntAt(&spec, 8, (int32_t)spec.size());

		// The new REBENVDT: its children, SPECLOBJ in its place (or last when it was missing)
		Span const env = top[envAt];
		Bytes newEnv;

		newEnv.insert(newEnv.end(), image.begin() + (std::ptrdiff_t)env.at, image.begin() + (std::ptrdiff_t)(env.at + kHeadingBytes));

		for (size_t i = 0; i < inside.size(); i++)
		{
			if (haveSpec && i == specAt)
			{
				newEnv.insert(newEnv.end(), spec.begin(), spec.end());
			}
			else
			{
				AddCopy(&newEnv, image, inside[i]);
			}
		}

		if (!haveSpec)
		{
			newEnv.insert(newEnv.end(), spec.begin(), spec.end());
		}

		if (newEnv.size() > kLargest)
		{
			return PathModeResult::CantBuild;
		}

		PutIntAt(&newEnv, 8, (int32_t)newEnv.size());

		Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)env.at);

		out.insert(out.end(), newEnv.begin(), newEnv.end());
		out.insert(out.end(), image.begin() + (std::ptrdiff_t)(env.at + env.size), image.end());

		if (out.size() > kLargest)
		{
			return PathModeResult::CantBuild;
		}

		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return PathModeResult::Done;
		}

		return result == LevelOpenResult::CantRead ? PathModeResult::CantBuild : PathModeResult::LoaderRefused;
	}

	int PathMode_NearestStep(PathGraph const &graph, std::vector<int32_t> const &numbers, int start, std::vector<PathNode> const &steps)
	{
		int const count = (int)graph.neighbours.size();

		if (start < 0 || start >= count || numbers.size() != graph.neighbours.size() || steps.empty())
		{
			return -1;
		}

		std::vector<int> distance((size_t)count, -1), queue(1, start);

		distance[(size_t)start] = 0;

		for (size_t at = 0; at < queue.size(); at++)
		{
			for (int const next : graph.neighbours[(size_t)queue[at]])
			{
				if (next >= 0 && next < count && distance[(size_t)next] == -1)
				{
					distance[(size_t)next] = distance[(size_t)queue[at]] + 1;
					queue.push_back(next);
				}
			}
		}

		int best = 1000000;   // 0xf4240 (0x4f96bf)
		int chosen = -1;

		for (int i = 0; i < count; i++)
		{
			if (distance[(size_t)i] == -1 || distance[(size_t)i] >= best)
			{
				continue;
			}

			bool first = true;

			for (int k = 0; k < i && first; k++)
			{
				first = numbers[(size_t)k] != numbers[(size_t)i];
			}

			for (PathNode const &step : steps)
			{
				if (first && step.module == numbers[(size_t)i])
				{
					best = distance[(size_t)i];
					chosen = i;
				}
			}
		}

		return chosen;
	}

	bool PathMode_Route(PathGraph const &graph, int from, int to, std::vector<int> *between)
	{
		between->clear();

		int const count = (int)graph.neighbours.size();

		if (from < 0 || to < 0 || from >= count || to >= count)
		{
			return false;
		}

		std::vector<int> depth((size_t)count, -1), parent((size_t)count, -1), queue;

		depth[(size_t)from] = 0;
		queue.push_back(from);

		for (size_t at = 0; at < queue.size(); at++)
		{
			int const here = queue[at];

			for (int const next : graph.neighbours[(size_t)here])
			{
				if (next >= 0 && next < count && depth[(size_t)next] == -1)
				{
					depth[(size_t)next] = depth[(size_t)here] + 1;
					parent[(size_t)next] = here;
					queue.push_back(next);
				}
			}
		}

		// back from the target while the depth is above 1, pushing each parent in front
		int at = to;

		while (depth[(size_t)at] > 1)
		{
			at = parent[(size_t)at];
			between->insert(between->begin(), at);
		}

		return true;
	}

	bool PathMode_AddModule(std::vector<PathNode> *path, PathGraph const &graph, std::vector<int32_t> const &numbers, int lastIndex, int pickedIndex)
	{
		if (pickedIndex < 0 || (size_t)pickedIndex >= numbers.size() || path->size() >= kPathModeMostNodes)
		{
			return false;
		}

		std::vector<int> between;

		if (!path->empty())
		{
			if (lastIndex < 0 || (size_t)lastIndex >= numbers.size() || !PathMode_Route(graph, lastIndex, pickedIndex, &between) || path->size() + between.size() + 1 > kPathModeMostNodes)
			{
				return false;
			}
		}

		for (int const index : between)
		{
			PathNode node = { numbers[(size_t)index], { 0, 0, 0 }, 1, 0 };

			path->push_back(node);
		}

		PathNode picked = { numbers[(size_t)pickedIndex], { 0, 0, 0 }, 0, 0 };

		path->push_back(picked);
		return true;
	}

	void PathMode_DeleteLast(std::vector<PathNode> *path)
	{
		if (path->empty())
		{
			return;
		}

		path->pop_back();

		while (!path->empty() && (path->back().flags & 1) != 0)
		{
			path->pop_back();
		}
	}

	int32_t PathMode_FlagsAfterProperties(int32_t flags, bool loopChosen)
	{
		return loopChosen ? (flags & ~2) : (flags | 2);
	}

	bool PathMode_BuildGraph(OpenLevelFile *file, PathGraph *graph, std::vector<int32_t> *numbers, std::vector<std::string> *names)
	{
		graph->neighbours.clear();
		numbers->clear();
		names->clear();

		if (!file || !file->Level())
		{
			return false;
		}

		Bytes image;
		std::vector<Span> top;

		if (!Walk(file, &image, &top))
		{
			return false;
		}

		struct Module
		{
			std::vector<int32_t> adjacent;
		};

		std::vector<Module> modules;

		for (Span const &chunk : top)
		{
			if (!NameIs(image, chunk.at, "RBOBJECT"))
			{
				continue;
			}

			std::vector<Span> inside;

			if (!Children(image, chunk.at + kHeadingBytes, chunk.at + chunk.size, &inside))
			{
				return false;
			}

			size_t const head = FirstNamed(image, inside, "OBJHEAD1");
			size_t const module = FirstNamed(image, inside, "MODULEDT");

			if (head == inside.size() || module == inside.size() || inside[head].size < kHeadingBytes + kNameAt + 1)
			{
				continue;
			}

			uint32_t const flags = (uint32_t)IntAt(image, inside[head].at + kHeadingBytes);

			if ((flags & 0x200) == 0)
			{
				continue;
			}

			std::string name;

			if (!NameOfObjectChunk(image, chunk, &name))
			{
				continue;
			}

			Module made;
			std::vector<Span> parts;

			if (!Children(image, inside[module].at + kHeadingBytes, inside[module].at + inside[module].size, &parts))
			{
				return false;
			}

			size_t const adj = FirstNamed(image, parts, "ADJMDLEP");

			if (adj != parts.size() && parts[adj].size >= kHeadingBytes + 4)
			{
				size_t const payload = parts[adj].at + kHeadingBytes;
				size_t const room = parts[adj].size - kHeadingBytes - 4;
				int32_t const listed = IntAt(image, payload);
				size_t const items = listed > 0 ? (size_t)listed : 0;

				// a count that the bytes can't hold is cut to what they hold
				for (size_t i = 0; i < items && (i + 1) * 20 <= room; i++)
				{
					made.adjacent.push_back(IntAt(image, payload + 4 + i * 20));
				}
			}

			numbers->push_back(IntAt(image, inside[head].at + kHeadingBytes + 48));
			names->push_back(name);
			modules.push_back(made);
		}

		graph->neighbours.assign(modules.size(), std::vector<int>());

		for (size_t i = 0; i < modules.size(); i++)
		{
			for (int32_t const value : modules[i].adjacent)
			{
				for (size_t j = 0; j < modules.size(); j++)
				{
					if ((*numbers)[j] == value)
					{
						graph->neighbours[i].push_back((int)j);
						break;
					}
				}
			}
		}

		return true;
	}

	bool PathMode_ReadPaths(OpenLevelFile *file, std::vector<PathInfo> *paths)
	{
		paths->clear();

		if (!file || !file->Level())
		{
			return false;
		}

		Bytes image;
		std::vector<Span> top;
		size_t envAt = 0;
		std::vector<Span> inside;
		size_t specAt = 0;

		if (!Walk(file, &image, &top) || !Find(image, top, &envAt, &inside, &specAt))
		{
			return false;
		}

		if (specAt == inside.size())
		{
			return true;   // no SPECLOBJ: no path
		}

		std::vector<Span> held;

		if (!Children(image, inside[specAt].at + kHeadingBytes, inside[specAt].at + inside[specAt].size, &held))
		{
			return false;
		}

		for (Span const &chunk : held)
		{
			if (!NameIs(image, chunk.at, "AVPPATH2") || paths->size() >= kPathModeMostPaths)
			{
				continue;
			}

			size_t const payload = chunk.at + kHeadingBytes;
			size_t const length = chunk.size - kHeadingBytes;
			char const *const zero = (char const *)memchr(image.data() + payload, 0, length);

			if (!zero)
			{
				continue;
			}

			size_t const nameBytes = (size_t)(zero - (image.data() + payload));
			size_t const at = (nameBytes + 4) & ~(size_t)3;

			if (at + 16 > length)
			{
				continue;
			}

			PathInfo made;

			made.name.assign((char const *)image.data() + payload, nameBytes);
			made.number = IntAt(image, payload + at);
			made.flags = IntAt(image, payload + at + 4);
			made.spare = IntAt(image, payload + at + 8);

			int32_t const listed = IntAt(image, payload + at + 12);
			size_t const room = (length - at - 16) / 24;
			size_t const count = listed > 0 ? ((size_t)listed < room ? (size_t)listed : room) : 0;

			for (size_t n = 0; n < count && n < kPathModeMostNodes; n++)
			{
				size_t const base = payload + at + 16 + n * 24;
				PathNode node;

				node.module = IntAt(image, base);
				node.place[0] = IntAt(image, base + 4);
				node.place[1] = IntAt(image, base + 8);
				node.place[2] = IntAt(image, base + 12);
				node.flags = IntAt(image, base + 16);
				node.spare = IntAt(image, base + 20);
				made.nodes.push_back(node);
			}

			paths->push_back(made);
		}

		return true;
	}
}
