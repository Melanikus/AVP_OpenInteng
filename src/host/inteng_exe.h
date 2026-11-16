// inteng_exe.h: the user's own INSTALLED inteng.exe, read at run time for inteng's binary data (its compiled-in shapes, its tables): found, checked to be the very exe that was traced, and
// read only by bounded reads at its addresses. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng, shared with Drake's light marker. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// THE RULING (Melanikus via Bishop 11-16): inteng's short texts may stay in our source; its BINARY data (fonts, shapes, anything from inteng.exe's .data) is never copied into our source and is
// read when the editor runs from his own files. The program looks for inteng.exe next to itself, then in the folder its IntengTools setting names (editor_main.cpp); only the exe whose sha256 is
// kIntengExeSha256 is used, so every address of our maps (AvP-docs\engine\inteng) means what was traced.
//
// THE FILE IS HOSTILE INPUT all the same: its headers and section table are checked against the file (IntengExe_Parse), and a read is given only when every byte asked is inside one section's
// bytes in the file (IntengExe_ReadBytes). Nothing is ever run.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	// The traced inteng.exe (AvP-docs\engine\inteng\dump-holes.md): 2117632 bytes
	char const kIntengExeSha256[] = "c52294569560217e45eb2bca0efbd3327339a1d1be5628c70eaef6a48d3a22cb";
	size_t const kIntengExeMostBytes = 4 * 1024 * 1024;
	int const kIntengExeMostSections = 32;

	struct IntengExeSection
	{
		uint32_t va;        // its address (the image base added)
		uint32_t rawSize;   // its bytes in the file
		uint32_t rawAt;     // where they start in the file
	};

	struct IntengExeImage
	{
		std::vector<uint8_t> bytes;
		uint32_t imageBase = 0;
		std::vector<IntengExeSection> sections;
	};

	enum class IntengExeRefusal
	{
		None,
		NotFound,          // no inteng.exe in the places looked at
		NotAFile,          // the name is a folder, a link or a device, or it couldn't be read whole
		TooLong,           // bigger than kIntengExeMostBytes
		NotTheTracedExe,   // its sha256 is not kIntengExeSha256
		NotAnExe,          // not a 32-bit Windows program (MZ, PE, i386, PE32), or its headers don't fit the file
		BadSections,       // no section, more than kIntengExeMostSections, or one whose bytes are not in the file
	};

	// In plain words, for the status line and the log: static text, never null
	char const *IntengExe_Why(IntengExeRefusal why);

	// The sha256 of the bytes, as 64 lower-case hex letters (FIPS 180-4; our own code)
	std::string IntengExe_Sha256(uint8_t const *bytes, size_t size);

	// The headers and the section table of a program's bytes, every number checked against the file. The hash is not checked here.
	bool IntengExe_Parse(std::vector<uint8_t> bytes, IntengExeImage *out, IntengExeRefusal *why);

	// The first of the paths that names a file (later ones only when the earlier are not there): a plain file of at most kIntengExeMostBytes, whose sha256 is kIntengExeSha256, parsed.
	// *from (when given) gets the path read.
	bool IntengExe_Load(std::vector<std::string> const &paths, IntengExeImage *out, std::string *from, IntengExeRefusal *why);

	// count bytes at the address va, only when all of them are inside ONE section's bytes in the file; false (out untouched) otherwise
	bool IntengExe_ReadBytes(IntengExeImage const &image, uint32_t va, size_t count, uint8_t *out);
	bool IntengExe_ReadInt32(IntengExeImage const &image, uint32_t va, int32_t *out);
	bool IntengExe_ReadUint32(IntengExeImage const &image, uint32_t va, uint32_t *out);

	// The editor's one loaded inteng.exe (set once at start by editor_main.cpp), or null with *why saying why there is none
	void IntengExe_SetShared(IntengExeImage const *image, std::string const &why);
	IntengExeImage const *IntengExe_Shared(std::string *why);
}
