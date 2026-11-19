// See waypoint_model.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "waypoint_model.h"

#include <string.h>

#include <algorithm>

namespace host
{
	namespace
	{
		size_t const kHeading = 12;
		int32_t const kMostPlace = 1 << 24;       // CheckPlace (chunk.hpp)

		int32_t IntAt(uint8_t const *bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes + at, 4);
			return value;
		}

		void PutInt(std::vector<uint8_t> *out, int32_t value)
		{
			uint8_t bytes[4];

			memcpy(bytes, &value, 4);
			out->insert(out->end(), bytes, bytes + 4);
		}

		bool Refuse(std::string *why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}

		bool PlaceOk(int32_t const v[3])
		{
			for (int i = 0; i < 3; i++)
			{
				if (v[i] > kMostPlace || v[i] < -kMostPlace)
				{
					return false;
				}
			}

			return true;
		}

		int FindLink(std::vector<WaypointLinkRec> const &links, int32_t to)
		{
			for (size_t i = 0; i < links.size(); i++)
			{
				if (links[i].index == to)
				{
					return (int)i;
				}
			}

			return -1;
		}
	}

	bool Waypoints_Parse(uint8_t const *bytes, size_t size, WaypointSet *set, std::string *why)
	{
		set->alien.clear();
		set->ground.clear();

		if (!bytes || size < kHeading + 4 || memcmp(bytes, "WAYPOINT", 8) != 0)
		{
			return Refuse(why, "not a WAYPOINT chunk");
		}

		if (IntAt(bytes, 8) < 0 || (size_t)IntAt(bytes, 8) != size)
		{
			return Refuse(why, "the chunk's size is not the bytes' size");
		}

		int32_t const count = IntAt(bytes, kHeading);
		size_t const smallest = 3 * 12 + 4 * 4;    // three corners, two numbers and two counts

		if (count < 0 || (size_t)count > (size - kHeading - 4) / smallest)
		{
			return Refuse(why, "the number of waypoints does not fit the chunk");
		}

		size_t at = kHeading + 4;
		std::vector<WaypointRec> all;
		int32_t firstGround = -1;

		for (int32_t i = 0; i < count; i++)
		{
			if (at + smallest > size)
			{
				return Refuse(why, "a waypoint runs past the chunk");
			}

			WaypointRec wp;

			for (int c = 0; c < 3; c++)
			{
				wp.min[c] = IntAt(bytes, at + 4 * (size_t)c);
				wp.max[c] = IntAt(bytes, at + 12 + 4 * (size_t)c);
				wp.centre[c] = IntAt(bytes, at + 24 + 4 * (size_t)c);
			}

			at += 36;

			if (!PlaceOk(wp.min) || !PlaceOk(wp.max) || !PlaceOk(wp.centre))
			{
				return Refuse(why, "a waypoint's corner is too far from the middle of the level");
			}

			wp.flags = IntAt(bytes, at);
			wp.spare2 = IntAt(bytes, at + 4);
			at += 8;

			int32_t const links = IntAt(bytes, at);

			at += 4;

			if (links < 0 || (size_t)links > (size - at) / 8)
			{
				return Refuse(why, "a waypoint's number of links does not fit the chunk");
			}

			for (int32_t l = 0; l < links; l++)
			{
				WaypointLinkRec link;

				link.index = IntAt(bytes, at);
				link.flags = IntAt(bytes, at + 4);
				wp.links.push_back(link);
				at += 8;
			}

			if (at + 4 > size)
			{
				return Refuse(why, "a waypoint's module links run past the chunk");
			}

			int32_t const modules = IntAt(bytes, at);

			at += 4;

			if (modules < 0 || (size_t)modules > (size - at) / 8)
			{
				return Refuse(why, "a waypoint's number of module links does not fit the chunk");
			}

			for (int32_t m = 0; m < modules; m++)
			{
				void const *const zero = at < size ? memchr(bytes + at, 0, size - at) : nullptr;

				if (!zero)
				{
					return Refuse(why, "a module link's name has no end");
				}

				size_t const length = (size_t)((uint8_t const *)zero - (bytes + at));
				size_t const padded = (length + 4) & ~(size_t)3;

				if (padded > size - at || size - at - padded < 4)
				{
					return Refuse(why, "a module link runs past the chunk");
				}

				WaypointModuleLink link;

				link.name.assign((char const *)bytes + at, length);
				link.flags = IntAt(bytes, at + padded);
				wp.moduleLinks.push_back(link);
				at += padded + 4;
			}

			if (wp.flags & kWaypointFlagFirstGround)
			{
				firstGround = i;
				wp.flags &= ~kWaypointFlagFirstGround;
			}

			all.push_back(wp);
		}

		if (at != size)
		{
			return Refuse(why, "bytes are left over after the last waypoint");
		}

		size_t const groundAt = firstGround >= 0 ? (size_t)firstGround : all.size();

		for (size_t i = 0; i < all.size(); i++)
		{
			(i < groundAt ? set->alien : set->ground).push_back(all[i]);
		}

		// a link has to lead to a place in its own list
		for (size_t i = 0; i < all.size(); i++)
		{
			size_t const listSize = i < groundAt ? set->alien.size() : set->ground.size();

			for (WaypointLinkRec const &link : all[i].links)
			{
				if (link.index < 0 || (size_t)link.index >= listSize)
				{
					set->alien.clear();
					set->ground.clear();
					return Refuse(why, "a link leads to a waypoint that is not in its list");
				}
			}
		}

		return true;
	}

	std::vector<uint8_t> Waypoints_Serialize(WaypointSet const &set)
	{
		std::vector<uint8_t> out;
		size_t const total = set.alien.size() + set.ground.size();

		out.insert(out.end(), (uint8_t const *)"WAYPOINT", (uint8_t const *)"WAYPOINT" + 8);
		PutInt(&out, 0);   // the size, put in at the end
		PutInt(&out, (int32_t)total);

		for (size_t i = 0; i < total; i++)
		{
			WaypointRec const &wp = i < set.alien.size() ? set.alien[i] : set.ground[i - set.alien.size()];

			for (int c = 0; c < 3; c++)
			{
				PutInt(&out, wp.min[c]);
			}

			for (int c = 0; c < 3; c++)
			{
				PutInt(&out, wp.max[c]);
			}

			for (int c = 0; c < 3; c++)
			{
				PutInt(&out, wp.centre[c]);
			}

			PutInt(&out, i == set.alien.size() ? (wp.flags | kWaypointFlagFirstGround) : wp.flags);
			PutInt(&out, wp.spare2);
			PutInt(&out, (int32_t)wp.links.size());

			for (WaypointLinkRec const &link : wp.links)
			{
				PutInt(&out, link.index);
				PutInt(&out, link.flags);
			}

			PutInt(&out, (int32_t)wp.moduleLinks.size());

			for (WaypointModuleLink const &link : wp.moduleLinks)
			{
				size_t const padded = (link.name.size() + 4) & ~(size_t)3;
				size_t const at = out.size();

				out.resize(at + padded, 0);
				memcpy(out.data() + at, link.name.data(), link.name.size());
				PutInt(&out, link.flags);
			}
		}

		int32_t const size = (int32_t)out.size();

		memcpy(out.data() + 8, &size, 4);
		return out;
	}

	bool Waypoints_AddLink(std::vector<WaypointRec> *list, int32_t a, int32_t b, int32_t flags)
	{
		if (!list || a < 0 || b < 0 || (size_t)a >= list->size() || (size_t)b >= list->size())
		{
			return false;
		}

		// FUN_00586b40's loop: the first pass is (a, b, flags), the second (b, a, flags with One way turned into One way the other direction)
		int32_t from = a, to = b, bits = flags;

		for (int pass = 0; pass < 2; pass++)
		{
			WaypointRec &wp = (*list)[(size_t)from];
			int const at = FindLink(wp.links, to);

			if (at >= 0)
			{
				wp.links[(size_t)at].flags |= bits;

				if ((wp.links[(size_t)at].flags & 1) && (wp.links[(size_t)at].flags & 2))
				{
					wp.links[(size_t)at].flags &= ~3;
				}
			}
			else
			{
				WaypointLinkRec link;

				link.index = to;
				link.flags = bits;
				wp.links.push_back(link);
			}

			if (bits & 1)
			{
				bits = (bits & ~1) | 2;
			}

			std::swap(from, to);
		}

		return true;
	}

	bool Waypoints_SetLinkFlags(std::vector<WaypointRec> *list, int32_t a, int32_t b, int32_t flags)
	{
		if (!list || a < 0 || b < 0 || a == b || (size_t)a >= list->size() || (size_t)b >= list->size())
		{
			return false;
		}

		int32_t const mine = (flags & 1) && (flags & 2) ? flags & ~3 : flags;
		int32_t const back = (mine & ~3) | ((mine & 1) ? 2 : 0) | ((mine & 2) ? 1 : 0);
		int32_t const ends[2][3] = { { a, b, mine }, { b, a, back } };

		for (auto const &end : ends)
		{
			WaypointRec &wp = (*list)[(size_t)end[0]];
			int const at = FindLink(wp.links, end[1]);

			if (at >= 0)
			{
				wp.links[(size_t)at].flags = end[2];
			}
			else
			{
				WaypointLinkRec link;

				link.index = end[1];
				link.flags = end[2];
				wp.links.push_back(link);
			}
		}

		return true;
	}

	bool Waypoints_DeleteLink(std::vector<WaypointRec> *list, int32_t a, int32_t b)
	{
		if (!list || a < 0 || b < 0 || (size_t)a >= list->size() || (size_t)b >= list->size())
		{
			return false;
		}

		bool any = false;
		int32_t from = a, to = b;

		// FUN_00586d00 loops with the two swapped until the link is not found
		for (int pass = 0; pass < 2; pass++)
		{
			WaypointRec &wp = (*list)[(size_t)from];
			int const at = FindLink(wp.links, to);

			if (at < 0)
			{
				break;
			}

			wp.links.erase(wp.links.begin() + at);
			any = true;
			std::swap(from, to);
		}

		return any;
	}

	bool Waypoints_Delete(std::vector<WaypointRec> *list, int32_t at)
	{
		if (!list || at < 0 || (size_t)at >= list->size())
		{
			return false;
		}

		list->erase(list->begin() + at);

		for (WaypointRec &wp : *list)
		{
			std::vector<WaypointLinkRec> kept;

			for (WaypointLinkRec link : wp.links)
			{
				if (link.index == at)
				{
					continue;
				}

				if (link.index > at)
				{
					link.index--;
				}

				kept.push_back(link);
			}

			wp.links = kept;
		}

		return true;
	}

	int32_t Waypoints_Create(std::vector<WaypointRec> *list, int32_t const place[3])
	{
		WaypointRec wp;

		for (int c = 0; c < 3; c++)
		{
			wp.min[c] = place[c] - 500;
			wp.max[c] = place[c] + 500;
			wp.centre[c] = place[c];
		}

		wp.flags = 0;
		wp.spare2 = 0;
		list->push_back(wp);
		return (int32_t)list->size() - 1;
	}

	bool Waypoints_Transfer(WaypointSet *into, WaypointSet const &from)
	{
		if (from.alien.empty() && from.ground.empty())
		{
			return false;
		}

		int32_t const alienBase = (int32_t)into->alien.size();
		int32_t const groundBase = (int32_t)into->ground.size();

		for (WaypointRec wp : from.alien)
		{
			for (WaypointLinkRec &link : wp.links)
			{
				link.index += alienBase;
			}

			into->alien.push_back(wp);
		}

		for (WaypointRec wp : from.ground)
		{
			for (WaypointLinkRec &link : wp.links)
			{
				link.index += groundBase;
			}

			into->ground.push_back(wp);
		}

		return true;
	}
}
