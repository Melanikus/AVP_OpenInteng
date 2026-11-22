// The test for the auto-plot's start guard (src/host/autoplot_command.cpp, Begin): A RUN GOING IS NEVER STARTED AGAIN (Bishop 2, 11-22, from Vasquez's review of
// alpha.5 item 1, AvP-docs\review\drake-item1-plotstart-review.md). Begin makes the run's state new (its StartingRun, its jobs cleared, its stage set); called while a run
// goes, it would clear the jobs the worker thread may be reading, a crash. Its callers are the Run (which checks the stage itself) and the replace question's Yes (which
// relied on the question being modal), so the guard is Begin's own: its FIRST statement, before any of its state is touched, refuses unless the stage is Idle, says so,
// and returns.
// HOW: Begin is in autoplot_command.cpp's own unnamed namespace, and that file needs the game's level, the grid and the worker, which no unit test links. So, as
// editor_start_test checks the editor's start, this reads the source: Begin's body, its first statement, and the order of what follows. It proves the guard is there
// and first; the scenarios prove a run still starts from Idle (acceptance-autoplot-start-message). Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>

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

static std::string FileText(char const *path)
{
	std::string text;
	FILE *file = nullptr;

	if (fopen_s(&file, path, "rb") != 0 || !file)
	{
		return text;
	}

	char block[4096];
	size_t got = 0;

	while ((got = fread(block, 1, sizeof(block), file)) > 0 && text.size() < (8u << 20))
	{
		text.append(block, got);
	}

	fclose(file);
	return text;
}

// The text from `from` to the next line that is exactly "\t}" (the end of a function one tab in), or "" when either is missing
static std::string Body(std::string const &text, std::string const &from)
{
	size_t const start = text.find(from);

	if (start == std::string::npos)
	{
		return std::string();
	}

	size_t const end = text.find("\n\t}\n", start);

	return end == std::string::npos ? std::string() : text.substr(start, end - start);
}

int main()
{
	std::string const source = FileText("src/host/autoplot_command.cpp");

	Check(!source.empty(), "src/host/autoplot_command.cpp is read (" + std::to_string(source.size()) + " bytes)");

	std::string const begin = Body(source, "\tvoid Begin(OpenLevelFile *open, std::vector<int32_t> const &heads, std::map<int32_t, std::vector<int32_t> > const &groups)\n\t{\n");

	Check(!begin.empty(), "Begin is found, with its body");

	// its first statement, after the opening brace and the comment lines over it: the guard
	size_t const open = begin.find("\n\t{\n");
	size_t firstAt = open == std::string::npos ? std::string::npos : begin.find_first_not_of(" \t\n", open + 4);

	while (firstAt != std::string::npos && begin.compare(firstAt, 2, "//") == 0)
	{
		size_t const lineEnd = begin.find('\n', firstAt);

		firstAt = lineEnd == std::string::npos ? std::string::npos : begin.find_first_not_of(" \t\n", lineEnd);
	}

	std::string const first = firstAt == std::string::npos ? std::string() : begin.substr(firstAt, begin.find('\n', firstAt) - firstAt);

	Check(first == "if (stage != Stage::Idle)", "Begin's first statement is the guard, \"if (stage != Stage::Idle)\" (it is \"" + first + "\")");

	// the guard's block: it says so and returns, and nothing of the run's state is touched before that return
	size_t const guardEnd = firstAt == std::string::npos ? std::string::npos : begin.find("\n\t\t}\n", firstAt);
	std::string const guard = guardEnd == std::string::npos ? std::string() : begin.substr(firstAt, guardEnd - firstAt);

	Check(guard.find("Say(") != std::string::npos && guard.find("return;") != std::string::npos, "the guard says why on the status line and returns");
	Check(guard.find("starting") == std::string::npos && guard.find("jobs") == std::string::npos && guard.find("stage =") == std::string::npos,
		"and touches nothing of the run: not its StartingRun, not its jobs, not its stage");

	// what Begin does to the run's state comes after the guard
	char const *const resets[] = { "starting = StartingRun();", "jobs.clear();", "stage = Stage::Starting;" };
	bool after = guardEnd != std::string::npos;

	for (char const *reset : resets)
	{
		size_t const at = begin.find(reset);

		after = after && at != std::string::npos && at > guardEnd;
	}

	Check(after, "a new StartingRun, the jobs cleared and the stage set come after the guard, never before it");

	printf("%s: %d failed\n", checksFailed ? "FAILED" : "ok", checksFailed);
	return checksFailed ? 1 : 0;
}
