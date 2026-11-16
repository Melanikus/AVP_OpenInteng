// The test that no two typed-answer prompts share a number (the kPrompt... constants of src/host): two devs claiming the same "next free" number
// compiled without a word and opened each other's prompt. Reads every file of src/host, so a prompt declared in any of them is seen. It also says the next free number.
// Written 2026-11-15 by Melanikus (Apone) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// Run from the repository's root (the test runner does).

#include <stdio.h>
#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	std::filesystem::path const folder = "src/host";
	std::map<int, std::string> byNumber;
	std::map<std::string, std::string> byName;
	std::regex const declaration("int32_t\\s+const\\s+(kPrompt[A-Za-z0-9_]*)\\s*=\\s*(-?[0-9]+)\\s*;");
	int seen = 0;
	int highest = 0;

	if (!std::filesystem::is_directory(folder))
	{
		printf("  FAILED  no src/host folder: run this from the repository's root\n");
		return 1;
	}

	for (auto const &entry : std::filesystem::directory_iterator(folder))
	{
		std::string const ext = entry.path().extension().string();

		if (ext != ".h" && ext != ".cpp")
		{
			continue;
		}

		std::ifstream in(entry.path());
		std::string line;

		while (std::getline(in, line))
		{
			std::smatch found;

			if (!std::regex_search(line, found, declaration))
			{
				continue;
			}

			std::string const name = found[1];
			int const number = atoi(found[2].str().c_str());
			std::string const where = entry.path().filename().string() + ": " + name;

			seen++;
			highest = number > highest ? number : highest;

			// kPromptNone is 0 and is the only one allowed to be 0
			if (number == 0 && name != "kPromptNone")
			{
				Check(false, where + " is 0, which means no prompt");
			}

			if (byName.count(name))
			{
				Check(false, where + " is declared twice (also " + byName[name] + ")");
			}

			byName[name] = where;

			if (byNumber.count(number))
			{
				Check(false, "prompt number " + std::to_string(number) + " is used twice: " + byNumber[number] + " and " + where);
			}
			else
			{
				byNumber[number] = where;
			}
		}
	}

	// A BARE prompt number in the code or in a scenario is how a renumbering broke Turn End (cut 10: a table held 41, which had become Soft Edge's): the code names a prompt by its kPrompt constant,
	// and a scenario's expect-prompt holds 0 or a number some kPrompt constant has
	{
		std::regex const bare("(OpenAPrompt|FormOpen)\\(\\s*[0-9]|promptFor\\s*=\\s*[0-9]|kPrompt[A-Za-z]*\\[\\]\\s*=\\s*\\{[^}]*[^A-Za-z_0-9][1-9][0-9]*[,} ]");
		int barePlaces = 0;

		for (auto const &entry : std::filesystem::directory_iterator(folder))
		{
			if (entry.path().extension() != ".cpp" && entry.path().extension() != ".h")
			{
				continue;
			}

			std::ifstream in(entry.path());
			std::string line;

			while (std::getline(in, line))
			{
				if (std::regex_search(line, bare))
				{
					Check(false, "a bare prompt number in " + entry.path().filename().string() + ": " + line.substr(0, 120));
					barePlaces++;
				}
			}
		}

		Check(barePlaces == 0, "the code names every prompt by its kPrompt constant (no bare number)");

		std::set<int> known = { 0 };

		for (auto const &n : byNumber)
		{
			known.insert(n.first);
		}

		int badExpect = 0;

		if (std::filesystem::is_directory("tests/scenarios"))
		{
			for (auto const &entry : std::filesystem::directory_iterator("tests/scenarios"))
			{
				if (entry.path().extension() != ".txt")
				{
					continue;
				}

				std::ifstream in(entry.path());
				std::string line;

				while (std::getline(in, line))
				{
					if (line.compare(0, 14, "expect-prompt ") == 0 && !known.count(atoi(line.c_str() + 14)))
					{
						Check(false, entry.path().filename().string() + ": " + line + " is no prompt number any kPrompt constant has");
						badExpect++;
					}
				}
			}
		}

		Check(badExpect == 0, "every scenario's expect-prompt is 0 or a number a kPrompt constant has");
	}

	Check(seen > 20, "the kPrompt constants were found (" + std::to_string(seen) + ")");
	Check(checksFailed == 0, "every prompt number is used once; the highest is " + std::to_string(highest) + ", so the next free number is " + std::to_string(highest + 1));
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
