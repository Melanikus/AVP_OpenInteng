// verify_all_stock_maps.cpp: the check program of acceptance-all-stock-maps*.txt, the owner's "All stock maps" (Waypoint mode > Alien auto waypoint, AvP-docs director/
// plan-all-stock-maps.md): it sets up the run's PRIVATE data copy, and afterwards checks what is on disk there. Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE LEVELS (avpx 0de2af86 3dc/avp/win95/frontend/env_info.cpp, MarineEpisodes, PredatorEpisodes, AlienEpisodes and RifNamesForEnvironments): the run changes the 22 levels of the
// Marine and Predator campaigns, and never the 10 of the Alien campaign. The list is written here from the game's table, NOT taken from the editor's, so a wrong list in the editor
// fails. A level the data folder doesn't hold is the run's "missing" (Skipped): so a run on 3 of the 22 is a data folder that holds only those 3.
//
// WHERE IT WRITES: only prepare and restore write, and only into a data folder laid out as run-shards.sh's private shard copy (...\s<k>\lv\<folder>, mirrored afresh from the
// original at every run), never an original or a game folder; and its own work folder, <data folder>\..\all-stock-maps (the same shard's, outside the data folder), which holds
// the snapshot and each level's bytes from before the run, so a second sitting (R7, a scenario of its own) still finds them.
//
// USAGE: verify_all_stock_maps <check> <the data folder> <the stock levels folder> [<option> ...]
//   prepare [keep=<a>,<b>,...] [replace=<a>:<b>] [bak=<a>:<b>]
//              RESTORE first: the 32 campaign levels are put back to the stock folder's bytes and every one of their .rif.bak files is deleted. Then the fixture: with keep, only
//              the campaign levels named stay (the other Marine and Predator levels are deleted from the copy, so the run finds them missing); replace=<a>:<b> writes stock <b>'s
//              bytes as level <a> (a level that isn't Steam's own <a>, as an edited or a stranger's one is: R6, R8); bak=<a>:<b> writes stock <b>'s bytes as <a>.rif.bak (a .bak
//              that's already there, which the run must keep byte for byte). Then every file under avp_rifs is written down (name, size, SHA-256), and each campaign level's
//              bytes are kept, in the work folder.
//   after      each of the 22 the copy held: its .rif.bak holds its bytes from before the run (or, if one was there, is unchanged), and its .rif changed: the game's loader takes
//              it, every alien network it had is kept as it was (Rebellion's own: overwrite is off), and at least one AI module got a network it didn't have. Each of the 22 it
//              didn't hold is still not there, with no .rif.bak. Each of the 10 Alien levels is as it was, with no .rif.bak. Nothing else under avp_rifs changed, came or went (the
//              editor's own avp_rifs\OpenedByEditor apart).
//   again      after a second run (another sitting): as after, against the same snapshot (every .rif.bak is STILL the first one: a .bak that's there is never replaced)
//   cancelled  after a Cancel part way: each of the 22 is either done (as after) or untouched (as it was, and its .rif.bak as it was or none), at least one of each (or the Cancel
//              didn't land part way and the check proves nothing); the rest as after
//   untouched  nothing under avp_rifs changed, came or went (R12: AutoPlot=0)
//   restore    as prepare's first half: the 32 back to stock and their .rif.bak files deleted (the end of a scenario, so the shard's copy is stock again)
//   same <a> <b>   the two files are the same bytes (R1: a batch-saved level against a fresh single-level run's saved copy)
// after, again and cancelled also take log <the editor's log>: the log holds the run's report (Drake's lines): "All stock maps: the run ended: ...", each of the 22 the
// copy held as "All stock maps: CHANGED <file>" (cancelled: or NOT STARTED), and each it didn't hold as "All stock maps: SKIPPED <file>".
// It passes (exit 0) when every check does; the lines say what was found.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "level_file.h"
#include "sha256_stream.h"
#include "waypoint_net_level.h"

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	// read only: nothing this program opens through the game's code is ever written
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

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

using namespace host;
namespace fs = std::filesystem;

static int failed = 0;

static void Check(bool ok, std::string const &what)
{
	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
	failed += ok ? 0 : 1;
}

// the Marine and Predator campaigns' levels (changed by the run), and the Alien campaign's (never), by RifNamesForEnvironments' names
static std::vector<std::string> const kChanged = { "derelict", "genshd1", "invasion", "odobenus", "sulaco", "hangar", "temple_m", "vaults_m", "nost03_m", "stat101_m", "fall_m",
	"fall", "area52", "vaults", "furyall", "caverns", "battle", "invasion_p", "escape_p", "temple_p", "breakout_p", "sulaco_p" };
static std::vector<std::string> const kUntouched = { "temple", "escape", "nost03", "stat101", "breakout", "invasion_a", "derelict_a", "sulaco_a", "caverns_a", "furyall_a" };

static uint64_t const kMostBytes = 64ull * 1024 * 1024;   // the largest stock level is about 3 MB

static std::string Lower(std::string s)
{
	for (char &c : s)
	{
		c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}

	return s;
}

// A level's name from the command line, as the TABLE above spells it, or "" if the name isn't one of the 32. The name is only used to find the
// level's PLACE in the two tables (the Marine and Predator levels first, then the Alien ones); what comes back is the table's own text at that place,
// so a path is never made from the command line's text. (Added 2026-11-23 by Melanikus (Frost): Snyk Code followed the options' names into the paths
// made from them, as it did in verify_level_same, whose IgnoredChunkName does the same.)
static std::string TableName(std::string const &name)
{
	size_t place = 0;
	bool found = false;

	for (bool changed : { true, false })
	{
		for (std::string const &known : changed ? kChanged : kUntouched)
		{
			if (known == name)
			{
				found = true;
				break;
			}

			place++;
		}

		if (found)
		{
			break;
		}
	}

	if (!found)
	{
		return std::string();
	}

	return place < kChanged.size() ? kChanged[place] : kUntouched[place - kChanged.size()];
}

// A path from the command line, made whole (GetFullPathNameA) and checked before anything reads, writes or deletes under it: it must exist, be a
// folder or a plain file as asked, and not be a link or a junction (either can lead anywhere). One that climbs out of a folder ("..") or is longer
// than a path can be is refused. What is used is the whole path, not the argument. (Added 2026-11-23 by Melanikus (Frost): Snyk Code, a command
// line's path used unchecked; the same check as verify_level_same's LevelSameWholePath.)
static bool WholePath(char const *given, bool folder, fs::path *out)
{
	if (!given || !given[0] || strlen(given) >= MAX_PATH || strstr(given, "..") != nullptr)
	{
		return false;
	}

	char full[MAX_PATH] = "";
	DWORD const length = GetFullPathNameA(given, MAX_PATH, full, nullptr);

	if (length == 0 || length >= MAX_PATH)
	{
		return false;
	}

	DWORD const kind = GetFileAttributesA(full);

	if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || ((kind & FILE_ATTRIBUTE_DIRECTORY) != 0) != folder)
	{
		return false;
	}

	// and every folder on the way to it: Windows follows a junction or a link anywhere in a path, so one higher up could lead anywhere too (Dietrich's
	// review, 11-23)
	for (fs::path up = fs::path(std::string(full)).parent_path(); !up.empty() && up != up.root_path(); up = up.parent_path())
	{
		DWORD const above = GetFileAttributesA(up.string().c_str());

		if (above == INVALID_FILE_ATTRIBUTES || (above & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			return false;
		}
	}

	*out = fs::path(std::string(full));
	return true;
}

// a file's bytes, whole, or false (none, not a plain file, or over the cap)
static bool ReadAll(fs::path const &file, std::vector<char> *bytes)
{
	bytes->clear();

	if (file.empty())
	{
		return false;
	}

	// opened without following a link, and judged and read on THAT handle, so nothing can be swapped in between the look and the read (Dietrich's review,
	// 11-23: it used to look by name, then open by name). Shared for writing and deleting too, as the stream it replaces was: the editor's log is still
	// open for writing in the running editor when the checks read it (measured: with reading shared only, the log read as missing, 3 files RED)
	HANDLE const handle = CreateFileW(file.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

	if (handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	BY_HANDLE_FILE_INFORMATION info;
	bool good = GetFileType(handle) == FILE_TYPE_DISK && GetFileInformationByHandle(handle, &info) &&
		!(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
	uint64_t const size = good ? ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow : 0;
	good = good && size <= kMostBytes;

	if (good)
	{
		bytes->resize((size_t)size);
		size_t got = 0;

		while (good && got < bytes->size())
		{
			DWORD read = 0;
			DWORD const ask = (DWORD)(bytes->size() - got > 1024 * 1024 ? 1024 * 1024 : bytes->size() - got);
			good = ReadFile(handle, bytes->data() + got, ask, &read, nullptr) && read > 0;
			got += read;
		}
	}

	CloseHandle(handle);

	if (!good)
	{
		bytes->clear();
	}

	return good;
}

static bool WriteAll(fs::path const &file, std::vector<char> const &bytes)
{
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), (std::streamsize)bytes.size());
	return out.good();
}

static std::string Sha256Hex(std::vector<char> const &bytes)
{
	Sha256Stream s;
	Sha256_Start(&s);
	Sha256_Add(&s, bytes.data(), bytes.size());
	uint8_t sum[32];
	Sha256_Finish(&s, sum);
	static char const digits[] = "0123456789abcdef";
	std::string out;

	for (uint8_t b : sum)
	{
		out += digits[b >> 4];
		out += digits[b & 15];
	}

	return out;
}

// the file in `folder` whose name is `name` case-blind (Windows' own rule), or an empty path
static fs::path Find(fs::path const &folder, std::string const &name)
{
	std::error_code e;

	for (fs::directory_iterator it(folder, e), end; !e && it != end; it.increment(e))
	{
		if (Lower(it->path().filename().string()) == Lower(name))
		{
			return it->path();
		}
	}

	return fs::path();
}

// every file under avp_rifs (the editor's own OpenedByEditor apart): its name from avp_rifs, lower case -> "<size> <sha256>"
static std::map<std::string, std::string> Listing(fs::path const &rifs)
{
	std::map<std::string, std::string> out;
	std::error_code e;

	for (fs::recursive_directory_iterator it(rifs, fs::directory_options::none, e), end; !e && it != end; it.increment(e))
	{
		std::string const relative = Lower(fs::relative(it->path(), rifs, e).string());

		// (the editor's own folder, and only it: "openedbyeditor" or what is under it, never a file whose name merely starts so: Dietrich's review, 11-23)
		if (relative == "openedbyeditor" || relative.rfind("openedbyeditor\\", 0) == 0)
		{
			if (it->is_directory(e))
			{
				it.disable_recursion_pending();
			}

			continue;
		}

		std::vector<char> bytes;

		if (fs::is_regular_file(fs::symlink_status(it->path(), e)))
		{
			out[relative] = ReadAll(it->path(), &bytes) ? std::to_string(bytes.size()) + " " + Sha256Hex(bytes) : std::string("unreadable");
		}
	}

	return out;
}

static std::map<std::string, std::string> ReadSnapshot(fs::path const &file)
{
	std::map<std::string, std::string> out;
	std::ifstream in(file, std::ios::binary);
	std::string line;

	while (std::getline(in, line))
	{
		size_t const tab = line.find('\t');

		if (tab != std::string::npos)
		{
			out[line.substr(0, tab)] = line.substr(tab + 1);
		}
	}

	return out;
}

struct Nets
{
	bool read = false;
	std::map<int32_t, uint64_t> byMaster;   // each AI module with alien waypoints: its master's index -> its WAYPOINT chunk's hash
	std::string why;
};

static Nets NetsOf(fs::path const &file)
{
	Nets n;
	OpenLevelFile open;

	if (open.Open(file.string().c_str()) != LevelOpenResult::Opened)
	{
		n.why = "the game's loader doesn't take " + file.filename().string();
		return n;
	}

	std::vector<WaypointNetModule> nets;

	if (!WaypointNets_CheckLevel(&open, &nets, &n.why))
	{
		return n;
	}

	for (WaypointNetModule const &net : nets)
	{
		n.byMaster[net.master] = net.waypointHash;
	}

	n.read = true;
	return n;
}

// a level done by the run: its .rif changed from its bytes before the run, the game's loader takes it, the networks it had kept, at least one new
static bool Done(fs::path const &level, fs::path const &beforeFile, std::string *words)
{
	std::vector<char> now, was;

	if (!ReadAll(level, &now) || !ReadAll(beforeFile, &was) || now == was)
	{
		*words = "its .rif is as it was before the run (not changed)";
		return false;
	}

	Nets const a = NetsOf(beforeFile), b = NetsOf(level);

	if (!a.read || !b.read)
	{
		*words = "its networks can't be read: " + (b.read ? a.why : b.why);
		return false;
	}

	size_t kept = 0, added = 0;

	for (auto const &[master, hash] : a.byMaster)
	{
		auto const it = b.byMaster.find(master);
		kept += (it != b.byMaster.end() && it->second == hash) ? 1 : 0;
	}

	for (auto const &[master, hash] : b.byMaster)
	{
		added += a.byMaster.count(master) ? 0 : 1;
	}

	*words = std::to_string(added) + " AI module(s) got alien waypoints they didn't have, " + std::to_string(kept) + " of its " + std::to_string(a.byMaster.size()) + " networks kept";
	return kept == a.byMaster.size() && added >= 1;
}

// The data folder must be run-shards.sh's private shard copy (...\s<k>\lv\<folder>) before anything is written in it
static bool IsShardCopy(fs::path const &data, std::string *why)
{
	std::error_code e;
	fs::path const full = fs::weakly_canonical(fs::absolute(data, e), e);
	std::string const lv = Lower(full.parent_path().filename().string()), shard = Lower(full.parent_path().parent_path().filename().string());
	std::string const run = full.parent_path().parent_path().parent_path().filename().string();
	std::string const shards = Lower(full.parent_path().parent_path().parent_path().parent_path().filename().string());
	bool const shardName = shard.size() >= 2 && shard[0] == 's' && shard.find_first_not_of("0123456789", 1) == std::string::npos;
	// run-shards' own layout, every part of it (Dietrich's review, 11-23): ...\shards\<run>\s<k>\lv\<folder>, the run's name as run-shards allows one
	// (letters, digits, '.', '_', '-', 1 to 60), so a folder that only ends like a shard's is no shard's
	bool const runName = !run.empty() && run.size() <= 60 && run != "." && run != ".." &&
		run.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-") == std::string::npos;
	*why = full.string();
	return !e && lv == "lv" && shardName && runName && shards == "shards";
}

// The 32 campaign levels back to the stock folder's bytes, and every one of their .rif.bak files deleted
static bool Restore(fs::path const &rifs, fs::path const &stockRifs)
{
	bool good = true;
	std::error_code e;

	for (bool changed : { true, false })
	{
		for (std::string const &name : changed ? kChanged : kUntouched)
		{
			fs::path const s = Find(stockRifs, name + ".rif");
			std::vector<char> bytes;

			if (!ReadAll(s, &bytes))
			{
				Check(false, name + ".rif: not in the stock levels folder " + stockRifs.string());
				return false;
			}

			fs::path const there = Find(rifs, name + ".rif");
			fs::path const target = there.empty() ? rifs / s.filename() : there;
			good = WriteAll(target, bytes) && good;

			for (fs::path bak; !(bak = Find(rifs, name + ".rif.bak")).empty();)
			{
				if (!fs::remove(bak, e))
				{
					good = false;
					break;
				}
			}
		}
	}

	return good;
}

static std::vector<std::string> Split(std::string const &s, char by)
{
	std::vector<std::string> out;
	size_t at = 0;

	for (;;)
	{
		size_t const end = s.find(by, at);
		out.push_back(s.substr(at, end == std::string::npos ? std::string::npos : end - at));

		if (end == std::string::npos)
		{
			return out;
		}

		at = end + 1;
	}
}

static int Usage()
{
	printf("  FAILED  usage: verify_all_stock_maps prepare|after|again|cancelled|untouched|restore <the data folder> <the stock levels folder> [options], or same <a> <b>\n");
	return 1;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	std::string const check = argc >= 2 ? argv[1] : "";

	if (check == "same")
	{
		std::vector<char> a, b;
		fs::path first, second;

		if (argc != 4)
		{
			return Usage();
		}

		bool const read = WholePath(argv[2], false, &first) && WholePath(argv[3], false, &second) && ReadAll(first, &a) && ReadAll(second, &b);
		size_t at = 0;

		while (read && at < a.size() && at < b.size() && a[at] == b[at])
		{
			at++;
		}

		std::string const refused = " path is refused (a plain file that exists, not a link, with no \"..\")";
		Check(read && a == b, !read ? (first.empty() ? "the first" + refused : second.empty() ? "the second" + refused : first.string() + " or " + second.string() + " can't be read") :
			first.string() + " and " + second.string() + (a == b ? " are the same " + std::to_string(a.size()) + " bytes" : " differ, first at byte " + std::to_string(at) + " (" +
			std::to_string(a.size()) + " and " + std::to_string(b.size()) + " bytes)"));
		return failed ? 1 : 0;
	}

	std::set<std::string> const checks = { "prepare", "after", "again", "cancelled", "untouched", "restore" };

	if (!checks.count(check) || argc < 4)
	{
		return Usage();
	}

	fs::path data, stockRifs;

	if (!WholePath(argv[2], true, &data) || !WholePath(argv[3], true, &stockRifs))
	{
		Check(false, "the data folder and the stock levels folder are folders that exist, not links, with no \"..\" (" + std::string(data.empty() ? "the data folder" :
			"the stock levels folder") + " is refused)");
		return 1;
	}

	fs::path const rifs = data / "avp_rifs";
	fs::path const work = data.parent_path() / "all-stock-maps", snapshot = work / "before.txt";
	std::string where;

	// THE DATA FOLDER'S avp_rifs: never a link or a junction, and never the stock folder itself (Dietrich's review, 11-23). Through a junction to the stock
	// folder, prepare's keep= deleted 21 levels from a copy of stock (measured on the verifier before this, AvP-builds-excluded/frost-should-test).
	{
		std::error_code e;
		DWORD const kind = GetFileAttributesW(rifs.wstring().c_str());

		if (kind == INVALID_FILE_ATTRIBUTES || (kind & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || fs::equivalent(rifs, stockRifs, e))
		{
			Check(false, "the data folder's avp_rifs is a folder of its own, not a link or a junction, and not the stock levels folder: " + rifs.string());
			return 1;
		}
	}

	if (check == "prepare" || check == "restore")
	{
		if (!IsShardCopy(data, &where))
		{
			Check(false, "the data folder is a shard's private copy (...\\shards\\<run>\\s<k>\\lv\\<folder>), so writing in it touches nothing else: it is " + where);
			return 1;
		}

		Check(Restore(rifs, stockRifs), "the 32 campaign levels are stock's bytes again, with no .rif.bak");

		if (check == "restore" || failed)
		{
			return failed ? 1 : 0;
		}

		std::set<std::string> keep;
		std::map<std::string, std::string> expected;   // a level the fixture made the run refuse or fail: its outcome, written for the checks after
		std::error_code e;
		fs::path const twins = data.parent_path() / "all-stock-maps-twins";   // (the other name of a hard-linked .rif.bak: outside the data folder, on its disk)

		fs::remove_all(twins, e);

		for (int a = 4; a < argc; a++)
		{
			std::string const option = argv[a];
			size_t const equals = option.find('=');
			std::string const key = option.substr(0, equals), value = equals == std::string::npos ? "" : Lower(option.substr(equals + 1));
			std::vector<std::string> pair;   // the levels the option names, each as the table spells it ("" for a name that isn't a campaign level)

			for (std::string const &given : Split(value, ':'))
			{
				pair.push_back(TableName(given));
			}

			if (key == "keep")
			{
				for (std::string const &given : Split(value, ','))
				{
					std::string const name = TableName(given);
					bool const marineOrPredator = std::find(kChanged.begin(), kChanged.end(), name) != kChanged.end();
					Check(marineOrPredator, "keep " + (name.empty() ? std::string("a name that isn't a level") : name) + ": a Marine or Predator level");

					if (marineOrPredator)
					{
						keep.insert(name);
					}
				}
			}
			else if ((key == "replace" || key == "bak") && pair.size() == 2 && !pair[0].empty() && !pair[1].empty())
			{
				std::vector<char> bytes;
				fs::path const level = Find(rifs, pair[0] + ".rif");
				bool const made = ReadAll(Find(stockRifs, pair[1] + ".rif"), &bytes) && !level.empty() &&
					WriteAll(key == "replace" ? level : fs::path(level.string() + ".bak"), bytes);
				Check(made, key == "replace" ? pair[0] + ".rif now holds stock " + pair[1] + "'s bytes (not Steam's own " + pair[0] + ")" :
					pair[0] + ".rif.bak is there already, holding stock " + pair[1] + "'s bytes");
			}
			// THE RUN'S REFUSALS (Vasquez's review of the engine, 11-22): a FOLDER where the .rif.bak would go, and a .rif.bak that is one of a HARD LINK's
			// two names, must each make the run REFUSE that level (the backup can't be written as a new plain file), leaving it untouched; a level cut to
			// half its bytes must make it FAIL (the loader refuses it), not saved, and the run go on to the next
			else if ((key == "bakdir" || key == "baklink" || key == "truncate") && pair.size() == 1 && std::find(kChanged.begin(), kChanged.end(), pair[0]) != kChanged.end())
			{
				std::string const name = pair[0];   // the table's text
				fs::path const level = Find(rifs, name + ".rif");
				fs::path const bak = fs::path(level.string() + ".bak");
				std::vector<char> bytes;
				bool made = !level.empty();

				if (key == "bakdir")
				{
					made = made && fs::create_directory(bak, e);
				}
				else if (key == "baklink")
				{
					fs::create_directories(twins, e);
					made = made && ReadAll(level, &bytes) && WriteAll(twins / (name + ".rif.bak"), bytes);
					fs::create_hard_link(twins / (name + ".rif.bak"), bak, e);
					made = made && !e;
				}
				else
				{
					made = made && ReadAll(level, &bytes) && bytes.size() > 64;
					bytes.resize(bytes.size() / 2);
					made = made && WriteAll(level, bytes);
				}

				expected[name] = key == "truncate" ? "failed" : "refused";
				Check(made, key == "bakdir" ? name + ".rif.bak is a FOLDER (the run must refuse " + name + ")" : key == "baklink" ?
					name + ".rif.bak is one of a hard link's two names (the run must refuse " + name + ")" : name + ".rif is cut to half its bytes (the run must fail it)");
			}
			else
			{
				Check(false, "a fixture option this program doesn't know: " + option);
			}
		}

		if (!keep.empty())
		{
			size_t gone = 0;

			for (std::string const &name : kChanged)
			{
				fs::path const level = Find(rifs, name + ".rif");

				if (!keep.count(name) && !level.empty() && fs::remove(level, e))
				{
					gone++;
				}
			}

			Check(gone + keep.size() == kChanged.size(), "only " + std::to_string(keep.size()) + " of the 22 Marine and Predator levels left in the copy (" + std::to_string(gone) +
				" taken out, so the run finds them missing)");
		}

		// the snapshot, and each campaign level's bytes before the run, in the work folder
		fs::remove_all(work, e);
		fs::create_directories(work / "levels", e);
		std::map<std::string, std::string> const now = Listing(rifs);
		std::ofstream out(snapshot, std::ios::binary);

		for (auto const &[name, sum] : now)
		{
			out << name << "\t" << sum << "\n";
		}

		out.close();
		bool kept = true;

		for (bool changed : { true, false })
		{
			for (std::string const &name : changed ? kChanged : kUntouched)
			{
				std::vector<char> bytes;
				fs::path const level = Find(rifs, name + ".rif");
				kept = (level.empty() || (ReadAll(level, &bytes) && WriteAll(work / "levels" / (name + ".rif"), bytes))) && kept;
			}
		}

		std::ofstream expect(work / "expected.txt", std::ios::binary);

		for (auto const &[name, outcome] : expected)
		{
			expect << name << "\t" << outcome << "\n";
		}

		expect.close();
		Check(!out.fail() && !expect.fail() && kept && now.size() > 10, "every file under avp_rifs written down (" + std::to_string(now.size()) +
			") and each campaign level's bytes kept, in " + work.string());
		return failed ? 1 : 0;
	}

	// THE CHECKS AFTER A RUN, against the snapshot prepare wrote
	std::map<std::string, std::string> const before = ReadSnapshot(snapshot);

	if (before.empty())
	{
		Check(false, "the snapshot from before the run is read (" + snapshot.string() + ")");
		return 1;
	}

	std::map<std::string, std::string> const expectedAfter = ReadSnapshot(work / "expected.txt");   // the levels the fixture made the run refuse or fail
	std::map<std::string, std::string> const now = Listing(rifs);
	std::set<std::string> allowed;
	std::vector<std::string> held, notHeld;   // of the 22, the ones the copy held before the run, and the ones it didn't (the run's "missing")
	size_t done = 0, untouched = 0;

	for (std::string const &name : check == "untouched" ? std::vector<std::string>() : kChanged)
	{
		fs::path const kept = work / "levels" / (name + ".rif");
		std::error_code e;

		if (!fs::exists(kept, e))
		{
			bool const absent = Find(rifs, name + ".rif").empty() && Find(rifs, name + ".rif.bak").empty();
			Check(absent, name + ": not in the copy before the run, and still not there, with no .rif.bak");
			notHeld.push_back(name);
			continue;
		}

		held.push_back(name);
		fs::path const level = Find(rifs, name + ".rif");
		std::string const levelKey = Lower(level.filename().string()), bakKey = levelKey + ".bak";
		allowed.insert(levelKey);
		allowed.insert(bakKey);
		fs::path const bak = Find(rifs, name + ".rif.bak");
		std::vector<char> was, backup;
		ReadAll(kept, &was);
		bool const hadBak = before.count(bakKey) == 1;
		bool const bakRight = !bak.empty() && ReadAll(bak, &backup) && (hadBak ? now.count(bakKey) && now.at(bakKey) == before.at(bakKey) : backup == was);
		std::string const bakWords = bak.empty() ? "is MISSING" : hadBak ? (bakRight ? "was there and is unchanged" : "was there and was CHANGED") :
			(bakRight ? "holds its bytes from before the run" : "does NOT hold its bytes from before the run");
		// a level the fixture made the run refuse (a folder, or a hard link's name, where its .rif.bak goes) or fail (cut short): untouched
		auto const expectIt = expectedAfter.find(name);

		if (expectIt != expectedAfter.end())
		{
			std::error_code de;
			bool const asItWas = now.count(levelKey) && before.count(levelKey) && now.at(levelKey) == before.at(levelKey);
			bool const isFolder = !bak.empty() && fs::is_directory(fs::symlink_status(bak, de));
			bool const refused = expectIt->second == "refused";
			bool const bakAsItWas = refused ? (isFolder || (hadBak && bakRight)) : (bak.empty() || (!hadBak && bakRight));
			std::string const bakState = refused ? (isFolder ? "is still the folder put there" : hadBak ? (bakRight ? "is the hard link's name, unchanged" : "the hard link's name was CHANGED") :
				"is MISSING") : (bak.empty() ? "isn't there" : bakWords);
			Check(asItWas && bakAsItWas, name + ": " + (refused ? "REFUSED" : "FAILED") + ", as the fixture must make it: its .rif " + (asItWas ? "is as it was" : "was CHANGED") +
				", its .rif.bak " + bakState);
			continue;
		}

		std::string words;
		bool const isDone = Done(level, kept, &words);

		if (check == "cancelled" && !isDone)
		{
			bool const asItWas = now.count(levelKey) && before.count(levelKey) && now.at(levelKey) == before.at(levelKey);
			bool const bakAsItWas = hadBak ? bakRight : (bak.empty() || bakRight);
			Check(asItWas && bakAsItWas, name + ": untouched: its .rif is as it was, its .rif.bak " + (bak.empty() ? "isn't there" : bakWords));
			untouched++;
			continue;
		}

		Check(isDone && bakRight, name + ": " + words + "; its .rif.bak " + bakWords);
		done += isDone ? 1 : 0;
	}

	if (check == "cancelled")
	{
		Check(done >= 1 && untouched >= 1, "the Cancel landed part way: " + std::to_string(done) + " done, " + std::to_string(untouched) + " untouched");
	}

	if (check != "untouched")
	{
		for (std::string const &name : kUntouched)
		{
			fs::path const level = Find(rifs, name + ".rif");
			std::string const key = Lower(level.filename().string());
			bool const asItWas = !level.empty() && now.count(key) && before.count(key) && now.at(key) == before.at(key);
			bool const noBak = Find(rifs, name + ".rif.bak").empty();
			Check(asItWas && noBak, name + " (Alien campaign): " + (asItWas ? "as it was" : "CHANGED") + (noBak ? ", no .rif.bak" : ", has a .rif.bak"));
			allowed.insert(key);   // (checked just above)
		}
	}
	else
	{
		allowed.clear();
	}

	// everything else: as the snapshot wrote it
	std::vector<std::string> different;

	for (auto const &[name, sum] : before)
	{
		auto const it = now.find(name);

		if (!allowed.count(name) && (it == now.end() || it->second != sum))
		{
			different.push_back(name + (it == now.end() ? " (gone)" : " (changed)"));
		}
	}

	for (auto const &[name, sum] : now)
	{
		if (!before.count(name) && !allowed.count(name))
		{
			different.push_back(name + " (new)");
		}
	}

	std::string list;

	for (size_t i = 0; i < different.size() && i < 12; i++)
	{
		list += (i ? ", " : ": ") + different[i];
	}

	Check(different.empty(), std::string(check == "untouched" ? "nothing" : "nothing else") + " under avp_rifs changed, came or went (" + std::to_string(different.size()) + list + ")");

	// the report: every one of the 22 the copy held named in a line of the editor's log that says "All stock maps"
	if (argc == 6 && strcmp(argv[4], "log") == 0)
	{
		std::vector<char> log;
		fs::path logFile;
		bool const read = WholePath(argv[5], false, &logFile) && ReadAll(logFile, &log);
		// (Drake's report lines, all_stock_maps_ui.cpp AllStockMaps_LogLines: "All stock maps: the run ended: ...", then one per level in the run's order,
		// "All stock maps: <CHANGED|SKIPPED|REFUSED|FAILED|NOT STARTED> <file>: ...", the file as the engine builds it, "derelict.RIF". The whole prefix is
		// matched, case-blind, so the engine's own progress lines, which also name each file, can't stand in for the report.)
		std::string const text = Lower(std::string(log.begin(), log.end()));
		std::string missing;
		size_t missingCount = 0;
		auto has = [&text](std::string const &heading, std::string const &name) { return text.find("all stock maps: " + heading + " " + name + ".rif") != std::string::npos; };

		for (std::string const &name : held)
		{
			// (a second sitting, "again": every AI module the first run linked keeps its waypoints, so there is nothing to write and the engine says SKIPPED,
			// "nothing written, so the file is left as it was": measured on exe-cache/39afb952.exe, 2026-11-22)
			auto const it = expectedAfter.find(name);
			std::string const heading = it != expectedAfter.end() ? it->second : check == "again" ? "skipped" : "changed";

			if (!has(heading, name) && !(check == "cancelled" && has("not started", name)))
			{
				missing += (missingCount++ ? ", " : ": ") + name + (check == "cancelled" ? " (neither " : " (not ") + heading + (check == "cancelled" ? " nor not started)" : ")");
			}
		}

		for (std::string const &name : notHeld)
		{
			if (!has("skipped", name))
			{
				missing += (missingCount++ ? ", " : ": ") + name + " (not SKIPPED)";
			}
		}

		bool const ended = text.find("all stock maps: the run ended: ") != std::string::npos;
		Check(read && ended && missingCount == 0, "the editor's log has the run's report: its end line" + std::string(ended ? "" : " (MISSING)") + ", and each of the 22 under its "
			"heading, CHANGED or (missing from the copy) SKIPPED (" + std::to_string(missingCount) + " not" + missing + ")");
	}
	else if (argc != 4)
	{
		return Usage();
	}

	return failed ? 1 : 0;
}
