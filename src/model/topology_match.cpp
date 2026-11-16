// topology_match.cpp: see topology_match.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "topology_match.h"

namespace model
{
	namespace
	{
		bool NodeIsGood(TopologyNode const &node, size_t count)
		{
			if (node.corners < 3 || node.corners > kTopologyMostEdges)
			{
				return false;
			}

			for (int k = 0; k < node.corners; k++)
			{
				if (node.neighbour[k] < -1 || node.neighbour[k] >= (int)count)
				{
					return false;
				}
			}

			return true;
		}

		// The edge of `node` that points back to `target`, or -1
		int EdgeTo(TopologyNode const &node, int target)
		{
			for (int k = 0; k < node.corners; k++)
			{
				if (node.neighbour[k] == target)
				{
					return k;
				}
			}

			return -1;
		}

		// One walk from the reference pair with the second reference's rotation (FUN_00576cb0 0x5770bb-0x57751b)
		TopologyMatch Walk(std::vector<TopologyNode> const &first, int first1, std::vector<TopologyNode> const &second, int second1, int rotation)
		{
			TopologyMatch made;

			made.found = true;
			made.mirrored = false;
			made.rotation = rotation;

			std::vector<char> firstDone(first.size(), 0), secondDone(second.size(), 0);
			std::vector<int> rotationOf(second.size(), 0);
			TopologyPair start = { first1, second1, rotation % second[(size_t)second1].corners };

			made.pairs.push_back(start);
			firstDone[(size_t)first1] = 1;
			secondDone[(size_t)second1] = 1;
			rotationOf[(size_t)second1] = start.rotation;

			for (size_t i = 0; i < made.pairs.size(); i++)
			{
				TopologyNode const &a = first[(size_t)made.pairs[i].from];
				TopologyNode const &b = second[(size_t)made.pairs[i].to];

				for (int k = 0; k < a.corners; k++)
				{
					int const na = a.neighbour[k];
					int const nb = b.neighbour[(rotationOf[(size_t)made.pairs[i].to] + k) % b.corners];

					if (na < 0 || nb < 0 || firstDone[(size_t)na] || secondDone[(size_t)nb] || first[(size_t)na].corners != second[(size_t)nb].corners)
					{
						continue;
					}

					int const backA = EdgeTo(first[(size_t)na], made.pairs[i].from);
					int const backB = EdgeTo(second[(size_t)nb], made.pairs[i].to);

					if (backA < 0 || backB < 0)
					{
						continue;
					}

					int const n = second[(size_t)nb].corners;
					TopologyPair pair = { na, nb, ((n - backA) + backB) % n };

					rotationOf[(size_t)nb] = pair.rotation;
					firstDone[(size_t)na] = 1;
					secondDone[(size_t)nb] = 1;
					made.pairs.push_back(pair);
				}
			}

			return made;
		}

		bool PairsHave(TopologyMatch const &match, int a, int b)
		{
			for (TopologyPair const &pair : match.pairs)
			{
				if (pair.from == a && pair.to == b)
				{
					return true;
				}
			}

			return false;
		}
	}

	TopologyMatch MatchByTopology(std::vector<TopologyNode> const &first, int firstReference1, int firstReference2, std::vector<TopologyNode> const &second, std::vector<TopologyNode> const &secondMirrored, int secondReference1, int secondReference2)
	{
		TopologyMatch best;

		best.found = false;
		best.mirrored = false;
		best.rotation = 0;

		if (second.size() != secondMirrored.size() || firstReference1 < 0 || firstReference2 < 0 || secondReference1 < 0 || secondReference2 < 0 || (size_t)firstReference1 >= first.size() || (size_t)firstReference2 >= first.size() ||
			(size_t)secondReference1 >= second.size() || (size_t)secondReference2 >= second.size())
		{
			return best;
		}

		for (TopologyNode const &node : first)
		{
			if (!NodeIsGood(node, first.size()))
			{
				return best;
			}
		}

		for (size_t i = 0; i < second.size(); i++)
		{
			if (!NodeIsGood(second[i], second.size()) || !NodeIsGood(secondMirrored[i], second.size()))
			{
				return best;
			}
		}

		if (first[(size_t)firstReference1].corners != second[(size_t)secondReference1].corners)
		{
			return best;
		}

		size_t bestCount = 0;

		for (int pass = 0; pass < 2; pass++)
		{
			std::vector<TopologyNode> const &used = pass == 0 ? second : secondMirrored;

			for (int rotation = 0; rotation < used[(size_t)secondReference1].corners; rotation++)
			{
				TopologyMatch const tried = Walk(first, firstReference1, used, secondReference1, rotation);

				if (PairsHave(tried, firstReference2, secondReference2) && tried.pairs.size() > bestCount)
				{
					bestCount = tried.pairs.size();
					best = tried;
					best.found = true;
					best.mirrored = pass == 1;
					best.rotation = rotation;
				}
			}
		}

		return best;
	}

	int TopologySourceCorner(int corners, int rotation, bool mirrored, int k)
	{
		if (corners < 3 || corners > 4 || k < 0 || k >= corners)
		{
			return 0;
		}

		int const turn = ((rotation % corners) + corners) % corners;

		// FUN_00575da0: two triangles take source corner (rotation + k) mod 3, mirrored (rotation + 5 - k) mod 3 (0x575e27-0x575e5c); two quads (a merged pair) (k - rotation) mod 4, mirrored (7 - k - rotation) mod 4 (0x575f01-0x575f2f)
		if (corners == 3)
		{
			return mirrored ? (turn + 5 - k) % 3 : (turn + k) % 3;
		}

		return mirrored ? (((7 - k - turn) % 4) + 4) % 4 : (k - turn + 4) % 4;
	}
}
