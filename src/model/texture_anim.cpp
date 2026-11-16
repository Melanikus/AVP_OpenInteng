// texture_anim.cpp: see texture_anim.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_anim.h"

#include <string.h>

namespace model
{
	namespace
	{
		void Put(std::vector<unsigned char> *out, int32_t value)
		{
			unsigned char raw[sizeof(value)];

			memcpy(raw, &value, sizeof(value));
			out->insert(out->end(), raw, raw + sizeof(raw));
		}
	}

	char const *TextureAnimWords(TextureAnimResult result)
	{
		switch (result)
		{
		case TextureAnimResult::Ok: return "the animation fits the chunk";
		case TextureAnimResult::NotFit: return "the animation would not be taken by the game's loader (a polygon needs 3 corners and a sequence, a sequence 1 to 32767 frames)";
		case TextureAnimResult::TooBig: return "the animation is too big for a chunk";
		}

		return "";
	}

	TextureAnimResult WriteTextureAnim(TextureAnim const &anim, std::vector<unsigned char> *payload)
	{
		if (!payload)
		{
			return TextureAnimResult::NotFit;
		}

		payload->clear();
		Put(payload, (int32_t)anim.polygons.size());

		for (AnimPolygon const &polygon : anim.polygons)
		{
			if (polygon.numVerts != 3 || polygon.sequences.empty())
			{
				return TextureAnimResult::NotFit;
			}

			Put(payload, polygon.polygon);
			Put(payload, polygon.group);
			Put(payload, (int32_t)polygon.sequences.size());
			Put(payload, polygon.numVerts);
			Put(payload, polygon.flags);
			Put(payload, polygon.identifier);

			for (AnimSequence const &sequence : polygon.sequences)
			{
				size_t const frames = sequence.textures.size();

				if (frames < 1 || frames > (size_t)kMostAnimFrames || sequence.uv.size() != frames * 6 || (frames == 1 && (sequence.flags & kAnimNoInterpToFirst)))
				{
					return TextureAnimResult::NotFit;
				}

				Put(payload, sequence.speed);
				Put(payload, sequence.flags);
				Put(payload, (int32_t)frames);
				Put(payload, sequence.spare1);
				Put(payload, sequence.spare2);

				for (int32_t const texture : sequence.textures)
				{
					Put(payload, texture);
				}

				for (int32_t const number : sequence.uv)
				{
					Put(payload, number);
				}
			}

			if (payload->size() > 0x7ffffff0u)
			{
				return TextureAnimResult::TooBig;
			}
		}

		return TextureAnimResult::Ok;
	}

	int32_t NextAnimGroup(TextureAnim const &anim, int32_t atLeast)
	{
		int32_t next = atLeast;

		for (AnimPolygon const &polygon : anim.polygons)
		{
			if (polygon.group + 1 > next)
			{
				next = polygon.group + 1;
			}
		}

		return next;
	}

	AnimSequence NewAnimSequence(int32_t texture, int32_t const uv[6])
	{
		AnimSequence sequence;

		sequence.textures.push_back(texture);
		sequence.uv.assign(uv, uv + 6);
		return sequence;
	}

	void AnimAddFrame(AnimSequence *sequence, size_t after, int32_t texture, int32_t const uv[6])
	{
		size_t const place = after < sequence->textures.size() ? after + 1 : sequence->textures.size();

		sequence->textures.insert(sequence->textures.begin() + (std::ptrdiff_t)place, texture);
		sequence->uv.insert(sequence->uv.begin() + (std::ptrdiff_t)(place * 6), uv, uv + 6);
	}

	void AnimRemoveFrame(AnimSequence *sequence, size_t at)
	{
		if (at >= sequence->textures.size())
		{
			return;
		}

		sequence->textures.erase(sequence->textures.begin() + (std::ptrdiff_t)at);
		sequence->uv.erase(sequence->uv.begin() + (std::ptrdiff_t)(at * 6), sequence->uv.begin() + (std::ptrdiff_t)(at * 6 + 6));
	}
}
