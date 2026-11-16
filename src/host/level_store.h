// level_store.h: the editor's open level as the edit path's PayloadStore (field_edit.h). Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng (S3d). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// OpenLevelFile (src/io/level_file.h) has PayloadOf and SetPayload but doesn't know the host's PayloadStore, as the io layer knows nothing of
// the host. This joins them, and does nothing else: each call goes straight to the open level, which does every check (a chunk that isn't
// the level's, a payload of another size). It holds the open level's address only, so it must not outlive it.
#pragma once

#include <vector>

#include "field_edit.h"
#include "level_file.h"

namespace host
{
	class LevelStore : public PayloadStore
	{
	public:
		explicit LevelStore(OpenLevelFile *open) : open(open) {}

		bool PayloadOf(Chunk *chunk, std::vector<unsigned char> *payload) const override
		{
			return open && open->PayloadOf(chunk, payload);
		}

		bool SetPayload(Chunk *chunk, std::vector<unsigned char> const &payload) override
		{
			return open && open->SetPayload(chunk, payload);
		}

	private:
		OpenLevelFile *open;
	};
}
