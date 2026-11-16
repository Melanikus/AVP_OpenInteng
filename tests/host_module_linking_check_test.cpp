// The end-to-end check of Module Linking, made by someone who built none of it (the non-builder's check for the Module Linking MVP). It runs on a REAL level (the
// first program argument): each operation is done on a freshly opened copy of the level through the editor's own host calls (LinkTree and SetModuleLinks for Normal,
// Branch, Delete and Exit; AddReverseLinks; AutoLink in both kinds with and without deleting the old links; AutoLink from a .mlf record; Delete Links), the level is
// SAVED AS A COPY and loaded again with the game's loader, and what the library then says is compared with what the operation promises. Every item that fails, or
// leaves the level different from what its description says, is a FAILED line. Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_module_linking_check_test.exe <a level .rif> <a folder it may write its copies into (made if it isn't there)>. With no arguments it says so and
// passes. The level is only read; the copies are the only files written.
//
// AFTER EVERY OPERATION: (a) the game's loader takes the copy; (b) the opened file is byte for byte what it was; (c) the number of objects is as it was; (d) every
// module the operation did not name keeps its links and its header flags (except the ones the operation says it touches); (e) the module flag 0x200 of every module
// the operation touched is set exactly when the module has links.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "module_links.h"
#include "module_data_edit.h"
#include "object_edit.h"
#include "object_base.h"
#include "reverse_links.h"
#include "autolink_edit.h"
#include "autolink_mlf.h"
#include "hidden_objects.h"

static int checksFailed = 0;
static bool gFlagsMayDisagree = false;   // no operation is allowed to leave a module with links and no flag any more (Drake 4f9ea67)

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// ---- Stand-ins for the parts of the game the chunk code calls: files are read by Windows' own

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

static bool ReadWholeFile(std::string const &path, std::vector<char> *out)
{
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER size;
	bool good = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 31);

	if (good)
	{
		out->assign((size_t)size.QuadPart, 0);
		DWORD got = 0;
		good = ReadFile(file, out->data(), (DWORD)out->size(), &got, 0) && got == out->size();
	}

	CloseHandle(file);
	return good;
}

static std::string CopyPathIn(std::string const &folder, char const *name)
{
	return folder + (folder.empty() || folder.back() == '\\' || folder.back() == '/' ? "" : "\\") + name;
}

static std::string HexOf(uint8_t const *id)
{
	char text[17];

	for (int at = 0; at < 8; at++)
	{
		_snprintf_s(text + 2 * at, 3, _TRUNCATE, "%02X", id[at]);
	}

	return std::string(text, 16);
}

// What the game's own chunk library says about a level: each module's links (in the order the level holds them) and header flags
struct Snapshot
{
	std::map<int32_t, std::vector<host::LinkItem> > links;   // modules that have a VMDARRAY
	std::map<int32_t, int> flags;                            // every module's header flags
	std::map<int32_t, std::string> idOf;
	size_t objects = 0;
};

static Snapshot Take(OpenLevelFile &open)
{
	Snapshot now;
	List<Object_Chunk *> all;
	std::set<int32_t> modules;

	open.Level()->list_objects(&all);
	host::ModuleIndexes(open.Level(), &modules);

	for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		now.objects++;

		if (modules.count(object->object_data.index_num) != 0)
		{
			Object_Header_Chunk *const header = object->get_header();

			now.flags[object->object_data.index_num] = header ? header->flags : -1;
			now.idOf[object->object_data.index_num] = HexOf((uint8_t const *)&object->object_data.ID);
		}
	}

	for (int32_t const index : modules)
	{
		std::vector<host::LinkItem> items;

		if (host::ReadModuleLinks(open.Level(), index, &items))
		{
			now.links[index] = items;
		}
	}

	return now;
}

static std::set<int32_t> TargetsOf(Snapshot const &s, int32_t module)
{
	std::set<int32_t> out;
	auto const found = s.links.find(module);

	if (found != s.links.end())
	{
		for (host::LinkItem const &item : found->second)
		{
			out.insert(item.objectIndex);
		}
	}

	return out;
}

static bool SameLinks(Snapshot const &a, Snapshot const &b, int32_t module)
{
	auto const x = a.links.find(module);
	auto const y = b.links.find(module);

	if (x == a.links.end() || y == b.links.end())
	{
		return x == a.links.end() && y == b.links.end();
	}

	return x->second.size() == y->second.size() && (x->second.empty() || memcmp(x->second.data(), y->second.data(), x->second.size() * sizeof(host::LinkItem)) == 0);
}

static void IdFromHex(std::string const &hex, uint8_t *id)
{
	for (int at = 0; at < 8; at++)
	{
		unsigned int value = 0;

		sscanf_s(hex.substr((size_t)at * 2, 2).c_str(), "%2x", &value);
		id[at] = (uint8_t)value;
	}
}

static double LevelScale(OpenLevelFile const &open)
{
	Chunk *const data = open.Level()->lookup_single_child("REBENVDT");
	Chunk_With_Children *const holder = data && !data->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(data) : nullptr;
	Chunk *const scale = holder ? holder->lookup_single_child("ENVSDSCL") : nullptr;
	std::vector<unsigned char> bytes;
	double value = 1.0;

	if (scale && open.PayloadOf(scale, &bytes) && bytes.size() >= 8)
	{
		memcpy(&value, bytes.data(), 8);
	}

	return value > 0.0 ? value : 1.0;
}

struct Context
{
	std::string levelPath, folder;
	std::vector<char> original;
	Snapshot before;
};

// One operation: open the level fresh, let `edit` change it, save a COPY, load the copy, and give the reloaded snapshot back. The common checks are made here.
// `touched` names the modules the operation may change; every other module must be as it was.
static bool Run(Context const &c, std::string const &name, std::set<int32_t> const &touched, std::function<bool(OpenLevelFile &)> const &edit, Snapshot *after)
{
	printf("%s\n", name.c_str());

	OpenLevelFile level;

	if (level.Open(c.levelPath.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, "the game's loader takes the level");
		return false;
	}

	bool const done = edit(level);

	Check(done, "the operation reports it is done");

	std::string const copyPath = CopyPathIn(c.folder, "module-linking-check-copy.rif");

	CreateDirectoryA(c.folder.c_str(), NULL);
	DeleteFileA(copyPath.c_str());

	if (level.SaveAs(copyPath.c_str()) != LevelSaveResult::Saved)
	{
		Check(false, "the level is saved as a COPY");
		return false;
	}

	std::vector<char> originalAfter;

	Check(ReadWholeFile(c.levelPath, &originalAfter) && originalAfter == c.original, "the opened file is byte for byte what it was");

	OpenLevelFile reloaded;

	if (reloaded.Open(copyPath.c_str()) != LevelOpenResult::Opened || !reloaded.Level())
	{
		Check(false, "the game's loader takes the copy");
		DeleteFileA(copyPath.c_str());
		return false;
	}

	Check(true, "the game's loader takes the copy");
	*after = Take(reloaded);
	DeleteFileA(copyPath.c_str());

	Check(after->objects == c.before.objects, "the number of objects is as it was: " + std::to_string(after->objects));

	size_t strayLinks = 0, strayFlags = 0;
	std::string firstStray;

	for (auto const &entry : c.before.flags)
	{
		if (touched.count(entry.first) != 0)
		{
			continue;
		}

		if (!SameLinks(c.before, *after, entry.first))
		{
			strayLinks++;
			firstStray = firstStray.empty() ? "module " + std::to_string(entry.first) : firstStray;
		}

		if (after->flags.count(entry.first) == 0 || after->flags.at(entry.first) != entry.second)
		{
			strayFlags++;
		}
	}

	Check(strayLinks == 0 && strayFlags == 0, "every module the operation does not touch keeps its links and its flags: " + std::to_string(c.before.flags.size() - touched.size()) + " compared, "
		+ std::to_string(strayLinks) + " links changed, " + std::to_string(strayFlags) + " flags changed" + (firstStray.empty() ? std::string() : " (first: " + firstStray + ")"));

	size_t flagWrong = 0;
	std::string firstFlag;

	for (int32_t const module : touched)
	{
		if (after->flags.count(module) == 0)
		{
			continue;
		}

		bool const hasLinks = after->links.count(module) != 0 && !after->links.at(module).empty();
		bool const flagged = (after->flags.at(module) & 0x200) != 0;

		if (hasLinks != flagged)
		{
			flagWrong++;
			firstFlag = firstFlag.empty() ? "module " + std::to_string(module) + (hasLinks ? " has links and no flag" : " has the flag and no links") : firstFlag;
		}
	}

	Check(flagWrong == 0 || gFlagsMayDisagree, "every touched module has the flag 0x200 exactly when it has links: " + std::to_string(touched.size()) + " compared, " + std::to_string(flagWrong) + " wrong"
		+ (firstFlag.empty() ? std::string() : " (first: " + firstFlag + ")"));
	return true;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc < 3)
	{
		printf("  host_module_linking_check_test <a level .rif> <a folder for its copies>: no level was named, so nothing was tested here\n");
		printf("All checks passed\n");
		return 0;
	}

	Context c;

	c.levelPath = argv[1];
	c.folder = argv[2];

	Check(ReadWholeFile(c.levelPath, &c.original) && !c.original.empty(), "the level is read (it is only read, never written)");

	OpenLevelFile pristine;

	if (pristine.Open(c.levelPath.c_str()) != LevelOpenResult::Opened || !pristine.Level())
	{
		Check(false, "the game's loader takes the level");
		return 1;
	}

	c.before = Take(pristine);

	double const scale = LevelScale(pristine);
	std::set<int32_t> modules;

	host::ModuleIndexes(pristine.Level(), &modules);

	// A: a module with links and the flag whose first target B is a module with links too; C: a module that is in no link of A and is not A
	int32_t a = -1, b = -1, c2 = -1;

	for (auto const &entry : c.before.links)
	{
		if (entry.second.size() >= 2 && (c.before.flags.at(entry.first) & 0x200) != 0)
		{
			a = entry.first;
			b = entry.second[0].objectIndex;
			break;
		}
	}

	if (a >= 0)
	{
		std::set<int32_t> const mine = TargetsOf(c.before, a);

		for (int32_t const m : modules)
		{
			if (m != a && mine.count(m) == 0 && c.before.idOf.count(m) != 0 && c.before.links.count(m) != 0)
			{
				c2 = m;
				break;
			}
		}
	}

	Check(a >= 0 && b >= 0 && c2 >= 0, "the level has a module A with links, one of its targets B, and a module C outside A's links: A = " + std::to_string(a) + ", B = " + std::to_string(b) + ", C = " + std::to_string(c2));

	if (a < 0 || b < 0 || c2 < 0)
	{
		return 1;
	}

	uint8_t aId[8];

	IdFromHex(c.before.idOf.at(a), aId);

	auto const loadTree = [&](OpenLevelFile &level, int32_t module, host::LinkTree *tree)
	{
		std::vector<host::LinkItem> items;
		std::set<int32_t> all;

		host::ModuleIndexes(level.Level(), &all);

		if (host::ReadModuleLinks(level.Level(), module, &items))
		{
			tree->Load(items.data(), (int)items.size(), [&all](int32_t i) { return all.count(i) != 0; });
		}
	};

	auto const writeTree = [&](OpenLevelFile &level, int32_t module, host::LinkTree &tree) -> bool
	{
		uint8_t id[8];

		if (!host::ModuleIdOfIndex(level.Level(), module, id))
		{
			return false;
		}

		std::vector<host::ModuleLinkItem> written;

		for (host::LinkItem const &item : tree.Flatten())
		{
			host::ModuleLinkItem const one = { item.objectIndex, item.branchNo, item.flags, item.spare };

			written.push_back(one);
		}

		// what WriteTheLinks does: links written and the flag put in, or no link and the flag taken out
		if (written.empty())
		{
			bool dropped = false;

			return host::DropModuleLinks(&level, module, &dropped) == host::ObjectEditResult::Done && host::SetModuleObjectFlag(&level, id, false) == host::BaseObjectResult::Done;
		}

		return host::SetModuleLinks(&level, id, written.data(), written.size()) == host::ObjectEditResult::Done && host::SetModuleObjectFlag(&level, id, true) == host::BaseObjectResult::Done;
	};

	Snapshot s;

	// 1. Normal Link: C is linked from A
	if (Run(c, "NORMAL LINK (A gets a link to C)", { a }, [&](OpenLevelFile &level)
	{
		host::LinkTree tree(a);

		loadTree(level, a, &tree);
		return tree.Normal(c2) && writeTree(level, a, tree);
	}, &s))
	{
		std::set<int32_t> const was = TargetsOf(c.before, a), now = TargetsOf(s, a);

		Check(s.links.at(a).size() == c.before.links.at(a).size() + 1 && now.count(c2) == 1, "A has one link more than before, and it goes to C: " + std::to_string(c.before.links.at(a).size()) + " to " + std::to_string(s.links.at(a).size()));
		Check(std::includes(now.begin(), now.end(), was.begin(), was.end()), "every link A had before is still there");
	}

	// 2. Branch Link then Delete Link
	if (Run(c, "BRANCH LINK, then DELETE LINK (A gets a branch to C, then loses the link to C again)", { a }, [&](OpenLevelFile &level)
	{
		host::LinkTree tree(a);

		loadTree(level, a, &tree);

		if (!tree.Branch(c2))
		{
			printf("  (Branch refused: the last link's branch is closed)\n");
			return tree.Normal(c2) && tree.Delete(c2) && writeTree(level, a, tree);
		}

		return tree.Has(c2) && tree.Delete(c2) && !tree.Has(c2) && writeTree(level, a, tree);
	}, &s))
	{
		Check(TargetsOf(s, a) == TargetsOf(c.before, a), "A's links lead to the same modules as before the branch and the delete");
	}

	// 3. Exit with an empty tree: the links are gone and the flag comes out
	if (Run(c, "EXIT WITH A TREE THAT HAS NO LINK (a changed empty tree is written)", { a }, [&](OpenLevelFile &level)
	{
		host::LinkTree tree(a);

		tree.MarkChanged();
		return writeTree(level, a, tree);
	}, &s))
	{
		Check(!(s.flags.at(a) & 0x200), "A's flag 0x200 is out");
		Check(s.links.count(a) == 0 || s.links.at(a).empty(), "the old links are gone too, not left behind with the flag out (the named deviation of 4f9ea67)");
	}

	gFlagsMayDisagree = false;

	// 4. Delete Links (Module)
	if (Run(c, "DELETE LINKS: MODULE (A)", { a }, [&](OpenLevelFile &level)
	{
		size_t removed = 0;

		return host::ModuleData_DeleteLinks(&level, aId, &removed) == host::ModuleDataResult::Done && removed >= 1 && host::SetModuleObjectFlag(&level, aId, false) == host::BaseObjectResult::Done;
	}, &s))
	{
		Check(s.links.count(a) == 0, "A has no VMDARRAY any more");
	}

	// 5. Add Reverse Links over A
	{
		std::set<int32_t> may = TargetsOf(c.before, a);

		may.insert(a);

		// the modules the walk can touch: every module in A's tree and the modules they link (a walk goes down the tree)
		std::set<int32_t> reach = may;

		for (int32_t const m : may)
		{
			for (int32_t const t : TargetsOf(c.before, m))
			{
				reach.insert(t);
			}
		}

		if (Run(c, "ADD REVERSE LINKS (picked module A)", reach, [&](OpenLevelFile &level)
		{
			std::vector<int32_t> sources = { a };
			host::ReverseLinksCounts counts = {};
			std::string why;

			bool const ok = host::AddReverseLinks(&level, &sources, &counts, &why) == host::ReverseLinksResult::Done;

			printf("  (walked %zu, touched %zu, changed %zu, links added %zu, written %zu)\n", counts.walked, counts.touched, counts.changed, counts.linksAdded, counts.written);
			return ok;
		}, &s))
		{
			size_t missing = 0;
			std::string first;

			for (int32_t const t : TargetsOf(s, a))
			{
				if (TargetsOf(s, t).count(a) == 0)
				{
					missing++;
					first = first.empty() ? "module " + std::to_string(t) : first;
				}
			}

			Check(missing == 0, "every module A links to has a link back to A afterwards: " + std::to_string(TargetsOf(s, a).size()) + " targets, " + std::to_string(missing) + " without the way back" + (first.empty() ? std::string() : " (first: " + first + ")"));
			{
				std::set<int32_t> const was = TargetsOf(c.before, a), now = TargetsOf(s, a);
				std::string lost;

				for (int32_t const w : was)
				{
					lost += now.count(w) == 0 ? " " + std::to_string(w) : std::string();
				}

				Check(lost.empty(), "A keeps every link it had: " + std::to_string(was.size()) + " before, " + std::to_string(now.size()) + " after" + (lost.empty() ? std::string() : "; lost the links to" + lost));
			}
		}
	}

	// 6 and 7. AutoLink, Module (add only), then with the old links deleted: only the picked module changes
	if (Run(c, "AUTOLINK: MODULE (A, the old links kept)", { a }, [&](OpenLevelFile &level)
	{
		std::vector<int32_t> sources = { a };
		host::AutoLinkCounts counts = {};
		std::string why;

		bool const ok = host::AutoLink_Run(&level, &sources, false, scale, 0, &counts, &why) == host::AutoLinkResult::Done;

		printf("  (sources %zu, links added %zu, written %zu, skipped %zu)\n", counts.sources, counts.linksAdded, counts.written, counts.skipped);
		return ok;
	}, &s))
	{
		std::set<int32_t> const was = TargetsOf(c.before, a), now = TargetsOf(s, a);

		Check(std::includes(now.begin(), now.end(), was.begin(), was.end()), "AutoLink with the old links kept leaves every old link of A in place: " + std::to_string(was.size()) + " before, " + std::to_string(now.size()) + " after");
	}

	std::set<int32_t> deleteOldTargets;

	if (Run(c, "AUTOLINK: MODULE (A, the old links deleted)", { a }, [&](OpenLevelFile &level)
	{
		std::vector<int32_t> sources = { a };
		host::AutoLinkCounts counts = {};
		std::string why;

		bool const ok = host::AutoLink_Run(&level, &sources, true, scale, 0, &counts, &why) == host::AutoLinkResult::Done;

		printf("  (sources %zu, links added %zu, written %zu, skipped %zu)\n", counts.sources, counts.linksAdded, counts.written, counts.skipped);
		return ok;
	}, &s))
	{
		deleteOldTargets = TargetsOf(s, a);

		bool allModules = true;

		for (int32_t const t : deleteOldTargets)
		{
			allModules = allModules && modules.count(t) != 0 && t != a;
		}

		Check(allModules, "every link A has afterwards goes to another module of the level: " + std::to_string(deleteOldTargets.size()) + " links");

		size_t kept = 0;

		for (int32_t const t : TargetsOf(c.before, a))
		{
			kept += deleteOldTargets.count(t);
		}

		printf("  (of A's %zu stored targets, AutoLink's own pass found %zu again)\n", TargetsOf(c.before, a).size(), kept);
	}

	// 8. AutoLink, Environment (every module, old links kept)
	if (Run(c, "AUTOLINK: ENVIRONMENT (every module, the old links kept)", modules, [&](OpenLevelFile &level)
	{
		host::AutoLinkCounts counts = {};
		std::string why;

		bool const ok = host::AutoLink_Run(&level, nullptr, false, scale, 0, &counts, &why) == host::AutoLinkResult::Done;

		printf("  (sources %zu, links added %zu, written %zu, skipped %zu)\n", counts.sources, counts.linksAdded, counts.written, counts.skipped);
		return ok;
	}, &s))
	{
		size_t lost = 0;

		for (int32_t const m : modules)
		{
			std::set<int32_t> const was = TargetsOf(c.before, m), now = TargetsOf(s, m);

			lost += std::includes(now.begin(), now.end(), was.begin(), was.end()) ? 0 : 1;
		}

		Check(lost == 0, "no module lost a link: " + std::to_string(modules.size()) + " modules, " + std::to_string(lost) + " lost one");
	}

	// 9. Load from file: one record naming module A at its own place
	{
		OpenLevelFile level;

		level.Open(c.levelPath.c_str());

		host::MlfRecord record = {};
		List<Object_Chunk *> all;

		level.Level()->list_objects(&all);

		for (LIF<Object_Chunk *> each(&all); !each.done(); each.next())
		{
			if (each() && each()->object_data.index_num == a)
			{
				record.name = each()->object_data.o_name;
				record.place[0] = each()->object_data.location.x;
				record.place[1] = each()->object_data.location.y;
				record.place[2] = each()->object_data.location.z;
			}
		}

		record.turn[0] = record.turn[4] = record.turn[8] = 0x10000;
		record.view[0] = record.place[0] / 100;
		record.view[1] = record.place[1] / 100;
		record.view[2] = record.place[2] / 100;

		std::vector<host::MlfRecord> records = { record };

		if (Run(c, "LOAD FROM FILE (.mlf: one record naming A)", { a }, [&](OpenLevelFile &opened)
		{
			host::MlfCounts counts = {};
			std::string why;

			bool const ok = host::AutoLink_FromMlf(&opened, records, scale, 0, &counts, &why) == host::AutoLinkResult::Done;

			printf("  (records %zu, used %zu, unknown %zu, links added %zu, written %zu)\n", counts.records, counts.used, counts.unknown, counts.linksAdded, counts.written);
			return ok;
		}, &s))
		{
			std::set<int32_t> const was = TargetsOf(c.before, a), now = TargetsOf(s, a);

			Check(std::includes(now.begin(), now.end(), was.begin(), was.end()), "Load from file keeps A's old links");
		}
	}

	// 10. The view items that must leave the data untouched: Hide, Un-hide last, Un-hide all (the editor's hidden set, hidden_objects.h; Show All, Mixed, Toggle View
	// Distance and Only Draw Linked are the same kind of view state, kept in the editor and not in the level). A level that went through them and is saved as a COPY is
	// byte for byte the copy of a level that did not.
	{
		printf("VIEW ITEMS (Hide, Un-hide last, Un-hide all leave the saved level byte for byte the same)\n");

		std::vector<char> plain, viewed;
		std::string const plainPath = CopyPathIn(c.folder, "module-linking-view-plain.rif");
		std::string const viewedPath = CopyPathIn(c.folder, "module-linking-view-viewed.rif");

		{
			OpenLevelFile level;

			level.Open(c.levelPath.c_str());
			DeleteFileA(plainPath.c_str());
			Check(level.SaveAs(plainPath.c_str()) == LevelSaveResult::Saved && ReadWholeFile(plainPath, &plain), "a level that went through no view item is saved as a COPY");
		}

		{
			OpenLevelFile level;

			level.Open(c.levelPath.c_str());

			uint8_t ids[16];

			IdFromHex(c.before.idOf.at(a), ids);
			IdFromHex(c.before.idOf.at(b), ids + 8);
			host::HiddenObjects_Reset();
			Check(host::HiddenObjects_Hide(ids, 2) == 2 && host::HiddenObjects_Is(ids) && host::HiddenObjects_Is(ids + 8), "Hide hides the two picked modules");
			Check(host::HiddenObjects_UnhideLast() && !host::HiddenObjects_Is(ids + 8) && host::HiddenObjects_Is(ids), "Un-hide last shows the last hidden and only that one");
			host::HiddenObjects_ShowAll(true);
			Check(host::HiddenObjects_Count() == 0, "Un-hide all shows every object");
			host::HiddenObjects_Hide(ids, 1);
			DeleteFileA(viewedPath.c_str());
			Check(level.SaveAs(viewedPath.c_str()) == LevelSaveResult::Saved && ReadWholeFile(viewedPath, &viewed), "a level with a module still hidden is saved as a COPY");
			host::HiddenObjects_Reset();
		}

		Check(!plain.empty() && plain == viewed, "the two copies are byte for byte the same: " + std::to_string(plain.size()) + " bytes");
		DeleteFileA(plainPath.c_str());
		DeleteFileA(viewedPath.c_str());
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
