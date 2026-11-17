// autolink_run.cpp: AutoLink's driver, the worker's part and the frame's writer (autolink_run.h says what inteng does; every step below carries its address).
// Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autolink_run.h"

#include <algorithm>

namespace host
{
	namespace
	{
		// One axis of the cut: lo / hi are the extents' least and most (+0x108 / +0x104 for x)
		int32_t GapOnAxis(int32_t placeA, int32_t mostA, int32_t leastA, int32_t placeB, int32_t mostB, int32_t leastB)
		{
			uint32_t gap;

			if (placeA > placeB)
			{
				gap = (uint32_t)leastA - (uint32_t)mostB - (uint32_t)placeB + (uint32_t)placeA;   // 0x4c7332-0x4c7342: A.least - B.most - B.place + A.place
			}
			else
			{
				gap = (uint32_t)leastB - (uint32_t)mostA - (uint32_t)placeA + (uint32_t)placeB;   // 0x4c7346-0x4c7356: B.least - A.most - A.place + B.place
			}

			return (int32_t)gap;
		}

		struct Work
		{
			std::vector<PortalModule> const &records;
			std::vector<int32_t> const &standsFor;
			int32_t a;
			int32_t visibleDistance;
			std::vector<uint8_t> *shown;
			AutoLinkPictureMaker const &picture;
			std::vector<AutoLinkEvent> *events;
			size_t *pictures;
			std::string *why;

			int32_t Count() const
			{
				return (int32_t)records.size();
			}

			// FUN_004c6fb0(door, handled) on one view (handled is the by-value list, param_2)
			bool Picture(AutoLinkView const &view, int32_t door, std::vector<int32_t> handled)
			{
				std::vector<uint32_t> pixels;

				if (!picture(view, *shown, &pixels, why))
				{
					return false;
				}

				(*pictures)++;

				AutoLinkEvent made;
				std::vector<int32_t> seen;

				made.door = door;
				made.end = false;
				made.badPixel = false;

				// the read-back (0x4c706f-0x4c7110): a bad pixel leaves at 0x4c729a, before the cut, the insertion, the door steps and the Close
				if (!AutoLink_Seen(pixels, Count(), &seen))
				{
					made.badPixel = true;
					events->push_back(made);
					events->push_back(AutoLinkEvent{ door, true, true, {} });
					return true;
				}

				std::vector<int32_t> doorSteps;

				for (int32_t const i : seen)
				{
					if (i < 0 || i >= Count())
					{
						continue;
					}

					// (a) the cut, only with a distance (0x4c7312-0x4c7319)
					if (visibleDistance != 0 && AutoLink_DistanceCut(records[(size_t)a], records[(size_t)i], visibleDistance))
					{
						continue;
					}

					int32_t const standsForThis = standsFor[(size_t)i];

					// (c) a stand-in (DAT_00704330[i] > -1, 0x4c73f2): its door in the tree; i in the door list when not handled yet (0x4c7444-0x4c7530)
					if (standsForThis > -1)
					{
						if (standsForThis >= Count())
						{
							continue;
						}

						if (std::find(handled.begin(), handled.end(), i) == handled.end())
						{
							handled.push_back(i);
							doorSteps.push_back(i);
						}

						made.insert.push_back(standsForThis);
						continue;
					}

					// (b) any other record: itself (0x4c75f5)
					made.insert.push_back(i);
				}

				events->push_back(made);

				// the door steps (0x4c76ea-0x4c7929): the stand-in hidden, its door drawn, the same view again, then back
				for (int32_t const s : doorSteps)
				{
					int32_t const doorOfS = standsFor[(size_t)s];

					(*shown)[(size_t)s] = 0;          // 0x4c770c OR 4
					(*shown)[(size_t)doorOfS] = 1;    // 0x4c7775 AND ~4

					bool const drawn = Picture(view, doorOfS, handled);   // 0x4c785c: handled passed by value (copied by FUN_004fc350)

					(*shown)[(size_t)s] = 1;          // 0x4c78b3 AND ~4
					(*shown)[(size_t)doorOfS] = 0;    // 0x4c7910 OR 4

					if (!drawn)
					{
						return false;
					}
				}

				events->push_back(AutoLinkEvent{ door, true, false, {} });   // 0x4c794b-0x4c7963
				return true;
			}
		};
	}

	bool AutoLink_DistanceCut(PortalModule const &a, PortalModule const &b, int32_t visibleDistance)
	{
		// extents: maxX, minX, maxY, minY, maxZ, minZ (+0x104 .. +0x118)
		int32_t const gapX = GapOnAxis(a.place[0], a.extents[0], a.extents[1], b.place[0], b.extents[0], b.extents[1]);
		int32_t const gapY = GapOnAxis(a.place[1], a.extents[2], a.extents[3], b.place[1], b.extents[2], b.extents[3]);
		int32_t const gapZ = GapOnAxis(a.place[2], a.extents[4], a.extents[5], b.place[2], b.extents[4], b.extents[5]);

		return gapX > visibleDistance || gapY > visibleDistance || gapZ > visibleDistance;   // 0x4c73be-0x4c73d2 (JG: signed)
	}

	bool AutoLink_ModuleWork(std::vector<PortalModule> const &records, std::vector<int32_t> const &standsFor, int32_t a, int32_t visibleDistance, std::vector<uint8_t> *shown,
		AutoLinkPictureMaker const &picture, std::vector<AutoLinkEvent> *events, size_t *pictures, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		size_t pictureCount = 0;

		pictures = pictures ? pictures : &pictureCount;

		if (!shown || !events || !picture || standsFor.size() != records.size() || shown->size() != records.size() || a < 0 || (size_t)a >= records.size() || standsFor[(size_t)a] > -1)
		{
			*why = "AutoLink: the module's work was asked with records that do not fit together";
			return false;
		}

		Work work{ records, standsFor, a, visibleDistance, shown, picture, events, pictures, why };

		// FUN_004c8930 0x4c8a2d-0x4c8a5d: the first stand-in of a (from 0) hidden, a drawn
		int32_t standIn = -1;

		for (size_t s = 0; s < standsFor.size(); s++)
		{
			if (standsFor[s] == a)
			{
				standIn = (int32_t)s;
				break;
			}
		}

		if (standIn >= 0)
		{
			(*shown)[(size_t)standIn] = 0;
			(*shown)[(size_t)a] = 1;
		}

		bool drawn = true;

		// FUN_004c7ab0 0x4c7d5b-0x4c7da3: each other record with a file object that meets a, in number order; each of its views (0x4c7dad-0x4c866f)
		for (size_t b = 0; drawn && b < records.size(); b++)
		{
			Portal portal;
			std::vector<AutoLinkView> views;

			if ((int32_t)b == a || standsFor[b] > -1 || !AutoLink_ModulesMeet(records[(size_t)a], records[b], &portal))
			{
				continue;
			}

			AutoLink_ViewsFor(portal, &views);

			for (size_t v = 0; drawn && v < views.size(); v++)
			{
				drawn = work.Picture(views[v], -1, std::vector<int32_t>());   // the copy of DAT_00704348: always empty (only made and freed, 0x4c6ee0 / 0x4c6f00)
			}
		}

		// 0x4c8a76-0x4c8a91: a hidden, its stand-in drawn (they stay so for the rest of the run)
		if (standIn >= 0)
		{
			(*shown)[(size_t)a] = 0;
			(*shown)[(size_t)standIn] = 1;
		}

		return drawn;
	}

	bool AutoLink_ViewWork(std::vector<PortalModule> const &records, std::vector<int32_t> const &standsFor, int32_t a, AutoLinkView const &view, int32_t visibleDistance,
		std::vector<uint8_t> *shown, AutoLinkPictureMaker const &picture, std::vector<AutoLinkEvent> *events, size_t *pictures, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		size_t pictureCount = 0;

		pictures = pictures ? pictures : &pictureCount;

		if (!shown || !events || !picture || standsFor.size() != records.size() || shown->size() != records.size() || a < 0 || (size_t)a >= records.size() || standsFor[(size_t)a] > -1)
		{
			*why = "AutoLink: the view's work was asked with records that do not fit together";
			return false;
		}

		Work work{ records, standsFor, a, visibleDistance, shown, picture, events, pictures, why };

		// FUN_004c8be0 0x4c8f25-0x4c8f58: the first stand-in of a (from 0) hidden, a drawn, as the driver does
		int32_t standIn = -1;

		for (size_t s = 0; s < standsFor.size() && standIn < 0; s++)
		{
			standIn = standsFor[s] == a ? (int32_t)s : standIn;
		}

		if (standIn >= 0)
		{
			(*shown)[(size_t)standIn] = 0;
			(*shown)[(size_t)a] = 1;
		}

		bool const drawn = work.Picture(view, -1, std::vector<int32_t>());   // 0x4c9031: FUN_004c6fb0(0, a copy of the empty DAT_00704348)

		// 0x4c9040-0x4c905a: a hidden, its stand-in drawn
		if (standIn >= 0)
		{
			(*shown)[(size_t)a] = 0;
			(*shown)[(size_t)standIn] = 1;
		}

		return drawn;
	}

	bool AutoLinkTreeWriter::Apply(LinkTree *tree, AutoLinkEvent const &event, std::vector<int32_t> const &objectIndexOf, size_t *added)
	{
		auto const objectOf = [&objectIndexOf](int32_t record, int32_t *out)
		{
			if (record < 0 || (size_t)record >= objectIndexOf.size())
			{
				return false;
			}

			*out = objectIndexOf[(size_t)record];
			return true;
		};

		if (!tree)
		{
			return false;
		}

		if (event.end)
		{
			if (begun.empty())
			{
				return false;
			}

			Begun const last = begun.back();

			begun.pop_back();

			// 0x4c7952-0x4c7963: a door picture that made or reused a branch closes it. INTENG BUG, FIXED (MAPPING-ORDERS section 15): a picture with a bad pixel too (inteng left at
			// 0x4c729a, before the Close, and a door branch it had re-opened at 0x4c7023 stayed open)
			tree->AutoLinkEnd(last.picture);
			return true;
		}

		int32_t door = -1;
		std::vector<int32_t> modules;

		if (event.door >= 0 && !objectOf(event.door, &door))
		{
			return false;
		}

		for (int32_t const record : event.insert)
		{
			int32_t module;

			if (!objectOf(record, &module))
			{
				return false;
			}

			modules.push_back(module);
		}

		// the door node's walk happens first (0x4c6fe8-0x4c702e), before the read-back, so a bad picture has it too
		Begun now{ tree->AutoLinkBegin(door) };

		for (int32_t const module : modules)
		{
			if (tree->AutoLinkAdd(&now.picture, module) && added)
			{
				(*added)++;
			}
		}

		begun.push_back(now);
		return true;
	}

	bool AutoLinkTreeWriter::Balanced() const
	{
		return begun.empty();
	}
}
