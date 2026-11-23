// verify_shape_extract.cpp: Option C's stock check and report (src/host/shape_extract.h): every outside shape the given levels place, made back into its
// shape file by the editor's own extractor, written into a scratch folder, and compared with the old tools' package file of the same name. Written
// 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_shape_extract <a new scratch folder> <the package's shape folder> <level folder> [<level folder> ...]
// Reads every .rif right in each level folder (packed or plain, each at most 64 MB, unpacked by the game's own decoder with its checks), lists its outside
// shapes (ShapeExtract_Placed), makes each rescale-1 one into its file (ShapeExtract_File), and keeps, per name, the file most copies make (the commonest
// version) in the scratch folder, written as a new file (never over one). Then, per name, against the package's file:
//   import  ShapeImport_Build of the extracted file and of the package's makes the same level shape, byte for byte (the package file must hold one shape);
//   parts   each chunk the extractor wrote against the package shape's chunk of that name: SHPRAWVT (the points: their place and so their orientation),
//           SHPVNORM and SHPPNORM (vertex and polygon normals), SHPCENTR (the centre), SHPPOLYS (the polygons), SHPUVCRD (the texture coordinates),
//           SHPMRGDT, SHPPCINF, TEXTANIM; the header's +24 to +63 (counts, sizes, the radius and the version); the flags.
// PRINTS one line a name, tab-separated, then the lists: (A) placed and in the package, (B) placed and not in it, (C) in it and placed nowhere. It writes
// nothing but the scratch folder. It gives back 0 when it ran; the lines are the result (it is a report, not a pass or fail).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "huffman.hpp"
#include "shape_extract.h"
#include "shape_import.h"

// the game's memory code (our_mem.cpp, for the decoder) calls these: here they only say what happened
void avp_MessageBox(const char *message, int)
{
	printf("the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("FAILED: the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

using namespace host;
using namespace host::shape_chunks;
namespace fs = std::filesystem;

static uint64_t const kMostBytes = 64ull * 1024 * 1024;

static std::string Lower(std::string s)
{
	for (char &c : s)
	{
		c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
	}

	return s;
}

static bool ReadAll(fs::path const &file, Bytes *bytes)
{
	std::error_code e;

	if (!fs::is_regular_file(fs::symlink_status(file, e)) || e || fs::file_size(file, e) > kMostBytes || e)
	{
		return false;
	}

	std::ifstream in(file, std::ios::binary);
	bytes->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return !in.bad();
}

// A level's bytes as the game reads them: a packed one (REBCRIF1) unpacked by the game's own decoder, with its own checks
static bool Unpacked(Bytes const &raw, Bytes *out)
{
	if (raw.size() >= 8 && memcmp(raw.data(), "REBINFF2", 8) == 0)
	{
		*out = raw;
		return true;
	}

	if (raw.size() < sizeof(HuffmanPackage) || memcmp(raw.data(), COMPRESSED_RIF_IDENTIFIER, 8) != 0)
	{
		return false;
	}

	int size = 0;
	memcpy(&size, raw.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(size));

	if (size < 12 || (uint64_t)size > 4 * kMostBytes)
	{
		return false;
	}

	std::unique_ptr<char, void (*)(void *)> const unpacked(HuffmanDecompressChecked((HuffmanPackage const *)raw.data(), (int)raw.size()), free);

	if (!unpacked)
	{
		return false;
	}

	out->assign(unpacked.get(), unpacked.get() + size);
	return true;
}

// The first chunk of each name inside a span, by name
static std::map<std::string, Bytes> PayloadsOf(Bytes const &b, size_t from, size_t to)
{
	std::map<std::string, Bytes> out;
	std::vector<Span> parts;

	if (Children(b, from, to, &parts))
	{
		for (Span const &p : parts)
		{
			std::string const name(b.data() + p.at, b.data() + p.at + 8);

			if (!out.count(name))
			{
				out[name] = Bytes(b.begin() + (std::ptrdiff_t)(p.at + kHeadingBytes), b.begin() + (std::ptrdiff_t)(p.at + p.size));
			}
		}
	}

	return out;
}

// The shapes (REBSHAPE) right inside a file
static std::vector<Span> ShapesOf(Bytes const &b)
{
	std::vector<Span> top, out;
	size_t total = 0;

	if (b.size() >= kHeadingBytes && ChunkSizeAt(b, 0, b.size(), &total) && Children(b, kHeadingBytes, total, &top))
	{
		for (Span const &s : top)
		{
			if (NameIs(b, s.at, "REBSHAPE"))
			{
				out.push_back(s);
			}
		}
	}

	return out;
}

struct Name
{
	std::set<std::string> levels;          // every level that places it
	size_t copies = 0, rescaled = 0;       // its rescale-1 copies, and the rescaled ones
	std::map<Bytes, size_t> variants;      // each distinct extracted file, and how many copies make it
	std::map<Bytes, std::string> from;     // a level each variant came from
	std::string refused;                   // why a copy couldn't be made, when none could
};

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 4)
	{
		printf("usage: verify_shape_extract <a new scratch folder> <the package's shape folder> <level folder> [<level folder> ...]\n");
		return 2;
	}

	fs::path const out = argv[1], package = argv[2];
	std::error_code e;

	if (fs::exists(out, e) && !fs::is_empty(out, e))
	{
		printf("refused: the scratch folder %s is not new and empty\n", argv[1]);
		return 2;
	}

	fs::create_directories(out, e);
	std::map<std::string, Name> names;
	size_t levels = 0, unreadable = 0;

	for (int a = 3; a < argc; a++)
	{
		std::vector<fs::path> files;

		for (fs::directory_iterator it(argv[a], e), end; !e && it != end; it.increment(e))
		{
			if (Lower(it->path().extension().string()) == ".rif")
			{
				files.push_back(it->path());
			}
		}

		std::sort(files.begin(), files.end(), [](fs::path const &x, fs::path const &y) { return Lower(x.filename().string()) < Lower(y.filename().string()); });

		for (fs::path const &file : files)
		{
			Bytes raw, level;

			if (!ReadAll(file, &raw) || !Unpacked(raw, &level))
			{
				unreadable++;
				printf("UNREADABLE\t%s\n", file.string().c_str());
				continue;
			}

			levels++;
			std::vector<ShapeExtractPlaced> placed;

			if (!ShapeExtract_Placed(level, &placed))
			{
				printf("UNWALKABLE\t%s\n", file.string().c_str());
				continue;
			}

			std::string const where = fs::path(argv[a]).filename().string() + "\\" + file.filename().string();

			for (ShapeExtractPlaced const &p : placed)
			{
				Name &n = names[Lower(p.fileName)];
				n.levels.insert(where);

				if (p.rescale != 1.0)
				{
					n.rescaled++;
					continue;
				}

				Bytes made;
				std::string plain, why;
				ShapeExtractResult const r = ShapeExtract_File(level, p, &made, &plain, &why);

				if (r != ShapeExtractResult::Done)
				{
					n.refused = std::string(ShapeExtractWords(r)) + (why.empty() ? "" : ": " + why);
					continue;
				}

				n.copies++;
				n.variants[made]++;

				if (!n.from.count(made))
				{
					n.from[made] = where;
				}
			}
		}
	}

	// the package's own files, by name
	std::map<std::string, fs::path> inPackage;

	for (fs::directory_iterator it(package, e), end; !e && it != end; it.increment(e))
	{
		if (Lower(it->path().extension().string()) == ".rif")
		{
			inPackage[Lower(it->path().filename().string())] = it->path();
		}
	}

	printf("levels read %zu, unreadable %zu; outside-shape names placed %zu; package files %zu\n", levels, unreadable, names.size(), inPackage.size());
	printf("NAME\tLEVELS\tCOPIES(rescale 1)\tRESCALED\tVARIANTS\tIN PACKAGE\tPACKAGE SHAPES\tIMPORT-EQUAL\tPARTS (= equal, ~ differs, - not in one)\tFROM\n");
	size_t inBoth = 0, importEqual = 0, notIn = 0;
	char const *const partNames[] = { "SHPRAWVT", "SHPVNORM", "SHPPNORM", "SHPCENTR", "SHPPOLYS", "SHPUVCRD", "SHPMRGDT", "SHPPCINF", "TEXTANIM" };

	for (auto const &[name, n] : names)
	{
		// the commonest variant, written as a new file
		Bytes const *best = nullptr;
		size_t bestCount = 0;

		for (auto const &[bytes, count] : n.variants)
		{
			if (count > bestCount)
			{
				best = &bytes;
				bestCount = count;
			}
		}

		if (best)
		{
			fs::path const target = out / name;
			HANDLE const h = CreateFileW(target.wstring().c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

			if (h != INVALID_HANDLE_VALUE)
			{
				DWORD wrote = 0;
				WriteFile(h, best->data(), (DWORD)best->size(), &wrote, NULL);
				CloseHandle(h);
			}
		}

		auto const pk = inPackage.find(name);
		std::string importWords = "-", parts, shapesWords = "-";

		if (pk != inPackage.end())
		{
			inBoth++;
			Bytes pfile, pf;
			ReadAll(pk->second, &pfile);
			bool const readable = Unpacked(pfile, &pf);
			std::vector<Span> const ps = readable ? ShapesOf(pf) : std::vector<Span>();
			shapesWords = readable ? std::to_string(ps.size()) : "unreadable";

			if (best && ps.size() == 1)
			{
				Bytes a, b;
				bool const ok = ShapeImport_Build(*best, name, 1, &a, nullptr) == ShapeImportResult::Done && ShapeImport_Build(pf, name, 1, &b, nullptr) == ShapeImportResult::Done;
				importWords = ok && a == b ? "yes" : ok ? "no" : "import refused";
				importEqual += ok && a == b ? 1 : 0;

				std::vector<Span> const es = ShapesOf(*best);
				std::map<std::string, Bytes> const ep = es.empty() ? std::map<std::string, Bytes>() : PayloadsOf(*best, es[0].at + kHeadingBytes, es[0].at + es[0].size);
				std::map<std::string, Bytes> const pp = PayloadsOf(pf, ps[0].at + kHeadingBytes, ps[0].at + ps[0].size);

				for (char const *part : partNames)
				{
					bool const inE = ep.count(part) == 1, inP = pp.count(part) == 1;
					parts += std::string(part).substr(3) + (inE && inP ? (ep.at(part) == pp.at(part) ? "=" : "~") : (inE || inP ? "-" : "")) + " ";
				}

				if (ep.count("SHPHEAD1") && pp.count("SHPHEAD1") && ep.at("SHPHEAD1").size() >= 64 && pp.at("SHPHEAD1").size() >= 64)
				{
					Bytes const &eh = ep.at("SHPHEAD1"), &ph = pp.at("SHPHEAD1");
					parts += std::string("HEAD24-63") + (memcmp(eh.data() + 24, ph.data() + 24, 40) == 0 ? "=" : "~") + " FLAGS" +
						(memcmp(eh.data(), ph.data(), 4) == 0 ? "=" : "~");
				}

				// the environment data the import reads: RIFFNAME, BMPNAMES, BMNAMEXT, BMNAMVER; and BMPNAMES again with every picture's "priority and
				// transparency are valid" flag (0x100, bitmap_names.hpp) set, as the package's newer files have it and a level's copy hasn't
				std::vector<Span> etop, ptop;
				std::map<std::string, Bytes> ee, pe;
				size_t total = 0;

				if (ChunkSizeAt(*best, 0, best->size(), &total) && Children(*best, kHeadingBytes, total, &etop))
				{
					for (Span const &s : etop)
					{
						if (NameIs(*best, s.at, "REBENVDT"))
						{
							ee = PayloadsOf(*best, s.at + kHeadingBytes, s.at + s.size);
						}
					}
				}

				if (ChunkSizeAt(pf, 0, pf.size(), &total) && Children(pf, kHeadingBytes, total, &ptop))
				{
					for (Span const &s : ptop)
					{
						if (NameIs(pf, s.at, "REBENVDT"))
						{
							pe = PayloadsOf(pf, s.at + kHeadingBytes, s.at + s.size);
						}
					}
				}

				for (char const *part : { "RIFFNAME", "BMPNAMES", "BMNAMEXT", "BMNAMVER" })
				{
					bool const inE = ee.count(part) == 1, inP = pe.count(part) == 1;
					parts += std::string(" ") + part + (inE && inP ? (ee.at(part) == pe.at(part) ? "=" : "~") : (inE || inP ? "-" : "0"));
				}

				if (ee.count("BMPNAMES") && pe.count("BMPNAMES") && ee.at("BMPNAMES") != pe.at("BMPNAMES"))
				{
					// each picture's flags, the first of its five numbers, with 0x100 set; the names in between walked as the list's own layout
					Bytes withValid = ee.at("BMPNAMES");
					size_t at = 4, count = withValid.size() >= 4 ? ((uint32_t)IntAt(withValid, 0) & 0xffff) : 0;

					for (size_t i = 0; i < count && at + 20 < withValid.size(); i++)
					{
						PutIntAt(&withValid, at, IntAt(withValid, at) | 0x100);
						size_t end = at + 20;

						while (end < withValid.size() && withValid[end] != 0)
						{
							end++;
						}

						size_t const length = end - (at + 20);
						at = at + 20 + length + (4 - length % 4);
					}

					parts += withValid == pe.at("BMPNAMES") ? " (BMPNAMES equal but for the valid flag 0x100)" : "";
				}
			}
			else if (ps.size() > 1)
			{
				importWords = "fragment file";
			}
		}
		else
		{
			notIn++;
		}

		std::string levelsWords;

		for (std::string const &l : n.levels)
		{
			levelsWords += (levelsWords.empty() ? "" : ";") + l;
		}

		printf("%s\t%zu\t%zu\t%zu\t%zu\t%s\t%s\t%s\t%s\t%s%s\n", name.c_str(), n.levels.size(), n.copies, n.rescaled, n.variants.size(), pk != inPackage.end() ? "yes" : "NO",
			shapesWords.c_str(), importWords.c_str(), parts.c_str(), best ? n.from.at(*best).c_str() : "-", n.refused.empty() ? "" : ("\trefused: " + n.refused).c_str());
		(void)levelsWords;
	}

	size_t placedNowhere = 0;

	for (auto const &[name, path] : inPackage)
	{
		if (!names.count(name))
		{
			placedNowhere++;
			printf("NOT PLACED\t%s\n", name.c_str());
		}
	}

	printf("SUMMARY: (A) placed and in the package %zu (import-equal %zu); (B) placed, not in it %zu; (C) in it, placed nowhere %zu\n", inBoth, importEqual, notIn, placedNowhere);
	return 0;
}
