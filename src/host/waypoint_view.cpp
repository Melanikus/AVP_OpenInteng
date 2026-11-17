// See waypoint_view.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_view.h"

#include <math.h>

#include <algorithm>

namespace host
{
	namespace
	{
		int32_t World(int64_t file, double scale)
		{
			double const value = (double)file * scale;

			if (value > 2147483000.0)
			{
				return 2147483000;
			}

			return value < -2147483000.0 ? -2147483000 : (int32_t)value;
		}

		void Line(std::vector<TrackLine> *lines, int32_t const a[3], int32_t const b[3], uint32_t colour)
		{
			TrackLine line;

			for (int i = 0; i < 3; i++)
			{
				line.from[i] = a[i];
				line.to[i] = b[i];
			}

			line.colour = colour;
			lines->push_back(line);
		}
	}

	size_t WaypointView_Lines(WaypointMode const &mode, double levelScale, std::vector<TrackLine> *lines, int32_t mins[3], int32_t maxs[3])
	{
		lines->clear();

		for (int i = 0; i < 3; i++)
		{
			mins[i] = maxs[i] = 0;
		}

		std::vector<WaypointRec> const &list = mode.List();

		if (mode.Mode() < 1 || list.empty())
		{
			return 0;
		}

		double const scale = levelScale > 0.0 ? levelScale : 1.0;
		bool first = true;

		for (size_t w = 0; w < list.size(); w++)
		{
			WaypointRec const &wp = list[w];
			int32_t low[3], high[3];

			for (int i = 0; i < 3; i++)
			{
				low[i] = World(wp.min[i], scale);
				high[i] = World(wp.max[i], scale);

				// the place aliens aim for is not always inside the box (a link is drawn to it): the bounds hold it too
				int32_t const aim = World(wp.centre[i], scale);
				int32_t const lowest = aim < low[i] ? aim : low[i];
				int32_t const highest = aim > high[i] ? aim : high[i];

				if (first || lowest < mins[i])
				{
					mins[i] = lowest;
				}

				if (first || highest > maxs[i])
				{
					maxs[i] = highest;
				}
			}

			first = false;

			// Minimise (0x1c): the boxes of the other waypoints are not drawn, only the current one's (named: inteng draws the others' boxes small)
			if (mode.Minimised() && (int32_t)w != mode.CurrentWaypoint())
			{
				continue;
			}

			uint32_t const colour = (int32_t)w == mode.CurrentWaypoint() ? kWaypointCurrentColour : ((int32_t)w == mode.BaseWaypoint() ? kWaypointBaseColour : kWaypointBoxColour);

			// the twelve edges: for each axis the four edges along it
			for (int axis = 0; axis < 3; axis++)
			{
				int const u = (axis + 1) % 3, v = (axis + 2) % 3;

				for (int corner = 0; corner < 4; corner++)
				{
					int32_t a[3], b[3];

					a[axis] = low[axis];
					b[axis] = high[axis];
					a[u] = b[u] = (corner & 1) ? high[u] : low[u];
					a[v] = b[v] = (corner & 2) ? high[v] : low[v];
					Line(lines, a, b, colour);
				}
			}
		}

		// the links, from the middle of one waypoint towards the middle of the other; a link with only One way is drawn to halfway
		for (size_t w = 0; w < list.size(); w++)
		{
			WaypointRec const &wp = list[w];
			int32_t from[3];

			for (int i = 0; i < 3; i++)
			{
				from[i] = World(wp.centre[i], scale);
			}

			for (WaypointLinkRec const &link : wp.links)
			{
				if (link.index < 0 || (size_t)link.index >= list.size())
				{
					continue;
				}

				WaypointRec const &other = list[(size_t)link.index];
				uint32_t const colour = (mode.HighlightFlag() != 0 && (link.flags & mode.HighlightFlag())) ? kWaypointHighlightColour : ((link.flags & kLinkDeviation) ? kWaypointDeviationColour : ((link.flags & kLinkAlien) ? kWaypointAlienLinkColour : kWaypointLinkColour));
				bool const oneWay = (link.flags & kLinkOneWay) != 0;

				// a two-way link is drawn once, from the lower number
				if (!oneWay && !(link.flags & kLinkOneWayBack) && link.index < (int32_t)w)
				{
					continue;
				}

				if (!oneWay && (link.flags & kLinkOneWayBack))
				{
					continue;   // the other end draws it
				}

				int32_t to[3];

				for (int i = 0; i < 3; i++)
				{
					int32_t const end = World(other.centre[i], scale);

					to[i] = oneWay ? from[i] + (end - from[i]) / 2 : end;
				}

				Line(lines, from, to, colour);
			}
		}

		return list.size();
	}

	// The block's centre of a waypoint's box: the middle of the box, or the aim point for a minimised one (FUN_00586e90: the cube is built round param_1[1..3])
	void WaypointView_BoxCentre(WaypointMode const &mode, size_t index, double levelScale, int32_t out[3])
	{
		std::vector<WaypointRec> const &list = mode.List();
		double const scale = levelScale > 0.0 ? levelScale : 1.0;

		for (int i = 0; i < 3; i++)
		{
			out[i] = 0;
		}

		if (index >= list.size())
		{
			return;
		}

		WaypointRec const &wp = list[index];
		bool const small = mode.Minimised() && (int32_t)index != mode.CurrentWaypoint();

		for (int i = 0; i < 3; i++)
		{
			out[i] = small ? World(wp.centre[i], scale) : World(((int64_t)wp.min[i] + wp.max[i]) / 2, scale);
		}
	}

	void WaypointView_WorldLines(WaypointMode const &mode, double levelScale, std::vector<TrackLine> *lines)
	{
		lines->clear();

		std::vector<WaypointRec> const &list = mode.List();

		if (mode.Mode() < 1 || list.empty())
		{
			return;
		}

		double const scale = levelScale > 0.0 ? levelScale : 1.0;

		// the boxes (FUN_00583130's six polyline faces: the twelve edges); a minimised waypoint's box is a cube 20,000 of inteng's units (200 of the world's) each way round its aim point (FUN_00586e90)
		for (size_t w = 0; w < list.size(); w++)
		{
			WaypointRec const &wp = list[w];
			bool const small = mode.Minimised() && (int32_t)w != mode.CurrentWaypoint();
			int32_t low[3], high[3];

			for (int i = 0; i < 3; i++)
			{
				int32_t const aim = World(wp.centre[i], scale);

				low[i] = small ? aim - kWaypointMinimisedHalf : World(wp.min[i], scale);
				high[i] = small ? aim + kWaypointMinimisedHalf : World(wp.max[i], scale);
			}

			uint32_t const colour = (int32_t)w == mode.CurrentWaypoint() ? kWaypointBoxCurrent : ((int32_t)w == mode.BaseWaypoint() ? kWaypointBoxBase : kWaypointBoxPlain);

			for (int axis = 0; axis < 3; axis++)
			{
				int const u = (axis + 1) % 3, v = (axis + 2) % 3;

				for (int corner = 0; corner < 4; corner++)
				{
					int32_t a[3], b[3];

					a[axis] = low[axis];
					b[axis] = high[axis];
					a[u] = b[u] = (corner & 1) ? high[u] : low[u];
					a[v] = b[v] = (corner & 2) ? high[v] : low[v];
					Line(lines, a, b, colour);
				}
			}
		}

		// Edit Extents' Centre Edit (FUN_005888c0): the link lines are hidden and the marker shown, a GREEN cube 10,000 of inteng's units each way (100 of the world's: the box FUN_00581650 makes at
		// 0x582cba-0x582ce8) put on the current waypoint's aim point every frame (0x5862f0)
		bool const centreEdit = mode.Mode() == 2 && mode.CentreEdit();

		if (centreEdit && mode.CurrentWaypoint() >= 0 && (size_t)mode.CurrentWaypoint() < list.size())
		{
			WaypointRec const &wp = list[(size_t)mode.CurrentWaypoint()];
			int32_t low[3], high[3];

			for (int i = 0; i < 3; i++)
			{
				int32_t const aim = World(wp.centre[i], scale);

				low[i] = aim - kWaypointAimMarkerHalf;
				high[i] = aim + kWaypointAimMarkerHalf;
			}

			for (int axis = 0; axis < 3; axis++)
			{
				int const u = (axis + 1) % 3, v = (axis + 2) % 3;

				for (int corner = 0; corner < 4; corner++)
				{
					int32_t a[3], b[3];

					a[axis] = low[axis];
					b[axis] = high[axis];
					a[u] = b[u] = (corner & 1) ? high[u] : low[u];
					a[v] = b[v] = (corner & 2) ? high[v] : low[v];
					Line(lines, a, b, kWaypointAimMarker);
				}
			}
		}

		// the links: ONE line of two halves for the pair (FUN_005835a0), made by the first link met in the list's order and shared by the other end's link back (FUN_005876c0 0x587a17-0x587ae6);
		// its colours from that link (FUN_00586570): the half at the link's own waypoint red with flag 2, the half at the other waypoint red with flag 1, else green, yellow when the link has the
		// highlighted flag
		for (size_t w = 0; w < list.size() && !centreEdit; w++)
		{
			WaypointRec const &wp = list[w];

			for (WaypointLinkRec const &link : wp.links)
			{
				if (link.index < 0 || (size_t)link.index >= list.size())
				{
					continue;
				}

				WaypointRec const &other = list[(size_t)link.index];

				// a waypoint earlier in the list with a link back has drawn this pair already
				if ((size_t)link.index < w && std::any_of(other.links.begin(), other.links.end(), [&](WaypointLinkRec const &back) { return back.index == (int32_t)w; }))
				{
					continue;
				}

				int32_t from[3], to[3], middle[3];

				for (int i = 0; i < 3; i++)
				{
					from[i] = World(wp.centre[i], scale);
					to[i] = World(other.centre[i], scale);
					middle[i] = World(((int64_t)wp.centre[i] + other.centre[i]) / 2, scale);
				}

				uint32_t const plain = (link.flags & mode.HighlightFlag()) != 0 ? kWaypointLinkHighlighted : kWaypointLinkPlain;

				Line(lines, from, middle, (link.flags & kLinkOneWayBack) ? kWaypointLinkClosed : plain);
				Line(lines, middle, to, (link.flags & kLinkOneWay) ? kWaypointLinkClosed : plain);
			}
		}
	}

	void WaypointView_FileUnits(int32_t const world[3], double levelScale, int32_t file[3])
	{
		double const scale = levelScale > 0.0 ? levelScale : 1.0;

		for (int i = 0; i < 3; i++)
		{
			file[i] = (int32_t)floor((double)world[i] / scale);
		}
	}
}
