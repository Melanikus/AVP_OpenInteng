// See state_line.h. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "state_line.h"

#include <map>

static std::map<std::string, std::string> fields;   // this frame's, by name (so the line's order is the names' order)

void StateLine_Add(char const *name, std::string const &value)
{
	if (!name || !*name)
	{
		return;
	}

	std::string safe;

	safe.reserve(value.size());

	for (char const letter : value)
	{
		safe += (letter == '"' || letter == '\n' || letter == '\r') ? '?' : letter;
	}

	fields[name] = safe;
}

void StateLine_NewFrame()
{
	fields.clear();
}

std::string StateLine_Words()
{
	std::string words;

	for (auto const &field : fields)
	{
		words += "; " + field.first + " \"" + field.second + "\"";
	}

	return words;
}
