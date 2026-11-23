// The test for the plot budget's setting read as text (src/host/autoplot_budget.h, AutoPlotBudget_FromText; Drake's review of 6774ce25, MUST 1). Written 2026-11-23 by
// Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//   NONE OR 0: the budget as it is (4000000), no words. A PLAIN NUMBER: that number, spaces round it let go. PAST THE MOST: held at 16000000, said. ANYTHING ELSE
//   (Drake's measured cases: 4294967297 wrapped to 1, 12abc read as 12, 8,000,000 as 8, -1 wrapped; and pt-BR's 8.000.000): the budget as it is, and words that
//   name the setting and say how it is written.
// The program gives back a non-zero result if any check failed.

#include <stdio.h>

#include <string>

#include "autoplot_budget.h"
#include "autoplot_command.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// one setting's text read: its budget and its words (called first, then checked: the message is made after the call)
static void Expect(char const *text, size_t wanted, bool saysWords, char const *what)
{
	std::string words;
	size_t const budget = host::AutoPlotBudget_FromText(text, &words);

	Check(budget == wanted && words.empty() == !saysWords, std::string(what) + ": \"" + text + "\" gives " + std::to_string((unsigned long long)budget) + " (" +
		std::to_string((unsigned long long)wanted) + " wanted), " + (words.empty() ? "no words" : "\"" + words + "\""));
}

int main()
{
	puts("the plot budget's setting read as text");

	size_t const standing = host::kAutoPlotMostModuleLooks, most = host::kAutoPlotMostModuleLooksSet;

	Expect("", standing, false, "none");
	Expect("0", standing, false, "0");
	Expect("8000000", 8000000, false, "a plain number");
	Expect("  8000000\t", 8000000, false, "spaces round it let go");
	Expect("1", 1, false, "the least");
	Expect("16000000", most, false, "the most");
	Expect("16000001", most, true, "one past the most: held there, said");
	Expect("99999999999", most, true, "eleven digits: held at the most, said");
	Expect("4294967297", most, true, "past 32 bits (the old read wrapped it to 1): held at the most, said");
	Expect("12abc", standing, true, "letters after it (the old read took 12): refused, the budget as it is, said");
	Expect("8,000,000", standing, true, "commas between thousands, as the editor's words write it (the old read took 8): refused, said");
	Expect("8.000.000", standing, true, "dots between thousands, as pt-BR writes it: refused, said");
	Expect("-1", standing, true, "a sign (the old read wrapped it to the most): refused, said");
	Expect("8 000 000", standing, true, "spaces inside: refused, said");

	std::string words;

	host::AutoPlotBudget_FromText("8,000,000", &words);
	Check(words.find("AutoPlotMostModuleLooks=") != std::string::npos && words.find("[Editor]") != std::string::npos && words.find("1 to 16000000") != std::string::npos &&
		words.find("no commas") != std::string::npos, "the refusal names the setting, its section and how it is written (\"" + words + "\")");

	printf(checksFailed ? "SOME CHECKS FAILED\n" : "All checks passed\n");
	return checksFailed ? 1 : 0;
}
