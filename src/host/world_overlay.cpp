// world_overlay.cpp: see world_overlay.h. Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "world_overlay.h"

#include <string.h>

namespace host
{
	namespace
	{
		std::vector<TrackLine> ownerLines;   // the owner's last list
		std::vector<TrackLine> extraLines;   // the gizmo's last
		std::vector<TrackLine> sent;         // what the engine was given: the extra lines, then the owner's

		bool Same(std::vector<TrackLine> const &a, std::vector<TrackLine> const &b)
		{
			return a.size() == b.size() && (a.empty() || memcmp(a.data(), b.data(), a.size() * sizeof(TrackLine)) == 0);
		}

		void Send()
		{
			sent.clear();
			sent.reserve(extraLines.size() + ownerLines.size());
			sent.insert(sent.end(), extraLines.begin(), extraLines.end());
			sent.insert(sent.end(), ownerLines.begin(), ownerLines.end());
			LevelView_WorldLines(sent.empty() ? nullptr : sent.data(), sent.size());
		}
	}

	void WorldOverlay_Lines(TrackLine const *lines, size_t count)
	{
		ownerLines.assign(lines && count ? lines : nullptr, lines && count ? lines + count : nullptr);
		Send();
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
}
