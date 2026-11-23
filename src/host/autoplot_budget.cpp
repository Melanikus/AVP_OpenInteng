// autoplot_budget.cpp: THE PLOT BUDGET'S SETTING, READ AS TEXT (autoplot_budget.h says what and why). Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_budget.h"

#include "autoplot_command.h"   // kAutoPlotMostModuleLooks, kAutoPlotMostModuleLooksSet

size_t host::AutoPlotBudget_FromText(std::string const &text, std::string *words)
{
	std::string notSaid;

	words = words ? words : &notSaid;
	words->clear();

	// spaces and tabs round it let go
	size_t first = 0, last = text.size();

	while (first < last && (text[first] == ' ' || text[first] == '\t'))
	{
		first++;
	}

	while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t'))
	{
		last--;
	}

	std::string const given = text.substr(first, last - first);

	// plain digits only: a sign, a comma or a dot between thousands (8,000,000; pt-BR's 8.000.000), a space inside or a letter is refused WHOLE (the old read kept a
	// number's leading digits, so 8,000,000 was 8: Drake's review of 6774ce25, MUST 1)
	bool digits = true;

	for (char const c : given)
	{
		digits = digits && c >= '0' && c <= '9';
	}

	if (!digits)
	{
		// the value as written, at most 24 letters, in SINGLE quotes: a double quote would end the status field of the harness's state line, which prints it in
		// double quotes (measured on 76daa479: the field read only "... is "); any letter that isn't plain, or a quote, is shown as '?'
		std::string shown;

		for (size_t i = 0; i < given.size() && i < 24; i++)
		{
			shown += given[i] >= ' ' && given[i] <= '~' && given[i] != '"' && given[i] != '\'' ? given[i] : '?';
		}

		*words = "AutoPlotMostModuleLooks= under [Editor] in AVP_OpenInteng.ini is '" + shown + (given.size() > 24 ? "...'" : "'") +
			", not a plain number from 1 to " + std::to_string((unsigned long long)kAutoPlotMostModuleLooksSet) + " (no commas, dots, signs or spaces): the plot's budget stays " +
			std::to_string((unsigned long long)kAutoPlotMostModuleLooks);
		return kAutoPlotMostModuleLooks;
	}

	// its value, its leading zeros let go; more than 8 digits is past the most whatever they are (so the number is never made past what it can hold)
	size_t lead = 0;

	while (lead < given.size() && given[lead] == '0')
	{
		lead++;
	}

	std::string const number = given.substr(lead);

	if (number.empty())
	{
		return kAutoPlotMostModuleLooks;   // none, or a 0: the budget as it is
	}

	size_t value = 0;

	if (number.size() <= 8)
	{
		for (char const c : number)
		{
			value = value * 10 + (size_t)(c - '0');
		}
	}

	if (number.size() > 8 || value > kAutoPlotMostModuleLooksSet)
	{
		*words = "AutoPlotMostModuleLooks= under [Editor] in AVP_OpenInteng.ini is more than " + std::to_string((unsigned long long)kAutoPlotMostModuleLooksSet) +
			", the most it takes (no AI module's searches can look at more): the plot's budget is " + std::to_string((unsigned long long)kAutoPlotMostModuleLooksSet);
		return kAutoPlotMostModuleLooksSet;
	}

	return value;
}
