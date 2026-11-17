// png_meta.cpp: the screenshot's context in its PNG (png_meta.h says what is written and how it is read). Written 2026-11-17 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "png_meta.h"

#include <string.h>

#include <windows.h>

namespace host
{
	namespace
	{
		uint8_t const kSignature[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
		size_t const kMostChunks = 1000000;   // a guard: every chunk is at least 12 bytes, so a 64 MB file has fewer
		int const kMostJsonDepth = 64;

		// The PNG's CRC-32 (ISO 3309 / ITU-T V.42, as the PNG standard's own sample code): over the chunk's type and data
		uint32_t Crc(uint8_t const *bytes, size_t length, uint32_t crc = 0xffffffffu)
		{
			static uint32_t table[256];
			static bool made = false;

			if (!made)
			{
				for (uint32_t n = 0; n < 256; n++)
				{
					uint32_t c = n;

					for (int k = 0; k < 8; k++)
					{
						c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
					}

					table[n] = c;
				}

				made = true;
			}

			for (size_t i = 0; i < length; i++)
			{
				crc = table[(crc ^ bytes[i]) & 0xff] ^ (crc >> 8);
			}

			return crc;
		}

		uint32_t BigEndian(uint8_t const *at)
		{
			return ((uint32_t)at[0] << 24) | ((uint32_t)at[1] << 16) | ((uint32_t)at[2] << 8) | (uint32_t)at[3];
		}

		void PutBigEndian(std::vector<uint8_t> *out, uint32_t value)
		{
			out->push_back((uint8_t)(value >> 24));
			out->push_back((uint8_t)(value >> 16));
			out->push_back((uint8_t)(value >> 8));
			out->push_back((uint8_t)value);
		}

		void Say(std::string *why, std::string const &words)
		{
			if (why)
			{
				*why = words;
			}
		}

		struct Chunk
		{
			size_t at;       // where its length starts
			uint32_t length; // its data's bytes
			char type[5];
		};

		bool IsLetter(uint8_t c)
		{
			return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
		}

		bool IsOurs(std::vector<uint8_t> const &png, Chunk const &chunk)
		{
			size_t const keyword = strlen(kPngMetaKeyword);

			return strcmp(chunk.type, "iTXt") == 0 && chunk.length > keyword && memcmp(png.data() + chunk.at + 8, kPngMetaKeyword, keyword) == 0 && png[chunk.at + 8 + keyword] == 0;
		}

		// Every chunk, checked as png_meta.h says, into *chunks
		PngMetaResult Walk(std::vector<uint8_t> const &png, std::vector<Chunk> *chunks, std::string *why)
		{
			chunks->clear();

			if (png.size() < sizeof(kSignature) || memcmp(png.data(), kSignature, sizeof(kSignature)) != 0)
			{
				Say(why, "the bytes do not start with the PNG signature");
				return PngMetaResult::NotPng;
			}

			size_t at = sizeof(kSignature);

			while (at < png.size())
			{
				if (chunks->size() >= kMostChunks)
				{
					Say(why, "more than " + std::to_string(kMostChunks) + " chunks");
					return PngMetaResult::BadChunk;
				}

				if (png.size() - at < 12)
				{
					Say(why, "the chunk at byte " + std::to_string(at) + " is cut short: " + std::to_string(png.size() - at) + " bytes left, a chunk takes at least 12");
					return PngMetaResult::Truncated;
				}

				Chunk chunk;

				chunk.at = at;
				chunk.length = BigEndian(png.data() + at);
				memcpy(chunk.type, png.data() + at + 4, 4);
				chunk.type[4] = 0;

				if (!IsLetter((uint8_t)chunk.type[0]) || !IsLetter((uint8_t)chunk.type[1]) || !IsLetter((uint8_t)chunk.type[2]) || !IsLetter((uint8_t)chunk.type[3]))
				{
					Say(why, "the chunk at byte " + std::to_string(at) + " has a type that is not four letters");
					return PngMetaResult::BadChunk;
				}

				if (chunk.length > 0x7fffffffu)
				{
					Say(why, std::string("the ") + chunk.type + " chunk at byte " + std::to_string(at) + " says it is " + std::to_string(chunk.length) + " bytes, past the PNG standard's 2^31 - 1");
					return PngMetaResult::BadChunk;
				}

				if ((size_t)chunk.length > png.size() - at - 12)
				{
					Say(why, std::string("the ") + chunk.type + " chunk at byte " + std::to_string(at) + " says it is " + std::to_string(chunk.length) + " bytes, past the end of the file");
					return PngMetaResult::Truncated;
				}

				uint32_t const stored = BigEndian(png.data() + at + 8 + chunk.length);
				uint32_t const worked = Crc(png.data() + at + 4, 4 + (size_t)chunk.length) ^ 0xffffffffu;

				if (stored != worked)
				{
					Say(why, std::string("the ") + chunk.type + " chunk at byte " + std::to_string(at) + " has a wrong CRC");
					return PngMetaResult::BadCrc;
				}

				if (chunks->empty() && (strcmp(chunk.type, "IHDR") != 0 || chunk.length != 13))
				{
					Say(why, std::string("the first chunk is ") + chunk.type + " of " + std::to_string(chunk.length) + " bytes, not IHDR of 13");
					return PngMetaResult::BadChunk;
				}

				chunks->push_back(chunk);
				at += 12 + (size_t)chunk.length;

				if (strcmp(chunk.type, "IEND") == 0)
				{
					if (chunk.length != 0)
					{
						Say(why, "the IEND chunk is not empty");
						return PngMetaResult::BadChunk;
					}

					if (at != png.size())
					{
						Say(why, std::to_string(png.size() - at) + " bytes follow the IEND chunk");
						return PngMetaResult::BadChunk;
					}

					return PngMetaResult::Done;
				}
			}

			Say(why, "the file ends with no IEND chunk");
			return PngMetaResult::NoEnd;
		}

		// UTF-8 as the standard allows it: no overlong form, no surrogate, nothing past U+10FFFF; and no zero byte
		bool Utf8(std::string const &text)
		{
			size_t i = 0;

			while (i < text.size())
			{
				uint8_t const c = (uint8_t)text[i];

				if (c == 0)
				{
					return false;
				}

				if (c < 0x80)
				{
					i++;
					continue;
				}

				size_t more;
				uint32_t code, least;

				if ((c & 0xe0) == 0xc0)
				{
					more = 1; code = c & 0x1f; least = 0x80;
				}
				else if ((c & 0xf0) == 0xe0)
				{
					more = 2; code = c & 0x0f; least = 0x800;
				}
				else if ((c & 0xf8) == 0xf0)
				{
					more = 3; code = c & 0x07; least = 0x10000;
				}
				else
				{
					return false;
				}

				if (text.size() - i - 1 < more)
				{
					return false;
				}

				for (size_t k = 1; k <= more; k++)
				{
					uint8_t const next = (uint8_t)text[i + k];

					if ((next & 0xc0) != 0x80)
					{
						return false;
					}

					code = (code << 6) | (next & 0x3f);
				}

				if (code < least || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
				{
					return false;
				}

				i += 1 + more;
			}

			return true;
		}

		// ---- One JSON object (RFC 8259's grammar), nested at most kMostJsonDepth deep

		void Space(std::string const &s, size_t *i)
		{
			while (*i < s.size() && (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\n' || s[*i] == '\r'))
			{
				(*i)++;
			}
		}

		bool Hex(char c)
		{
			return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		}

		bool Digit(char c)
		{
			return c >= '0' && c <= '9';
		}

		bool JsonString(std::string const &s, size_t *i)
		{
			if (*i >= s.size() || s[*i] != '"')
			{
				return false;
			}

			(*i)++;

			while (*i < s.size())
			{
				unsigned char const c = (unsigned char)s[*i];

				if (c == '"')
				{
					(*i)++;
					return true;
				}

				if (c < 0x20)
				{
					return false;   // a control character must be escaped
				}

				if (c == '\\')
				{
					(*i)++;

					if (*i >= s.size())
					{
						return false;
					}

					char const e = s[*i];

					if (e == 'u')
					{
						if (s.size() - *i < 5 || !Hex(s[*i + 1]) || !Hex(s[*i + 2]) || !Hex(s[*i + 3]) || !Hex(s[*i + 4]))
						{
							return false;
						}

						*i += 4;
					}
					else if (!strchr("\"\\/bfnrt", e) || e == 0)
					{
						return false;
					}
				}

				(*i)++;
			}

			return false;
		}

		bool JsonNumber(std::string const &s, size_t *i)
		{
			if (*i < s.size() && s[*i] == '-')
			{
				(*i)++;
			}

			if (*i >= s.size() || !Digit(s[*i]))
			{
				return false;
			}

			if (s[*i] == '0')
			{
				(*i)++;
			}
			else
			{
				while (*i < s.size() && Digit(s[*i]))
				{
					(*i)++;
				}
			}

			if (*i < s.size() && s[*i] == '.')
			{
				(*i)++;

				if (*i >= s.size() || !Digit(s[*i]))
				{
					return false;
				}

				while (*i < s.size() && Digit(s[*i]))
				{
					(*i)++;
				}
			}

			if (*i < s.size() && (s[*i] == 'e' || s[*i] == 'E'))
			{
				(*i)++;

				if (*i < s.size() && (s[*i] == '+' || s[*i] == '-'))
				{
					(*i)++;
				}

				if (*i >= s.size() || !Digit(s[*i]))
				{
					return false;
				}

				while (*i < s.size() && Digit(s[*i]))
				{
					(*i)++;
				}
			}

			return true;
		}

		bool JsonWord(std::string const &s, size_t *i, char const *word)
		{
			size_t const n = strlen(word);

			if (s.size() - *i < n || s.compare(*i, n, word) != 0)
			{
				return false;
			}

			*i += n;
			return true;
		}

		bool JsonValue(std::string const &s, size_t *i, int depth);

		bool JsonObject(std::string const &s, size_t *i, int depth)
		{
			if (depth >= kMostJsonDepth || *i >= s.size() || s[*i] != '{')
			{
				return false;
			}

			(*i)++;
			Space(s, i);

			if (*i < s.size() && s[*i] == '}')
			{
				(*i)++;
				return true;
			}

			for (;;)
			{
				Space(s, i);

				if (!JsonString(s, i))
				{
					return false;
				}

				Space(s, i);

				if (*i >= s.size() || s[*i] != ':')
				{
					return false;
				}

				(*i)++;

				if (!JsonValue(s, i, depth + 1))
				{
					return false;
				}

				Space(s, i);

				if (*i < s.size() && s[*i] == ',')
				{
					(*i)++;
					continue;
				}

				if (*i < s.size() && s[*i] == '}')
				{
					(*i)++;
					return true;
				}

				return false;
			}
		}

		bool JsonArray(std::string const &s, size_t *i, int depth)
		{
			if (depth >= kMostJsonDepth || *i >= s.size() || s[*i] != '[')
			{
				return false;
			}

			(*i)++;
			Space(s, i);

			if (*i < s.size() && s[*i] == ']')
			{
				(*i)++;
				return true;
			}

			for (;;)
			{
				if (!JsonValue(s, i, depth + 1))
				{
					return false;
				}

				Space(s, i);

				if (*i < s.size() && s[*i] == ',')
				{
					(*i)++;
					continue;
				}

				if (*i < s.size() && s[*i] == ']')
				{
					(*i)++;
					return true;
				}

				return false;
			}
		}

		bool JsonValue(std::string const &s, size_t *i, int depth)
		{
			Space(s, i);

			if (*i >= s.size())
			{
				return false;
			}

			char const c = s[*i];

			if (c == '{')
			{
				return JsonObject(s, i, depth);
			}

			if (c == '[')
			{
				return JsonArray(s, i, depth);
			}

			if (c == '"')
			{
				return JsonString(s, i);
			}

			if (c == 't')
			{
				return JsonWord(s, i, "true");
			}

			if (c == 'f')
			{
				return JsonWord(s, i, "false");
			}

			if (c == 'n')
			{
				return JsonWord(s, i, "null");
			}

			return JsonNumber(s, i);
		}

		// The text as png_meta.h asks: within the limit, UTF-8 with no zero byte, one JSON object and nothing after it but spaces
		PngMetaResult TextIsOurs(std::string const &json, std::string *why)
		{
			if (json.size() > kPngMetaMostTextBytes)
			{
				Say(why, "the text is " + std::to_string(json.size()) + " bytes, over the " + std::to_string(kPngMetaMostTextBytes) + " allowed");
				return PngMetaResult::TooLarge;
			}

			if (!Utf8(json))
			{
				Say(why, "the text is not UTF-8 (or holds a zero byte)");
				return PngMetaResult::BadText;
			}

			size_t i = 0;

			Space(json, &i);

			if (!JsonObject(json, &i, 0))
			{
				Say(why, "the text is not one well-formed JSON object (at byte " + std::to_string(i) + ")");
				return PngMetaResult::BadText;
			}

			Space(json, &i);

			if (i != json.size())
			{
				Say(why, "the text has more after its JSON object (at byte " + std::to_string(i) + ")");
				return PngMetaResult::BadText;
			}

			return PngMetaResult::Done;
		}

		// A plain file read whole, at most kPngMetaMostFileBytes
		PngMetaResult ReadWhole(char const *path, std::vector<uint8_t> *bytes, std::string *why)
		{
			bytes->clear();

			DWORD const kind = path ? GetFileAttributesA(path) : INVALID_FILE_ATTRIBUTES;

			if (kind == INVALID_FILE_ATTRIBUTES || (kind & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)))
			{
				Say(why, std::string("not a plain file: ") + (path ? path : "(none)"));
				return PngMetaResult::NotPng;
			}

			HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				Say(why, std::string("the file could not be opened: ") + path);
				return PngMetaResult::NotPng;
			}

			LARGE_INTEGER size;
			PngMetaResult result = PngMetaResult::Done;

			if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || (unsigned long long)size.QuadPart > kPngMetaMostFileBytes)
			{
				Say(why, std::string("the file is over ") + std::to_string(kPngMetaMostFileBytes) + " bytes (or its size can't be read): " + path);
				result = PngMetaResult::TooLarge;
			}
			else
			{
				bytes->resize((size_t)size.QuadPart);

				DWORD got = 0;

				if (!bytes->empty() && (!ReadFile(file, bytes->data(), (DWORD)bytes->size(), &got, nullptr) || got != bytes->size()))
				{
					Say(why, std::string("the file could not be read whole: ") + path);
					bytes->clear();
					result = PngMetaResult::Truncated;
				}
			}

			CloseHandle(file);
			return result;
		}
	}

	char const *PngMetaWords(PngMetaResult result)
	{
		switch (result)
		{
		case PngMetaResult::Done: return "done";
		case PngMetaResult::NotPng: return "not a PNG file";
		case PngMetaResult::BadChunk: return "a chunk of the PNG is malformed";
		case PngMetaResult::BadCrc: return "a chunk's CRC is wrong";
		case PngMetaResult::Truncated: return "the PNG is cut short";
		case PngMetaResult::NoEnd: return "the PNG has no IEND";
		case PngMetaResult::Duplicate: return "the PNG holds more than one context chunk";
		case PngMetaResult::TooLarge: return "too large";
		case PngMetaResult::NotFound: return "the PNG holds no context chunk";
		case PngMetaResult::BadText: return "the context chunk is not well-formed";
		}

		return "refused";
	}

	PngMetaResult PngMeta_Read(std::vector<uint8_t> const &png, std::string *json, std::string *why)
	{
		std::string ignored;

		json = json ? json : &ignored;
		json->clear();

		std::vector<Chunk> chunks;
		PngMetaResult const walked = Walk(png, &chunks, why);

		if (walked != PngMetaResult::Done)
		{
			return walked;
		}

		Chunk const *ours = nullptr;

		for (Chunk const &chunk : chunks)
		{
			if (!IsOurs(png, chunk))
			{
				continue;
			}

			if (ours)
			{
				Say(why, "a second context chunk at byte " + std::to_string(chunk.at) + " (the first at " + std::to_string(ours->at) + ")");
				return PngMetaResult::Duplicate;
			}

			ours = &chunk;
		}

		if (!ours)
		{
			Say(why, "the PNG holds no iTXt chunk with the keyword AVP_OpenInteng");
			return PngMetaResult::NotFound;
		}

		// the keyword and its zero (IsOurs), the two flags, an empty language tag and an empty translated keyword: five bytes after the keyword's zero
		size_t const data = ours->at + 8, end = data + ours->length;
		size_t const after = data + strlen(kPngMetaKeyword) + 1;

		if (end - after < 4)
		{
			Say(why, "the context chunk is too short for its flags");
			return PngMetaResult::BadText;
		}

		if (png[after] != 0 || png[after + 1] != 0)
		{
			Say(why, "the context chunk is compressed (flag " + std::to_string(png[after]) + ", method " + std::to_string(png[after + 1]) + "): ours never is");
			return PngMetaResult::BadText;
		}

		if (png[after + 2] != 0 || png[after + 3] != 0)
		{
			Say(why, "the context chunk has a language tag or a translated keyword: ours has neither");
			return PngMetaResult::BadText;
		}

		std::string const text((char const *)png.data() + after + 4, end - (after + 4));
		PngMetaResult const shaped = TextIsOurs(text, why);

		if (shaped != PngMetaResult::Done)
		{
			return shaped;
		}

		*json = text;
		Say(why, "");
		return PngMetaResult::Done;
	}

	PngMetaResult PngMeta_Write(std::vector<uint8_t> const &png, std::string const &json, std::vector<uint8_t> *out, std::string *why)
	{
		std::vector<Chunk> chunks;
		PngMetaResult const walked = Walk(png, &chunks, why);

		if (walked != PngMetaResult::Done)
		{
			return walked;
		}

		for (Chunk const &chunk : chunks)
		{
			if (IsOurs(png, chunk))
			{
				Say(why, "the PNG holds a context chunk already, at byte " + std::to_string(chunk.at));
				return PngMetaResult::Duplicate;
			}
		}

		PngMetaResult const shaped = TextIsOurs(json, why);

		if (shaped != PngMetaResult::Done)
		{
			return shaped;
		}

		// the chunk: its length, "iTXt", the keyword and its zero, flag 0, method 0, an empty language tag, an empty translated keyword, the text; then the CRC of the type and data
		std::vector<uint8_t> body;

		body.insert(body.end(), { 'i', 'T', 'X', 't' });
		body.insert(body.end(), kPngMetaKeyword, kPngMetaKeyword + strlen(kPngMetaKeyword));
		body.insert(body.end(), { 0, 0, 0, 0, 0 });
		body.insert(body.end(), json.begin(), json.end());

		std::vector<uint8_t> chunk;

		PutBigEndian(&chunk, (uint32_t)(body.size() - 4));
		chunk.insert(chunk.end(), body.begin(), body.end());
		PutBigEndian(&chunk, Crc(body.data(), body.size()) ^ 0xffffffffu);

		size_t const end = chunks.back().at;   // IEND's start (Walk ends on it)

		out->assign(png.begin(), png.begin() + (std::ptrdiff_t)end);
		out->insert(out->end(), chunk.begin(), chunk.end());
		out->insert(out->end(), png.begin() + (std::ptrdiff_t)end, png.end());
		Say(why, "");
		return PngMetaResult::Done;
	}

	PngMetaResult PngMeta_ReadFile(char const *path, std::string *json, std::string *why)
	{
		std::vector<uint8_t> bytes;
		PngMetaResult const read = ReadWhole(path, &bytes, why);

		if (read != PngMetaResult::Done)
		{
			if (json)
			{
				json->clear();
			}

			return read;
		}

		return PngMeta_Read(bytes, json, why);
	}

	PngMetaResult PngMeta_AddToFile(char const *path, std::string const &json, std::string *why)
	{
		std::vector<uint8_t> bytes, made;
		PngMetaResult result = ReadWhole(path, &bytes, why);

		if (result != PngMetaResult::Done)
		{
			return result;
		}

		result = PngMeta_Write(bytes, json, &made, why);

		if (result != PngMetaResult::Done)
		{
			return result;
		}

		// a temporary file in the same folder, read back, then put in place: on any failure the picture is as it was
		std::string const temporary = std::string(path) + ".meta";

		DeleteFileA(temporary.c_str());

		HANDLE const file = CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		DWORD wrote = 0;
		bool const written = file != INVALID_HANDLE_VALUE && WriteFile(file, made.data(), (DWORD)made.size(), &wrote, nullptr) && wrote == made.size();

		if (file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
		}

		std::string back;

		if (!written || PngMeta_ReadFile(temporary.c_str(), &back, nullptr) != PngMetaResult::Done || back != json)
		{
			DeleteFileA(temporary.c_str());
			Say(why, "the picture with its context could not be written and read back: " + temporary);
			return PngMetaResult::Truncated;
		}

		if (!MoveFileExA(temporary.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			DeleteFileA(temporary.c_str());
			Say(why, std::string("the picture could not be replaced (it is as it was): ") + path);
			return PngMetaResult::Truncated;
		}

		Say(why, "");
		return PngMetaResult::Done;
	}
}
