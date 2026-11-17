// The test of AutoLink's driver (src/host/autolink_run.cpp) against inteng's FUN_004c8930 / FUN_004c7ab0 / FUN_004c6fb0 (rows ML-A-1 and ML-A-7 of
// AvP-docs\engine\inteng\reassess\module-linking-map-dietrich.md, read on listing-all-2). No level, no device: the records are boxes and the "picture" is
// scripted from the shown flags, as a renderer would draw them. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// THE SCENE: record 0 = A and 1 = B, two 40 m rooms touching on x (they meet: one portal); 2 = D (a door), 3 = E (behind D), 4 = D2 (a door behind D), 5 = G (behind
// D2): far away, they meet nothing; 6 = D2's stand-in, 7 = D's stand-in (the stand-ins follow the modules, last door first). The picture of ANY view shows B; with D's
// stand-in drawn it shows that stand-in (hiding all behind it); with it hidden, D (when drawn), E, and D2's stand-in, or with that hidden, D2 (when drawn) and G.

#include <stdio.h>

#include <string>
#include <vector>

#include "autolink_run.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A box over [0, w] x [0, h] x [0, d] (local), as FUN_00447f10's stand-in box: point i at (bit 0 ? w : 0, bit 1 ? h : 0, bit 2 ? d : 0), 6 outward quads -x +x -y +y -z +z;
// `open` leaves one face out (0 = -x, 1 = +x; -1 none): a room's wall where it meets the next room is an opening, not a wall on the contact plane
static host::PortalShape Box(int32_t w, int32_t h, int32_t d, int open)
{
	host::PortalShape box;
	std::vector<std::vector<int32_t> > const faces = { { 0, 4, 6, 2 }, { 1, 3, 7, 5 }, { 4, 0, 1, 5 }, { 2, 6, 7, 3 }, { 0, 2, 3, 1 }, { 4, 5, 7, 6 } };
	int32_t const normals[6][3] = { { -65536, 0, 0 }, { 65536, 0, 0 }, { 0, -65536, 0 }, { 0, 65536, 0 }, { 0, 0, -65536 }, { 0, 0, 65536 } };

	for (int i = 0; i < 8; i++)
	{
		box.points.push_back(i & 1 ? w : 0);
		box.points.push_back(i & 2 ? h : 0);
		box.points.push_back(i & 4 ? d : 0);
	}

	for (int f = 0; f < 6; f++)
	{
		if (f != open)
		{
			box.polygons.push_back(faces[(size_t)f]);
			box.normals.insert(box.normals.end(), normals[f], normals[f] + 3);
		}
	}

	return box;
}

static host::PortalModule At(host::PortalShape const *shape, int32_t x, int32_t y, int32_t z, int32_t w, int32_t h, int32_t d)
{
	host::PortalModule one;

	one.place[0] = x;
	one.place[1] = y;
	one.place[2] = z;
	one.extents[0] = w;
	one.extents[1] = 0;
	one.extents[2] = h;
	one.extents[3] = 0;
	one.extents[4] = d;
	one.extents[5] = 0;
	one.shape = shape;
	return one;
}

// A 320 x 240 picture with a 10 x 10 block of each ID given (ID = record + 1), side by side
static void Draw(std::vector<int32_t> const &records, std::vector<uint32_t> *pixels)
{
	pixels->assign((size_t)(320 * 240), 0u);

	for (size_t n = 0; n < records.size(); n++)
	{
		for (int r = 100; r < 110; r++)
		{
			for (int c = 0; c < 10; c++)
			{
				(*pixels)[(size_t)(r * 320 + 20 + (int)n * 20 + c)] = (uint32_t)(records[n] + 1);
			}
		}
	}
}

static std::string Words(std::vector<host::AutoLinkEvent> const &events, size_t from, size_t count)
{
	std::string out;

	for (size_t e = from; e < from + count && e < events.size(); e++)
	{
		host::AutoLinkEvent const &one = events[e];

		out += out.empty() ? "" : " ";

		if (one.end)
		{
			out += "end(" + std::to_string(one.door) + ")";
			continue;
		}

		out += "pic(" + std::to_string(one.door) + (one.badPixel ? ",bad" : "") + ":";

		for (size_t i = 0; i < one.insert.size(); i++)
		{
			out += (i ? "," : "") + std::to_string(one.insert[i]);
		}

		out += ")";
	}

	return out;
}

static std::string Items(host::LinkTree const &tree)
{
	std::string out;

	for (host::LinkItem const &item : tree.Flatten())
	{
		out += (out.empty() ? "" : " ") + std::to_string(item.objectIndex) + "/" + std::to_string(item.branchNo);
	}

	return out;
}

int main()
{
	int32_t const room = 4000000;   // 40 m in inteng's units (x 100 per millimetre)
	host::PortalShape const roomA = Box(room, room, room, 1);   // open on +x
	host::PortalShape const roomB = Box(room, room, room, 0);   // open on -x
	host::PortalShape const smallShape = Box(100000, 100000, 100000, -1);
	std::vector<host::PortalModule> records;

	records.push_back(At(&roomA, 0, 0, 0, room, room, room));          // 0 A
	records.push_back(At(&roomB, room, 0, 0, room, room, room));       // 1 B: touches A on x
	for (int n = 0; n < 4; n++)
	{
		records.push_back(At(&smallShape, 400000000 + n * 1000000, 0, 0, 100000, 100000, 100000));   // 2 D, 3 E, 4 D2, 5 G: far away
	}
	records.push_back(At(&smallShape, 400000000 + 2 * 1000000, 0, 0, 100001, 100001, 100001));      // 6 D2's stand-in
	records.push_back(At(&smallShape, 400000000, 0, 0, 100001, 100001, 100001));                    // 7 D's stand-in

	std::vector<int32_t> const standsFor = { -1, -1, -1, -1, -1, -1, 4, 2 };
	std::vector<int32_t> const objectIndexOf = { 10, 11, 12, 13, 14, 15, 14, 12 };   // the stand-ins: their doors' objects

	host::Portal portal;

	Check(host::AutoLink_ModulesMeet(records[0], records[1], &portal), "the scene: A and B meet");
	Check(!host::AutoLink_ModulesMeet(records[0], records[2], &portal), "the scene: A and D do not");

	std::vector<std::vector<uint8_t> > shownSeen;
	host::AutoLinkPictureMaker const picture = [&shownSeen](host::AutoLinkView const &, std::vector<uint8_t> const &shown, std::vector<uint32_t> *pixels, std::string *)
	{
		std::vector<int32_t> drawn = { 1 };

		shownSeen.push_back(shown);

		if (shown[7])
		{
			drawn.push_back(7);
		}
		else
		{
			if (shown[2])
			{
				drawn.push_back(2);
			}

			drawn.push_back(3);

			if (shown[6])
			{
				drawn.push_back(6);
			}
			else
			{
				if (shown[4])
				{
					drawn.push_back(4);
				}

				drawn.push_back(5);
			}
		}

		Draw(drawn, pixels);
		return true;
	};

	// ---- A's work: every view of the portal A-B makes three pictures, nested: the view, D's second picture, and inside it D2's
	{
		std::vector<uint8_t> shown(records.size(), 1);
		std::vector<host::AutoLinkEvent> events;
		size_t pictures = 0;
		std::string why;
		std::vector<host::AutoLinkView> views;

		host::AutoLink_ModulesMeet(records[0], records[1], &portal);
		host::AutoLink_ViewsFor(portal, &views);
		shownSeen.clear();

		Check(host::AutoLink_ModuleWork(records, standsFor, 0, 0, &shown, picture, &events, &pictures, &why), "A's work is done: " + why);
		Check(!views.empty() && pictures == 3 * views.size(), "3 pictures per view (" + std::to_string(views.size()) + " views): " + std::to_string(pictures));
		// the view sees B and D's stand-in (7 -> its door 2); D's picture (door 2) sees B, D, E and D2's stand-in (6 -> 4); D2's (door 4), inside it, sees B, D, E, D2 and G
		Check(Words(events, 0, 6) == "pic(-1:1,2) pic(2:1,2,3,4) pic(4:1,2,3,4,5) end(4) end(2) end(-1)", "the first view's events, nested: " + Words(events, 0, 6));
		Check(shownSeen.size() >= 3 && shownSeen[1][7] == 0 && shownSeen[1][2] == 1 && shownSeen[2][6] == 0 && shownSeen[2][4] == 1 && shownSeen[2][7] == 0,
			"each door picture is drawn with the stand-in hidden and the door shown (0x4c770c / 0x4c7775), the outer door's swap kept inside");
		// after a door step the stand-in is drawn and the DOOR HIDDEN (0x4c78b3 AND ~4, 0x4c7910 OR 4), whatever it was before: from then on the stand-in is what the pictures show
		std::vector<uint8_t> after(records.size(), 1);

		after[2] = 0;
		after[4] = 0;
		Check(shown == after, "after A's work the doors met (D, D2) are hidden and their stand-ins drawn");
		Check(shownSeen.size() >= 4 && shownSeen[3][2] == 0 && shownSeen[3][7] == 1, "so the second view is drawn with D hidden and its stand-in drawn");

		// the frame: A's tree
		host::LinkTree tree(10);
		host::AutoLinkTreeWriter writer;
		size_t added = 0;
		bool applied = true;

		for (host::AutoLinkEvent const &one : events)
		{
			applied = writer.Apply(&tree, one, objectIndexOf, &added) && applied;
		}

		Check(applied && writer.Balanced(), "every event applies and every picture ends");
		// B then D after A; E becomes D's branch (Open), D2 after E inside it; G becomes D2's branch; D2 closed, then D (FUN_004c46c0's numbers: D at place 2 with 3 under it)
		Check(Items(tree) == "11/0 12/5 13/0 14/5 15/0", "A -> B -> D -> [E -> D2 -> [G]]: " + Items(tree));
		Check(added == 5, "five nodes made, the later views adding nothing: " + std::to_string(added));
	}

	// ---- D's own work (a door): its stand-in hidden and D drawn while it runs, then D hidden and its stand-in drawn, and they STAY so (0x4c8a33-0x4c8a91)
	{
		std::vector<uint8_t> shown(records.size(), 1);
		std::vector<host::AutoLinkEvent> events;
		size_t pictures = 0;
		std::string why;

		Check(host::AutoLink_ModuleWork(records, standsFor, 2, 0, &shown, picture, &events, &pictures, &why), "D's work is done (it meets nothing: no picture)");
		Check(pictures == 0 && shown[2] == 0 && shown[7] == 1, "after D's work, D is hidden and its stand-in drawn");
	}

	// ---- LOAD FROM FILE's work for one record (FUN_004c8be0 0x4c8f25-0x4c905a): ONE picture from the record's view, with the same door steps; a door module's own flags as the driver's
	{
		std::vector<uint8_t> shown(records.size(), 1);
		std::vector<host::AutoLinkEvent> events;
		size_t pictures = 0;
		std::vector<host::AutoLinkView> views;

		host::AutoLink_ModulesMeet(records[0], records[1], &portal);
		host::AutoLink_ViewsFor(portal, &views);
		shownSeen.clear();

		Check(!views.empty() && host::AutoLink_ViewWork(records, standsFor, 0, views[0], 0, &shown, picture, &events, &pictures, nullptr) && pictures == 3,
			"one view: its picture and the two door pictures nested in it: " + std::to_string(pictures));
		Check(Words(events, 0, 6) == "pic(-1:1,2) pic(2:1,2,3,4) pic(4:1,2,3,4,5) end(4) end(2) end(-1)", "the same events as AutoLink's first view: " + Words(events, 0, 6));

		std::vector<uint8_t> door(records.size(), 1);

		events.clear();
		shownSeen.clear();
		host::AutoLink_ViewWork(records, standsFor, 2, views[0], 0, &door, picture, &events, &pictures, nullptr);
		Check(!shownSeen.empty() && shownSeen[0][7] == 0 && shownSeen[0][2] == 1 && door[2] == 0 && door[7] == 1,
			"a door's record: its stand-in hidden while its view is drawn, then the door hidden and the stand-in drawn (0x4c8f48 / 0x4c904a)");
	}

	// ---- THE DISTANCE CUT (0x4c7317-0x4c73d2): by the places, signed, only with a distance
	{
		host::PortalModule a = At(&smallShape, 0, 0, 0, 1000, 1000, 1000);
		host::PortalModule b = At(&smallShape, 5000, 0, 0, 1000, 1000, 1000);   // gap on x: (5000 + 0) - (0 + 1000) = 4000

		Check(!host::AutoLink_DistanceCut(a, b, 4000) && host::AutoLink_DistanceCut(a, b, 3999), "a gap of 4000 is kept at 4000, cut at 3999");

		// lopsided: d's PLACE is below c's but its box (6000..7000) lies above c's (5000..6000). inteng takes the gap the places pick: (c.place + c.least) - (d.place + d.most)
		// = 5000 - 7000 = -2000; the other one, (d.place + d.least) - (c.place + c.most) = 6000 - 6000 = 0, is NOT used (ours before 2026-11-16 took the larger: 0). y and z overlap
		// deeply (-100000), so x decides.
		host::PortalModule c = At(&smallShape, 5000, 0, 0, 1000, 100000, 100000);
		host::PortalModule d = At(&smallShape, 0, 0, 0, 7000, 100000, 100000);

		d.extents[1] = 6000;   // d's least x

		Check(!host::AutoLink_DistanceCut(c, d, -1500) && host::AutoLink_DistanceCut(c, d, -2001), "the places pick the gap: -2000 on x (kept at -1500, cut at -2001)");

		// a NEGATIVE distance cuts even touching boxes (inteng cuts whenever the distance is not 0)
		host::PortalModule e = At(&smallShape, 1000, 0, 0, 1000, 1000, 1000);   // touches a: gaps 0, -1000, -1000

		Check(host::AutoLink_DistanceCut(a, e, -1), "a negative distance cuts a touching box");
	}

	// ---- the cut in a picture: B kept, the far stand-in cut when the distance is 1 m (100000)
	{
		std::vector<uint8_t> shown(records.size(), 1);
		std::vector<host::AutoLinkEvent> events;
		size_t pictures = 0;

		host::AutoLink_ModuleWork(records, standsFor, 0, 100000, &shown, picture, &events, &pictures, nullptr);
		Check(Words(events, 0, 2) == "pic(-1:1) end(-1)", "with a distance, the far stand-in is cut: no door step: " + Words(events, 0, 2));
	}

	// ---- a BAD PIXEL (an ID above the record count): the picture puts nothing in and makes no door step
	{
		std::vector<uint8_t> shown(records.size(), 1);
		std::vector<host::AutoLinkEvent> events;
		size_t pictures = 0;
		host::AutoLinkPictureMaker const bad = [](host::AutoLinkView const &, std::vector<uint8_t> const &, std::vector<uint32_t> *pixels, std::string *)
		{
			Draw({ 1, 7, 40 }, pixels);   // 41 > 8 records
			return true;
		};

		host::AutoLink_ModuleWork(records, standsFor, 0, 0, &shown, bad, &events, &pictures, nullptr);
		Check(Words(events, 0, 2) == "pic(-1,bad:) end(-1)", "a bad pixel: nothing put in, no door step: " + Words(events, 0, 2));
	}

	// ---- INTENG BUG, FIXED (MAPPING-ORDERS section 15): a bad pixel in a DOOR picture. inteng re-opens the door's old branch (0x4c7023) and then leaves at 0x4c729a, before the Close
	// (0x4c7952): the branch stays open. The writer closes it, as every door picture that reused a branch ends. Tree: R(10) -> 11 -> 12 [13], closed; door record 2 is object 12.
	{
		host::LinkTree tree(10);
		host::AutoLinkTreeWriter writer;
		size_t added = 0;

		tree.Normal(11);
		tree.Normal(12);
		tree.Branch(13);
		tree.Close(12);
		writer.Apply(&tree, host::AutoLinkEvent{ 2, false, true, {} }, objectIndexOf, &added);
		writer.Apply(&tree, host::AutoLinkEvent{ 2, true, true, {} }, objectIndexOf, &added);

		bool red = false;

		for (host::LinkTree::Tint const &one : tree.Tints())
		{
			red = red || (one.module == 12 && one.rgb == 0xFF0000);
		}

		Check(red && writer.Balanced() && added == 0, "a door picture with a bad pixel adds nothing and leaves the door's branch CLOSED (red), not open");
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
