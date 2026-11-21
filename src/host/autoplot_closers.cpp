// autoplot_closers.cpp: THE GAP CLOSERS' switch and their packing (autoplot_closers.h). Written 2026-11-21 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_closers.h"
#include "autoplot_plan.h"   // the plan the auto-plot writes (its boxes in the file's numbers, their roles, its notes)

#include <math.h>

#include <algorithm>
#include <string>

// (only for the ini read; NOMINMAX keeps windows.h's min and max macros out of std::min and std::max)
#define NOMINMAX
#include <windows.h>

namespace host
{
	static bool closersOn = false;

	bool AutoPlotClosers_On()
	{
		return closersOn;
	}

	void AutoPlotClosers_SetOn(bool on)
	{
		closersOn = on;
	}

	void AutoPlotClosers_Load(char const *settingsFile)
	{
		// the ini is input from outside: only the exact text "1" turns the closers on (read as text: a number read as a number can wrap round, 4294967297 to 1)
		char value[8] = "";

		closersOn = settingsFile && GetPrivateProfileStringA("Editor", "AutoPlotClosers", "", value, (DWORD)sizeof(value), settingsFile) == 1 && value[0] == '1';
	}

	std::vector<std::pair<double, double> > AutoPlot_CloserPieces(double from, double to, double most, double least)
	{
		std::vector<std::pair<double, double> > pieces;
		double const length = to - from;

		// the sizes are clamped here too (they may come from the dialog: input from outside): never under the body, and the most never under the least
		least = least >= (double)kAutoPlotCloserLeast ? least : (double)kAutoPlotCloserLeast;
		most = most >= least ? most : least;

		if (!(length >= least))
		{
			return pieces;   // too short for a box an alien's body can't swallow whole (and a NaN length gives none)
		}

		// the fewest pieces none of which is longer than the most; each then at least the least, as length >= least... unless most < 2 x least and the stretch
		// falls between them (a 1.3 m stretch with most 1.2: two of 0.65): then one piece, a little longer than the most, rather than two under the least
		int count = (int)ceil(length / most - 1e-9);

		count = count > 1 && length / count < least ? count - 1 : count;
		double const each = length / (double)count;

		for (int k = 0; k < count; k++)
		{
			// the last piece ends exactly at "to", so the pieces touch end to end with no gap from rounding
			pieces.push_back(std::make_pair(from + each * k, k + 1 == count ? to : from + each * (k + 1)));
		}

		return pieces;
	}

	std::vector<std::pair<double, double> > AutoPlot_CloserFree(double from, double to, std::vector<std::pair<double, double> > const &blocked)
	{
		std::vector<std::pair<double, double> > spans;

		for (auto const &b : blocked)
		{
			double const lo = std::min(b.first, b.second), hi = std::max(b.first, b.second);

			if (hi > from && lo < to)
			{
				spans.push_back(std::make_pair(std::max(lo, from), std::min(hi, to)));
			}
		}

		std::sort(spans.begin(), spans.end());

		std::vector<std::pair<double, double> > free;
		double at = from;

		for (auto const &s : spans)
		{
			if (s.first > at)
			{
				free.push_back(std::make_pair(at, s.first));
			}

			at = std::max(at, s.second);
		}

		if (at < to)
		{
			free.push_back(std::make_pair(at, to));
		}

		return free;
	}

	int AutoPlot_CloserClearAims(AutoPlotPlan *plan, double scale)
	{
		if (!plan || !(scale > 0.0))
		{
			return 0;   // no plan, or a scale that is not a number above 0: nothing to measure with
		}

		size_t const n = std::min(plan->list.size(), plan->volumeRoles.size());
		// the game's body box, the room left round it, and a closer's size across its wall (900 out, 200 in), in the file's numbers
		double const across = (kAutoPlotCloserLeast / 2.0) / scale, tall = (double)kAutoPlotCloserLeast / scale, margin = (double)kAutoPlotCloserClear / scale;
		double const wallDepth = (double)(kAutoPlotCloserDepth + kAutoPlotInto) / scale;
		int cleared = 0, tops = 0, bottoms = 0, cut = 0, noWall = 0;
		std::string left;

		// more than 1 into a box every way (boxes that only meet edge to edge are not in each other). In doubles: no sum is put in 32 bits
		auto const reaches = [](double const lo[3], double const hi[3], WaypointRec const &b)
		{
			for (int a = 0; a < 3; a++)
			{
				if (!(lo[a] < (double)b.max[a] - 1.0 && (double)b.min[a] < hi[a] - 1.0))
				{
					return false;
				}
			}

			return true;
		};
		auto const name = [&left](size_t k, size_t o, std::string const &why)
		{
			left += std::string(left.empty() ? "" : ", ") + "closer " + std::to_string((unsigned long long)k) + " by box " + std::to_string((unsigned long long)o) + "'s point (" + why + ")";
		};
		// the way across a closer's wall: its side 1100 across (900 out from the face, 200 into the wall); a closer that is 1100 along its wall too has both
		auto const acrossWall = [wallDepth](WaypointRec const &c, int a)
		{
			return fabs((double)c.max[a] - (double)c.min[a] - (wallDepth + 1.0)) <= 2.0;
		};

		for (size_t k = 0; k < n; k++)
		{
			if (plan->volumeRoles[k] != "closer")
			{
				continue;
			}

			WaypointRec &c = plan->list[k];
			double const middle = ((double)c.min[1] + (double)c.max[1]) / 2.0;
			double needTop = (double)c.min[1], needBottom = (double)c.max[1];   // where its top and its bottom must go (y is down: the top is its least y)
			size_t topBy = n, bottomBy = n;                                       // the box whose point decides each

			noWall += acrossWall(c, 0) || acrossWall(c, 2) ? 0 : 1;

			for (size_t o = 0; o < n; o++)
			{
				if (o == k || plan->volumeRoles[o] == "closer")
				{
					continue;   // another closer's own point: the planner keeps every centre in its own box alone, and closers along a wall meet end to end
				}

				// AT THE POINT: the body there, standing over it or hanging under it, and the room round it
				int32_t const *p = plan->list[o].centre;
				double const lo[3] = { p[0] - across - margin, p[1] - tall - margin, p[2] - across - margin };
				double const hi[3] = { p[0] + across + margin, p[1] + tall + margin, p[2] + across + margin };
				bool in = reaches(lo, hi, c);

				// AT THE POINT'S FOOT on this closer's wall: the body there across the closer's own depth, at the point's place along the wall and its height (and the room
				// round it); counted only when that body is in o as well, so the game can send an alien there on to o's point
				for (int a = 0; a <= 2 && !in; a += 2)
				{
					if (!acrossWall(c, a))
					{
						continue;
					}

					int const b = 2 - a;
					double footLo[3], footHi[3], bodyLo[3], bodyHi[3];

					footLo[a] = bodyLo[a] = (double)c.min[a];
					footHi[a] = bodyHi[a] = (double)c.max[a];
					footLo[b] = lo[b];
					footHi[b] = hi[b];
					bodyLo[b] = p[b] - across;
					bodyHi[b] = p[b] + across;
					footLo[1] = lo[1];
					footHi[1] = hi[1];
					bodyLo[1] = p[1] - tall;
					bodyHi[1] = p[1] + tall;
					in = reaches(bodyLo, bodyHi, plan->list[o]) && reaches(footLo, footHi, c);
				}

				if (!in)
				{
					continue;
				}

				if ((double)p[1] <= middle)
				{
					if (hi[1] > needTop)
					{
						needTop = hi[1];   // the point over the closer's middle: its top goes under the body (the lowest such body decides)
						topBy = o;
					}
				}
				else if (lo[1] < needBottom)
				{
					needBottom = lo[1];    // under it: its bottom goes over the body (the highest decides)
					bottomBy = o;
				}
			}

			if (topBy == n && bottomBy == n)
			{
				continue;
			}

			// THE MARGIN NEVER OPENS A GAP: the closer's top met the box over it and its bottom the box under it, so a move leaves a slot between that box and the closer.
			// A body's corners can straddle a slot as tall as the body, in neither box: a move stops 2 short of that. Cut there, the side is still done when the body
			// itself (without the margin) is clear; else it is named
			double const topMost = (double)c.min[1] + tall - 2.0, bottomMost = (double)c.max[1] - tall + 2.0;
			bool cutTop = false, cutBottom = false, topClear = true, bottomClear = true;

			if (topBy != n && needTop > topMost)
			{
				cutTop = true;
				topClear = topMost >= needTop - margin;
				needTop = topMost;
			}

			if (bottomBy != n && needBottom < bottomMost)
			{
				cutBottom = true;
				bottomClear = bottomMost <= needBottom + margin;
				needBottom = bottomMost;
			}

			double const newTop = topBy != n ? ceil(needTop) : (double)c.min[1], newBottom = bottomBy != n ? floor(needBottom) : (double)c.max[1];
			bool const wantTop = topBy != n && topClear, wantBottom = bottomBy != n && bottomClear;

			// NEVER UNDER THE 0.9 m LEAST: both when it keeps a body's height; else the top alone (the aim-point stall, the first seen); else the bottom alone
			bool doTop = false, doBottom = false;

			if (wantTop && wantBottom && newBottom - newTop >= tall)
			{
				doTop = doBottom = true;
			}
			else if (wantTop && (double)c.max[1] - newTop >= tall)
			{
				doTop = true;
			}
			else if (wantBottom && newBottom - (double)c.min[1] >= tall)
			{
				doBottom = true;
			}

			if (topBy != n && !doTop)
			{
				name(k, topBy, !topClear ? "clear of a body only with a slot over it a body fits in" :
					"only " + std::to_string((long long)floor(((wantBottom ? newBottom : (double)c.max[1]) - newTop) * scale)) + " mm of it would be left");
			}

			if (bottomBy != n && !doBottom)
			{
				name(k, bottomBy, !bottomClear ? "clear of a body only with a slot under it a body fits in" :
					"only " + std::to_string((long long)floor((newBottom - (wantTop ? newTop : (double)c.min[1])) * scale)) + " mm of it would be left");
			}

			if (!doTop && !doBottom)
			{
				continue;
			}

			c.min[1] = doTop ? (int32_t)newTop : c.min[1];
			c.max[1] = doBottom ? (int32_t)newBottom : c.max[1];
			c.centre[1] = (int32_t)(((int64_t)c.min[1] + (int64_t)c.max[1]) / 2);   // its own aim point: still in the middle of it, up and down
			cleared++;
			tops += doTop ? 1 : 0;
			bottoms += doBottom ? 1 : 0;
			cut += (doTop && cutTop) || (doBottom && cutBottom) ? 1 : 0;
		}

		if (cleared > 0 || !left.empty() || noWall > 0)
		{
			plan->notes.push_back("closers cleared of the body at another box's point: " + std::to_string((long long)cleared) + " (tops lowered " + std::to_string((long long)tops) +
				", bottoms raised " + std::to_string((long long)bottoms) + (cut > 0 ? ", margin cut " + std::to_string((long long)cut) : std::string()) + ")" +
				(left.empty() ? std::string() : "; left as they are: " + left) +
				(noWall > 0 ? "; closers whose wall could not be read (no side 1100 across: their points' feet not looked at): " + std::to_string((long long)noWall) : std::string()));
		}

		return cleared;
	}
}
