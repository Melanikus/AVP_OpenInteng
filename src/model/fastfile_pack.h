// fastfile_pack.h: a .FFL pack, the game's "fast file" that holds many files in one (fastfile\Snd25.FFL, Tex29.FFL), made from a list of files, and a checker
// that reads one back as strictly as the format allows. Written 2026-11-27 by Melanikus (Frost) for AVP_OpenInteng, for Export map's sounds and textures
// (AvP-docs director/design-dietrich-sound-import-export-map.md, section 1; Dietrich leads the import and the export, this is the part that makes the bytes).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no Windows, no files. The caller reads and writes the files.
//
// WHY STRICT. Steam AvP's reader (Rebellion's FFREAD.CPP, FFHeaderI::Read) checks nothing: it takes the sizes the pack claims, mallocs them, reads them,
// then walks the entries trusting every offset and every name (FFREAD.CPP:296-343). A pack that is even slightly wrong makes it read or write outside its
// buffers, on every player's machine. So we never hand out a pack the checker below hasn't passed, and the checker passes only what both readers (Steam's,
// and AvPx's 3dc/FastFile.cpp) read exactly as it was written.
//
// THE LAYOUT (both readers agree; every number is 4 bytes, little-endian):
//   "RFFL", version 0, how many files, how many bytes of entries follow, how many bytes of data follow the entries.
//   Each entry: the file's start (counted from the first data byte), its length, its name and a zero, then zeros to a multiple of 4. So an entry is
//     8 + ((letters + 4) & ~3) bytes: the step both readers take (FFREAD.CPP:342, FastFile.cpp:216-226).
//   Then the data: each file's bytes, each starting on a multiple of 4, zeros between, and zeros at the end to a multiple of 4.
//   The pack's size is 20 + the entry bytes + the data bytes, exactly.
//
// AS STOCK'S ARE. Measured 2026-11-27 on the 108 packs of the game's fastfile folder (2,548 files; common.ffl there is no pack: it has no "RFFL" and the
// game's fastfile\ffinfo.txt doesn't list it):
//   - the names are in the order _stricmp gives: each letter made small, then compared as a number, so '_' comes before the letters (all 108; with the
//     letters made capitals instead, 19 of them are out of order);
//   - the data is in the entries' order, back to back, each start the last end rounded up to 4, the first at 0, and the data bytes a multiple of 4 (all 108);
//   - the zeros between files and at the end are zeros; but the bytes after a name's own zero are NOT always zeros (2,256 of them aren't): Rebellion's
//     packer left whatever was in its memory there. Neither reader ever reads them. Ours writes zeros, and the checker doesn't look at them, or no stock
//     pack would pass;
//   - the most in one pack: 185 files, 2,586,488 bytes, a file of 263,042 bytes, a name of 68 letters (Tex47.FFL). The limits below are far above that.
//
// A NAME is the file's name as the game asks for it, from the game's folder: "sound\Custom\MyLevel\boom.wav". The game asks with backslashes and compares
// with _stricmp, so capitals don't matter but the slash does: a name written with '/' would never be found by Steam AvP, so '/' is refused. Otherwise a
// name passes the game's own rule for a name from a level (LevelFileName_WhyRefused, avpx/3dc/level_file_name.h: its letters, digits, _ - . + & @ and space,
// no part that goes up a folder or names a device), the same characters every stock pack's names use. Two names that differ only in capitals are the same
// file to both readers, so they are refused too.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	size_t const kPackMostFiles = 1000;                       // files in one pack (stock's most: 185)
	size_t const kPackMostBytes = 64u * 1024u * 1024u;        // the whole pack: Steam AvP, a 32-bit game, holds a whole pack in memory (stock's most: 2.6 MB)
	size_t const kPackMostFileBytes = 64u * 1024u * 1024u;    // one file in it (the game's WAV limit; stock's most: 263,042 bytes)
	size_t const kPackMostNameLetters = 199;                  // a name's letters, its zero not counted (stock's most: 68; the game's rule allows 200)

	// One file to put in a pack: its name as the game asks for it (above), and its bytes
	struct PackFile
	{
		std::string name;
		std::vector<uint8_t> bytes;
	};

	// Why a pack wasn't made
	enum class PackRefused
	{
		None,
		NoFiles,          // the list is empty
		TooManyFiles,     // more than kPackMostFiles
		NameRefused,      // the game's rule for a name from a level refuses it (an empty name, a letter no game file has, a part going up or naming a device)
		NameHasSlash,     // a '/': the game asks with '\' and would never find it
		NameTooLong,      // more than kPackMostNameLetters
		NameHasNul,       // a zero inside the name: the pack would end the name there
		NameTwice,        // two files with the same name, capitals aside
		FileTooBig,       // one file over kPackMostFileBytes
		PackTooBig,       // the whole pack would be over kPackMostBytes
	};

	// The words for a refusal, for the status line and the export's README
	char const *FastFilePack_RefusedWords(PackRefused refused);

	// The rules for one name (A NAME, above): None when the name may go in a pack. NameTwice and the sizes aren't about one name, so never come from here.
	PackRefused FastFilePack_WhyNameRefused(std::string const &name);

	// Makes the pack, as stock's are (AS STOCK'S ARE, above; the files are put in the names' order, whatever order they come in). True with the pack's bytes in
	// *pack. False with *pack empty, *refused saying why and *which the place in files of the file it's about (for NoFiles 0; for TooManyFiles
	// kPackMostFiles, the first one too many; for NameTwice the later of the two; for PackTooBig the file that took it over).
	bool FastFilePack_Build(std::vector<PackFile> const &files, std::vector<uint8_t> *pack, PackRefused *refused, size_t *which);

	// One file of a pack, as the checker read it: its name, where its bytes start (from the first data byte, as the entry says) and how many there are
	struct PackListed
	{
		std::string name;
		uint32_t offset = 0;
		uint32_t length = 0;
	};

	// THE STRICT CHECKER. True only when these bytes are a pack both readers take exactly as written, and laid out as stock's and ours are: the sizes add up to
	// the bytes exactly; the version is 0; 1 to kPackMostFiles files; every entry and its name's zero inside the entry bytes, and the walk over them ending
	// exactly at their end; every name passing FastFilePack_WhyNameRefused, in _stricmp's order, none twice; every file inside the data, in the entries'
	// order, back to back, starting on 4, at most kPackMostFileBytes; the zeros between and at the end zeros. The bytes after a name's zero aren't looked at
	// (AS STOCK'S ARE). Every sum is made in 64 bits and every count is held to its limit before anything is walked, so hostile bytes can't make it loop
	// or wrap. True with *listed holding every file in the pack's order. False with *listed empty and *why saying, in plain words, the first thing wrong.
	bool FastFilePack_Check(uint8_t const *bytes, size_t size, std::vector<PackListed> *listed, std::string *why);
}
