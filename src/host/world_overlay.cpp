// world_overlay.cpp: see world_overlay.h. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "world_overlay.h"

#include <string.h>

#include "level_view.h"   // kLevelViewWorldLinesMax: the most the engine takes

namespace host
{
	namespace
	{
		// the lists are compared byte for byte: a TrackLine has no gaps between its fields
		static_assert(sizeof(TrackLine) == 7 * sizeof(int32_t), "TrackLine is compared with memcmp: no padding");

		std::vector<TrackLine> ownerLines;        // the owner's last list
		std::vector<TrackLine> extraLines;        // the gizmo's last
		std::vector<TrackLine> backgroundLines;   // the background's last (Waypoint modes 1 to 3: every other network, dimmed)
		std::vector<TrackLine> sent;              // what the engine was given: the extra lines, the background, then the owner's
		uint64_t ownerGeneration = 0;             // the owner's change number given with its last list (0: none)
		uint64_t backgroundGeneration = 0;        // the background's

		bool Same(std::vector<TrackLine> const &a, std::vector<TrackLine> const &b)
		{
			return a.size() == b.size() && (a.empty() || memcmp(a.data(), b.data(), a.size() * sizeof(TrackLine)) == 0);
		}

		bool SameAsOwner(TrackLine const *lines, size_t count)
		{
			return count == ownerLines.size() && (count == 0 || memcmp(lines, ownerLines.data(), count * sizeof(TrackLine)) == 0);
		}

		void Send()
		{
			// the background is the part cut when the whole is more than the engine takes (the gizmo and the owner's list are what is being worked on)
			size_t const kept = extraLines.size() + ownerLines.size();
			size_t const room = kept < kLevelViewWorldLinesMax ? kLevelViewWorldLinesMax - kept : 0;
			size_t const background = backgroundLines.size() < room ? backgroundLines.size() : room;

			sent.clear();
			sent.reserve(kept + background);
			sent.insert(sent.end(), extraLines.begin(), extraLines.end());
			sent.insert(sent.end(), backgroundLines.begin(), backgroundLines.begin() + (ptrdiff_t)background);
			sent.insert(sent.end(), ownerLines.begin(), ownerLines.end());
			LevelView_WorldLines(sent.empty() ? nullptr : sent.data(), sent.size());
		}

		// an owner's new list: kept and sent only when it is not the one kept
		void Replace(TrackLine const *lines, size_t count)
		{
			size_t const n = lines ? count : 0;

			if (SameAsOwner(lines, n))
			{
				return;
			}

			ownerLines.assign(n ? lines : nullptr, n ? lines + n : nullptr);
			Send();
		}
	}

	void WorldOverlay_Lines(TrackLine const *lines, size_t count)
	{
		ownerGeneration = 0;
		Replace(lines, count);
	}

	void WorldOverlay_Lines(TrackLine const *lines, size_t count, uint64_t generation)
	{
		size_t const n = lines ? count : 0;

		if (generation != 0 && generation == ownerGeneration && n == ownerLines.size())
		{
			return;   // the owner says nothing changed: not read
		}

		Replace(lines, n);
		ownerGeneration = generation;
	}

	void WorldOverlay_Extra(std::vector<TrackLine> const &lines)
	{
		if (Same(lines, extraLines))
		{
			return;
		}

		extraLines = lines;
		Send();
	}

	void WorldOverlay_Background(TrackLine const *lines, size_t count, uint64_t generation)
	{
		size_t const n = lines ? count : 0;

		if (generation != 0 && generation == backgroundGeneration && n == backgroundLines.size())
		{
			return;   // the owner says nothing changed: not read
		}

		backgroundGeneration = generation;

		if (n == backgroundLines.size() && (n == 0 || memcmp(lines, backgroundLines.data(), n * sizeof(TrackLine)) == 0))
		{
			return;
		}

		backgroundLines.assign(n ? lines : nullptr, n ? lines + n : nullptr);
		Send();
	}

	uint32_t WorldOverlay_Dim(uint32_t argb)
	{
		uint32_t const red = (argb >> 16) & 0xFFu, green = (argb >> 8) & 0xFFu, blue = argb & 0xFFu;

		return (argb & 0xFF000000u) | ((red * 35u / 100u) << 16) | ((green * 35u / 100u) << 8) | (blue * 35u / 100u);
	}

	void WorldOverlay_DimLines(std::vector<TrackLine> *lines)
	{
		for (TrackLine &line : *lines)
		{
			line.colour = WorldOverlay_Dim(line.colour);
		}
	}
}
