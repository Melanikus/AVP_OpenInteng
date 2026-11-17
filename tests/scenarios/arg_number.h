// arg_number.h: a whole number named on a test tool's command line, bounded before any sum or difference uses it (Snyk Code, PR #79: a number from outside could overflow the
// arithmetic it reaches). Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// The number in text, when it is whole, decimal, and from least to most. Else false, and the line printed names the number (what) and why it isn't taken. atoi took "2abc" as 2 and
// any number too big as whatever it became; this takes neither.
inline bool ArgNumber(char const *what, char const *text, long least, long most, long *number)
{
	char *end = nullptr;

	errno = 0;

	long const value = text ? strtol(text, &end, 10) : 0;

	if (!text || end == text || *end != '\0' || errno == ERANGE)
	{
		printf("  FAILED  %s \"%s\" isn't a whole number\n", what, text ? text : "");
		return false;
	}

	if (value < least || value > most)
	{
		printf("  FAILED  %s %ld isn't from %ld to %ld\n", what, value, least, most);
		return false;
	}

	*number = value;
	return true;
}
