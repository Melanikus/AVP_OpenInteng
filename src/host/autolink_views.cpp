// See autolink_views.h. Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autolink_views.h"

namespace host
{
	namespace
	{
		using autolink_rules::kBase;
		using autolink_rules::kTurn;

		// FUN_00499b30: IMUL, SHRD 16 (the 64-bit product floored)
		int32_t Mul(int32_t a, int32_t b)
		{
			return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
		}

		// FUN_004b34b0(base, turn, out) (AP-4): out = turn x base, row by row
		void Compose(int32_t const base[9], int32_t const turn[9], int32_t out[9])
		{
			for (int r = 0; r < 3; r++)
			{
				for (int c = 0; c < 3; c++)
				{
					out[r * 3 + c] = Mul(turn[r * 3], base[c]) + Mul(turn[r * 3 + 1], base[3 + c]) + Mul(turn[r * 3 + 2], base[6 + c]);
				}
			}
		}

		void Add(std::vector<AutoLinkView> *views, int32_t const place[3], int32_t const turn[9], int32_t step)
		{
			AutoLinkView view;

			for (int i = 0; i < 3; i++)
			{
				view.place[i] = place[i];
			}

			for (int i = 0; i < 9; i++)
			{
				view.turn[i] = turn[i];
			}

			view.step = step;
			views->push_back(view);
		}
	}

	void AutoLink_ViewsFor(Portal const &portal, std::vector<AutoLinkView> *views)
	{
		if (!views)
		{
			return;
		}

		views->clear();

		if (portal.axis < 0 || portal.axis > 2 || (portal.direction != 1 && portal.direction != -1))
		{
			return;
		}

		// AP-2 (0x4c7dad-0x4c7dca, 0x4c7e56): off the axis the overlap's middle, on it the planes' middle 10000 back into A; the sums in 32 bits as inteng's (an int32 sum of two
		// coordinates of one level cannot wrap: the file's numbers x 100 x the scale are held well inside it by the loader's checks)
		int32_t place[3];

		for (int i = 0; i < 3; i++)
		{
			place[i] = i == portal.axis ? (int32_t)(((int64_t)portal.planeA + portal.planeB) / 2) - portal.direction * autolink_rules::kStepBack : (int32_t)(((int64_t)portal.lo[i] + portal.hi[i]) / 2);
		}

		int32_t const *const base = kBase[portal.axis][portal.direction == 1 ? 1 : 0];
		int32_t turned[9];

		// AP-5: the base, then T1..T4, each picture taken before the next product
		Add(views, place, base, 0);

		for (int t = 0; t < 4; t++)
		{
			Compose(base, kTurn[t], turned);
			Add(views, place, turned, 1 + t);
		}

		// AP-6: x and z contacts: T2 then T1, each at the place moved 500 beyond the overlap's side on the other horizontal axis; y: none (0x4c84cf)
		if (portal.axis != autolink_rules::kShiftSkipsAxis)
		{
			int const side = portal.axis == 0 ? 2 : 0;
			// x: view 6 on direction -1's hi side (0x4c82ff-0x4c8313), view 7 on direction +1's (0x4c841e-0x4c8432); z: the other way round (0x4c84fe-0x4c8512, 0x4c85ca-0x4c85de)
			bool const sixOnHi = portal.axis == 0 ? portal.direction == -1 : portal.direction == 1;
			int32_t six[3] = { place[0], place[1], place[2] };
			int32_t seven[3] = { place[0], place[1], place[2] };

			six[side] = sixOnHi ? portal.hi[side] + autolink_rules::kShift : portal.lo[side] - autolink_rules::kShift;
			seven[side] = !sixOnHi ? portal.hi[side] + autolink_rules::kShift : portal.lo[side] - autolink_rules::kShift;

			Compose(base, kTurn[autolink_rules::kShiftedTurn[0]], turned);
			Add(views, six, turned, 5);
			Compose(base, kTurn[autolink_rules::kShiftedTurn[1]], turned);
			Add(views, seven, turned, 6);
		}
	}

	bool AutoLink_Seen(std::vector<uint32_t> const &pixels, int32_t moduleCount, std::vector<int32_t> *seenNumbers)
	{
		if (!seenNumbers)
		{
			return false;
		}

		seenNumbers->clear();

		size_t const width = (size_t)kAutoLinkPictureWidth, height = (size_t)kAutoLinkPictureHeight;

		if (pixels.size() != width * height || moduleCount < 0)
		{
			return false;
		}

		std::vector<uint8_t> seen((size_t)moduleCount + 1, 0);

		// AP-8 (0x4c708f-0x4c7110): the 318 x 238 starts; an ID above the count anywhere among them is "unexpected colour found"
		size_t const block = (size_t)autolink_rules::kSeenBlock;

		for (size_t r = 0; r < (size_t)autolink_rules::kReadRows && r + block <= height; r++)
		{
			for (size_t c = 0; c < (size_t)autolink_rules::kReadColumns && c + block <= width; c++)
			{
				uint32_t const id = pixels[r * width + c] & 0x00FFFFFFu;

				if (id == 0)
				{
					continue;   // nothing drawn (inteng's 0x7fff)
				}

				if (id > (uint32_t)moduleCount)
				{
					seenNumbers->clear();
					return false;
				}

				if (seen[id])
				{
					continue;
				}

				bool whole = true;

				for (size_t dr = 0; dr < block && whole; dr++)
				{
					for (size_t dc = 0; dc < block && whole; dc++)
					{
						whole = (pixels[(r + dr) * width + c + dc] & 0x00FFFFFFu) == id;
					}
				}

				seen[id] = whole ? 1 : 0;
			}
		}

		for (size_t id = 1; id < seen.size(); id++)
		{
			if (seen[id])
			{
				seenNumbers->push_back((int32_t)id - autolink_rules::kIdOfNumber);
			}
		}

		return true;
	}
}
