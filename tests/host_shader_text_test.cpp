// host_shader_text_test.cpp: the renderer's shaders built into the editor (src/host/shader_text.h, the owner's "baked in"): the generated table (shader_text_data.cpp, by
// tools/make-shader-text.js) holds EVERY .vsh / .psh / .fxh of the pinned avpx/shaders, each byte for byte as its file (a CRLF read as LF), and nothing else, so an avpx pin
// that moves its shaders fails here until the table is made again; the source the renderer gets finds a name without case, refuses one it lacks, and counts both.
// Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "shader_text.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

// The tree's root from this file's own path (the build gives __FILE__ whole: /FC)
static std::string TreeRoot()
{
	std::string here = __FILE__;
	size_t const tests = here.find_last_of("\\/");
	size_t const root = tests == std::string::npos ? std::string::npos : here.find_last_of("\\/", tests - 1);

	return root == std::string::npos ? std::string(".") : here.substr(0, root);
}

static bool ReadWhole(std::string const &path, std::string *out)
{
	FILE *f = nullptr;

	if (fopen_s(&f, path.c_str(), "rb") != 0 || !f)
	{
		return false;
	}

	char block[4096];
	size_t got;

	out->clear();

	while ((got = fread(block, 1, sizeof(block), f)) > 0 && out->size() < 4u * 1024u * 1024u)
	{
		out->append(block, got);
	}

	fclose(f);

	// a CRLF read as LF, as the generator reads it
	std::string lf;

	for (size_t i = 0; i < out->size(); i++)
	{
		if (!((*out)[i] == '\r' && i + 1 < out->size() && (*out)[i + 1] == '\n'))
		{
			lf += (*out)[i];
		}
	}

	*out = lf;
	return true;
}

int main()
{
	std::string const folder = TreeRoot() + "\\avpx\\shaders\\";
	std::set<std::string> onDisk;
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "*").c_str(), &found);

	if (search != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string const name = found.cFileName;
			size_t const dot = name.rfind('.');
			std::string const kind = dot == std::string::npos ? std::string() : name.substr(dot);

			if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (kind == ".vsh" || kind == ".psh" || kind == ".fxh"))
			{
				onDisk.insert(name);
			}
		} while (FindNextFileA(search, &found));

		FindClose(search);
	}

	Check(!onDisk.empty(), "the pinned avpx's shaders are there to compare (" + std::to_string(onDisk.size()) + " in " + folder + ")");
	Check(host::kShaderTextCount == onDisk.size(), "the table holds as many as the folder (" + std::to_string(host::kShaderTextCount) + " and " + std::to_string(onDisk.size()) + ")");

	size_t same = 0;

	for (std::string const &name : onDisk)
	{
		std::string file;
		char const *text = nullptr;
		size_t length = 0;
		bool const read = ReadWhole(folder + name, &file);
		bool const inTable = host::ShaderText_Find(name.c_str(), &text, &length);
		bool const equal = read && inTable && length == file.size() && memcmp(text, file.data(), length) == 0;

		same += equal ? 1 : 0;

		if (!equal)
		{
			Check(false, name + ": " + (!read ? "not read" : (!inTable ? "NOT in the table" : "the table's text differs from the file's (run node tools/make-shader-text.js)")));
		}
	}

	Check(same == onDisk.size(), std::to_string(same) + " of " + std::to_string(onDisk.size()) + " shaders in the table byte for byte as their files");

	for (size_t i = 0; i < host::kShaderTextCount; i++)
	{
		if (onDisk.find(host::kShaderTexts[i].name) == onDisk.end())
		{
			Check(false, std::string(host::kShaderTexts[i].name) + " is in the table but not in avpx/shaders");
		}
	}

	// the source: without case, as Windows finds a file; a name it lacks is refused and counted; the renderer's own shaders all there
	char const *text = nullptr;
	size_t length = 0;
	size_t const missingBefore = host::ShaderText_Missing();

	Check(host::ShaderText_Find("SHADOWBLUR.VSH", &text, &length) && length > 0, "a name found without case (SHADOWBLUR.VSH)");
	Check(!host::ShaderText_Find("noSuchShader.psh", &text, &length), "a name it lacks is refused");
	Check(!host::ShaderText_Find("../shaders/main.vsh", &text, &length), "a path is not a name: refused");
	Check(!host::ShaderText_Find(nullptr, &text, &length), "no name: refused");
	Check(host::ShaderText_Missing() == missingBefore + 3, "every refusal counted");
	Check(host::ShaderText_Find("lightShapes.fxh", &text, &length) && host::ShaderText_Find("reliefMarch.fxh", &text, &length), "the two .fxh the pixel shaders #include are served");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
