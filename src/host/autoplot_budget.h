// autoplot_budget.h: THE PLOT BUDGET'S SETTING, READ AS TEXT. Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng, on Drake's review of 6774ce25 (MUST 1,
// measured: GetPrivateProfileIntA wraps past 4294967295 and keeps only a number's leading digits, so 4294967297 gave a budget of 1, 12abc gave 12 and 8,000,000,
// the way the editor's own words write the number, gave 8, which refuses nearly every module). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// Pure: no Windows, no editor state, so host_autoplot_budget_test links only it.
#pragma once

#include <stddef.h>

#include <string>

namespace host
{
	// [Editor] AutoPlotMostModuleLooks= as the settings file gives it (its text, read with GetPrivateProfileStringA): the budget it means (autoplot_command.h), and in
	// *words what the status line says when it was refused or held ("" otherwise).
	//   "" or a 0 (spaces and tabs round it let go): kAutoPlotMostModuleLooks, no words;
	//   plain digits from 1 to kAutoPlotMostModuleLooksSet: that number, no words;
	//   plain digits past it (more than 8 digits once leading zeros go, or a bigger number): kAutoPlotMostModuleLooksSet, and words saying it was held there;
	//   anything else (a sign, a comma or a dot between thousands, a space inside, a letter): kAutoPlotMostModuleLooks, and words naming the setting and how it is
	//   written
	size_t AutoPlotBudget_FromText(std::string const &text, std::string *words);
}
