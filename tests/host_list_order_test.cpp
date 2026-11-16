// The test for the list dialog's order (list_order.h): the same order as inteng's insertion (FUN_00475bf0 0x476403-0x476670, each row after the ones not above it), and fast on a hostile
// level's row count (Vasquez's find through Bishop, 2026-11-16: 100,000 rows froze Find / Select). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt. No window, none of the game.

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <string>
#include <vector>

#include "list_order.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// inteng's own way, one insertion at a time (the order the dialog showed before list_order.h)
static std::vector<size_t> InsertionOrder(std::vector<std::string> const &rows)
{
	std::vector<size_t> order;

	for (size_t i = 0; i < rows.size(); i++)
	{
		size_t at = order.size();

		while (at > 0 && rows[order[at - 1]].compare(rows[i]) > 0)
		{
			at--;
		}

		order.insert(order.begin() + (std::ptrdiff_t)at, i);
	}

	return order;
}

static uint32_t seed = 12345;

static uint32_t Next()
{
	seed = seed * 1103515245u + 12345u;
	return (seed >> 8) & 0xffffff;
}

int main()
{
	puts("The same order as inteng's insertion:");
	Check(ListOrder({}).empty(), "no rows: an empty order");
	Check(ListOrder({ "b", "a", "c" }) == std::vector<size_t>({ 1, 0, 2 }), "a, b, c from b, a, c");
	Check(ListOrder({ "x", "a", "x", "a" }) == std::vector<size_t>({ 1, 3, 0, 2 }), "equal rows keep the order they were given in");
	Check(ListOrder({ "Zeta", "alpha", "Alpha", "\xe9t\xe9" }) == InsertionOrder({ "Zeta", "alpha", "Alpha", "\xe9t\xe9" }), "capitals, small letters and a byte above 0x7f as the insertion orders them");

	bool allSame = true;

	for (int trial = 0; trial < 300 && allSame; trial++)
	{
		std::vector<std::string> rows;
		size_t const count = 1 + Next() % 60;

		for (size_t i = 0; i < count; i++)
		{
			std::string row;
			size_t const length = Next() % 4;

			for (size_t c = 0; c < length; c++)
			{
				row += (char)('a' + Next() % 3);   // few letters, so many rows are equal
			}

			rows.push_back(row);
		}

		allSame = ListOrder(rows) == InsertionOrder(rows);
	}

	Check(allSame, "300 random lists of up to 60 short rows, many equal: the same order every time");

	puts("A hostile level's count:");
	{
		std::vector<std::string> rows;

		for (size_t i = 0; i < 200000; i++)
		{
			rows.push_back("object" + std::to_string((unsigned long)Next()));
		}

		clock_t const before = clock();
		std::vector<size_t> const order = ListOrder(rows);
		double const seconds = (double)(clock() - before) / CLOCKS_PER_SEC;
		bool ascending = order.size() == rows.size();

		for (size_t k = 1; k < order.size() && ascending; k++)
		{
			ascending = rows[order[k - 1]].compare(rows[order[k]]) <= 0;
		}

		Check(ascending, "200,000 rows come out ascending");
		Check(seconds < 2.0, "and in under 2 seconds (" + std::to_string(seconds) + " s; one insertion at a time took minutes)");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
