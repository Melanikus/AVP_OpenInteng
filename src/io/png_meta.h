// png_meta.h: a screenshot's context carried INSIDE its PNG file (GitHub #78: the screenshot key; Drake makes the key and the context's JSON, this file puts it in the picture and reads it back).
// Written 2026-11-17 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IS WRITTEN: one iTXt chunk (the PNG standard's international text) with the keyword "AVP_OpenInteng", compression flag 0 and method 0 (not compressed), an empty language tag and an empty
// translated keyword, then the JSON as UTF-8, put just before the picture's IEND, its CRC worked out. Picture viewers ignore it; tools/read-shot-context.js prints it.
//
// READ AS HOSTILE INPUT (a PNG can come from anyone: a report's attachment): the 8-byte signature; then every chunk, in order: its length inside the file (and below 2^31), its type four letters,
// its CRC right; IHDR first (13 bytes), IEND last (empty) with nothing after it; exactly one iTXt with our keyword (none: NotFound, two or more: Duplicate); its flags 0 and 0, its language and
// translated keyword empty; its text at most kPngMetaMostTextBytes, UTF-8 with no zero byte, and ONE well-formed JSON object (nested at most 64 deep). Anything else is refused, and *why says where
// and what in plain words. Nothing is ever read past the bytes given.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace host
{
	enum class PngMetaResult
	{
		Done,
		NotPng,      // no PNG signature (or no bytes at all)
		BadChunk,    // a chunk's type is not four letters, its length is past 2^31, IHDR is not first, IEND is not empty, or bytes follow IEND
		BadCrc,      // a chunk's CRC is wrong
		Truncated,   // a chunk runs past the end of the bytes
		NoEnd,       // the bytes end with no IEND
		Duplicate,   // Read: two or more of our chunks; Write: the picture holds one already
		TooLarge,    // the text is over kPngMetaMostTextBytes (or a file over kPngMetaMostFileBytes)
		NotFound,    // Read: the picture holds no chunk of ours
		BadText,     // our chunk's shape is not ours (compressed, a language), or the text is not UTF-8, holds a zero byte, or is not one JSON object
	};

	size_t const kPngMetaMostTextBytes = 256 * 1024;        // the JSON's UTF-8 bytes
	size_t const kPngMetaMostFileBytes = 64 * 1024 * 1024;  // a picture file read whole (a 4K screenshot is far below)
	char const *const kPngMetaKeyword = "AVP_OpenInteng";

	// Plain words for a result ("the chunk's CRC is wrong")
	char const *PngMetaWords(PngMetaResult result);

	// png with our chunk holding json, put just before IEND, into *out. Refused (out untouched): a png that fails Read's walk, one that holds our chunk already (Duplicate), json over the limit, not
	// UTF-8, holding a zero byte, or not one JSON object. *why (when not null) says what.
	PngMetaResult PngMeta_Write(std::vector<uint8_t> const &png, std::string const &json, std::vector<uint8_t> *out, std::string *why);

	// The JSON our chunk holds, read as above. *json is emptied first, and given only on Done.
	PngMetaResult PngMeta_Read(std::vector<uint8_t> const &png, std::string *json, std::string *why);

	// The same on a file. AddToFile: the file read whole (a plain file, at most kPngMetaMostFileBytes), the chunk added, then written back SAFELY: a temporary file in the same folder, read back
	// (Read must give the same JSON), then put in place; on any failure the file is as it was. ReadFile: the file read whole and Read.
	PngMetaResult PngMeta_AddToFile(char const *path, std::string const &json, std::string *why);
	PngMetaResult PngMeta_ReadFile(char const *path, std::string *json, std::string *why);
}
