// verify_shot: the check of a screenshot the editor took with Print Screen or Shift+F12 (GitHub #78), for an acceptance. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_shot.exe <the shot .png, or a name with a * in it: the newest file that matches> [<check> ...]
//   is <field> <words>        the JSON's text field is exactly those words (a _ in them is a space): "screen" "Module_Linking" -> "screen":"Module Linking"
//   number <field> <n>        the JSON's number field is n
//   in <field> <words>        the JSON's list field holds those words (a _ is a space)
//   pos <x> <y> <z>           "pos" is exactly {"x":x,"y":y,"z":z}
//   no <text>                 the JSON holds nowhere that text (a _ is a space): a privacy check
//   same-box <png> <x> <y> <w> <h>   in that box of the window, the shot's pixels are the other picture's (the harness's picture of a later frame): at most 0.1% of them differ by more than 8 in a
//                             channel. The shot shows what that picture shows there (an open drop-down, a pop-up, a dialog).
// CHECKS FIRST: the PNG's context chunk is read by png_meta's own reader as hostile input (PngMeta_ReadFile), and it is one JSON object of format 1 that says what it took out. Gives back 0 only
// if every check passed. The pictures are decoded by Windows (WIC).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>
#include <wincodec.h>

#include "png_meta.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string Spaced(char const *words)
{
	std::string out = words;

	for (char &c : out)
	{
		c = c == '_' ? ' ' : c;
	}

	return out;
}

// a JSON string as Shot_JsonString writes one (plain ASCII; " and \ escaped)
static std::string Quoted(std::string const &text)
{
	std::string out = "\"";

	for (char const c : text)
	{
		if (c == '"' || c == '\\')
		{
			out += '\\';
		}

		out += c;
	}

	return out + "\"";
}

// The list field's text, from its [ to its ] (names never hold a ] unescaped... a name can: so the list is walked string by string)
static bool ListHolds(std::string const &json, std::string const &field, std::string const &wanted)
{
	size_t at = json.find("\"" + field + "\":[");

	if (at == std::string::npos)
	{
		return false;
	}

	at += field.size() + 4;

	while (at < json.size() && json[at] != ']')
	{
		if (json[at] != '"')
		{
			at++;
			continue;
		}

		size_t end = at + 1;

		while (end < json.size() && json[end] != '"')
		{
			end += json[end] == '\\' ? 2 : 1;
		}

		if (json.compare(at, end + 1 - at, Quoted(wanted)) == 0)
		{
			return true;
		}

		at = end + 1;
	}

	return false;
}

// The picture as 32-bit BGRA rows, decoded by WIC
static bool Decode(IWICImagingFactory *factory, char const *path, std::vector<uint8_t> *pixels, UINT *width, UINT *height)
{
	wchar_t wide[MAX_PATH];
	IWICBitmapDecoder *decoder = nullptr;
	IWICBitmapFrameDecode *frame = nullptr;
	IWICFormatConverter *converter = nullptr;
	bool good = MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH) > 0
		&& SUCCEEDED(factory->CreateDecoderFromFilename(wide, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))
		&& SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateFormatConverter(&converter))
		&& SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))
		&& SUCCEEDED(converter->GetSize(width, height)) && *width > 0 && *height > 0 && *width <= 16384 && *height <= 16384;

	if (good)
	{
		pixels->resize((size_t)*width * *height * 4);
		good = SUCCEEDED(converter->CopyPixels(NULL, *width * 4, (UINT)pixels->size(), pixels->data()));
	}

	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	return good;
}

// One of a box's numbers from the command line: 0 to 16384 (a window's side), anything else -1, so nothing taken from the words can make the box's sums overflow
static int BoxNumber(char const *words)
{
	int number = 0;

	if (!words || !words[0])
	{
		return -1;
	}

	for (char const *c = words; *c; c++)
	{
		if (*c < '0' || *c > '9' || number > 16384)
		{
			return -1;
		}

		number = number * 10 + (*c - '0');
	}

	return number <= 16384 ? number : -1;
}

// A shot's name holds the time it was taken (<map>-<YYYYMMDD>-<HHMMSS>.png), so a test names it with a * (Invasion-*.png): the newest file that matches, by the time it was written
static std::string NewestMatching(std::string const &pattern)
{
	if (pattern.find('*') == std::string::npos)
	{
		return pattern;
	}

	size_t const slash = pattern.find_last_of("\\/");
	std::string const folder = slash == std::string::npos ? std::string() : pattern.substr(0, slash + 1);
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA(pattern.c_str(), &found);
	std::string newest;
	FILETIME newestTime = {};

	if (search == INVALID_HANDLE_VALUE)
	{
		return pattern;
	}

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && (newest.empty() || CompareFileTime(&found.ftLastWriteTime, &newestTime) > 0))
		{
			newest = folder + found.cFileName;
			newestTime = found.ftLastWriteTime;
		}
	} while (FindNextFileA(search, &found));

	FindClose(search);
	return newest.empty() ? pattern : newest;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("verify_shot: <shot.png, or a name with * for the newest that matches> [is <field> <words> | number <field> <n> | in <field> <words> | pos <x> <y> <z> | no <text> | same-box <png> <x> <y> <w> <h>] ...\n");
		return 2;
	}

	std::string const shot = NewestMatching(argv[1]);

	argv[1] = (char *)shot.c_str();

	std::string json, why;
	host::PngMetaResult const read = host::PngMeta_ReadFile(argv[1], &json, &why);

	Check(read == host::PngMetaResult::Done, std::string("the shot holds its context, read as hostile input: ") + argv[1] + (read == host::PngMetaResult::Done ? "" : " (" + why + ")"));
	Check(json.compare(0, 11, "{\"format\":1") == 0, "the context is format 1");
	Check(json.find("\"privacy\":\"paths and user names removed\"") != std::string::npos, "the context says what it took out");
	printf("  --  %s\n", json.substr(0, 1500).c_str());

	IWICImagingFactory *factory = nullptr;
	bool const com = SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED));

	for (int a = 2; a < argc; a++)
	{
		std::string const check = argv[a];

		if ((check == "is" || check == "number" || check == "in") && a + 2 < argc)
		{
			std::string const field = argv[a + 1];
			std::string const words = Spaced(argv[a + 2]);

			if (check == "is")
			{
				Check(json.find("\"" + field + "\":" + Quoted(words)) != std::string::npos, field + " is \"" + words + "\"");
			}
			else if (check == "number")
			{
				Check(json.find("\"" + field + "\":" + words + ",") != std::string::npos || json.find("\"" + field + "\":" + words + "}") != std::string::npos, field + " is " + words);
			}
			else
			{
				Check(ListHolds(json, field, words), field + " holds \"" + words + "\"");
			}

			a += 2;
		}
		else if (check == "pos" && a + 3 < argc)
		{
			std::string const wanted = std::string("\"pos\":{\"x\":") + argv[a + 1] + ",\"y\":" + argv[a + 2] + ",\"z\":" + argv[a + 3] + "}";

			Check(json.find(wanted) != std::string::npos, "the camera's place: " + wanted);
			a += 3;
		}
		else if (check == "no" && a + 1 < argc)
		{
			std::string const text = Spaced(argv[a + 1]);

			Check(json.find(text) == std::string::npos, "privacy: \"" + text + "\" is nowhere in the context");
			a += 1;
		}
		else if (check == "same-box" && a + 5 < argc)
		{
			int const x = BoxNumber(argv[a + 2]), y = BoxNumber(argv[a + 3]), w = BoxNumber(argv[a + 4]), h = BoxNumber(argv[a + 5]);
			std::vector<uint8_t> shot, other;
			UINT sw = 0, sh = 0, ow = 0, oh = 0;

			if (!factory && com)
			{
				CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
			}

			bool const decoded = factory && Decode(factory, argv[1], &shot, &sw, &sh) && Decode(factory, argv[a + 1], &other, &ow, &oh);
			bool const fits = decoded && sw == ow && sh == oh && x >= 0 && y >= 0 && w > 0 && h > 0 && (UINT)(x + w) <= sw && (UINT)(y + h) <= sh;
			size_t differ = 0;

			for (int row = 0; fits && row < h; row++)
			{
				for (int column = 0; column < w; column++)
				{
					size_t const at = ((size_t)(y + row) * sw + (size_t)(x + column)) * 4;

					for (int channel = 0; channel < 3; channel++)
					{
						if (abs((int)shot[at + channel] - (int)other[at + channel]) > 8)
						{
							differ++;
							break;
						}
					}
				}
			}

			Check(fits && differ * 1000 <= (size_t)w * h, "the box " + std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(w) + "x" + std::to_string(h) + " is the same in the shot and " +
			      argv[a + 1] + (fits ? " (" + std::to_string((unsigned long long)differ) + " pixels differ)" : decoded ? " (the sizes or the box don't fit: " + std::to_string(sw) + "x" + std::to_string(sh) +
			      " and " + std::to_string(ow) + "x" + std::to_string(oh) + ")" : " (a picture could not be decoded)"));
			a += 5;
		}
		else
		{
			Check(false, "a check this tool doesn't know, or one missing its words: " + check);
		}
	}

	if (factory)
	{
		factory->Release();
	}

	if (checksFailed)
	{
		printf("FAILED verify_shot (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed verify_shot\n");
	return 0;
}
