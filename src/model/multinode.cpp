// multinode.cpp: the Texture Map's multiple node mode, its pure maths (see multinode.h). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng (TM-B).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "multinode.h"

#include <algorithm>

namespace model
{
	namespace
	{
		bool Within(int32_t a, int32_t b, int32_t reach)
		{
			int64_t const d = (int64_t)a - b;

			return d >= -(int64_t)reach && d <= (int64_t)reach;
		}

		void AddLink(std::vector<MultiNode> *nodes, int32_t from, int32_t to)
		{
			if (from < 0 || to < 0 || to <= from)
			{
				return;   // FUN_00560180 is called only for a higher-numbered other node; a corner no node holds joins nothing
			}

			std::vector<int32_t> &links = (*nodes)[(size_t)from].links;

			if (std::find(links.begin(), links.end(), to) == links.end())
			{
				links.push_back(to);
			}
		}

		bool NearAWhole(int64_t d, int32_t size)
		{
			int64_t const r = d % size;

			return r == 0 || r == 1 || r == size - 1;
		}
	}

	std::vector<MultiNode> MultiNodeJoin(std::vector<MultiNodePolygon> const &polygons, int32_t reach)
	{
		std::vector<MultiNode> nodes;

		for (size_t p = 0; p < polygons.size(); p++)
		{
			MultiNodePolygon const &polygon = polygons[p];

			if (!polygon.selected)
			{
				continue;
			}

			for (int c = 0; c < polygon.corners && c < 4; c++)
			{
				MultiNodeMember const member = { (int32_t)p, c };
				bool joined = false;

				for (MultiNode &node : nodes)
				{
					if (Within(polygon.x[c], node.x, reach) && Within(polygon.y[c], node.y, reach) && Within(polygon.z[c], node.z, reach))
					{
						node.members.push_back(member);
						joined = true;
						break;
					}
				}

				if (!joined)
				{
					MultiNode made = {};

					made.x = polygon.x[c];
					made.y = polygon.y[c];
					made.z = polygon.z[c];
					made.members.push_back(member);
					nodes.push_back(made);
				}
			}
		}

		return nodes;
	}

	int32_t MultiNodeOf(std::vector<MultiNode> const &nodes, int32_t polygon, int corner)
	{
		for (size_t n = 0; n < nodes.size(); n++)
		{
			for (MultiNodeMember const &member : nodes[n].members)
			{
				if (member.polygon == polygon && member.corner == corner)
				{
					return (int32_t)n;
				}
			}
		}

		return -1;
	}

	void MultiNodeLink(std::vector<MultiNode> *nodes, std::vector<MultiNodePolygon> const &polygons)
	{
		for (size_t n = 0; n < nodes->size(); n++)
		{
			std::vector<MultiNodeMember> const members = (*nodes)[n].members;

			for (MultiNodeMember const &member : members)
			{
				MultiNodePolygon const &polygon = polygons[(size_t)member.polygon];

				if (polygon.corners == 4)
				{
					AddLink(nodes, (int32_t)n, MultiNodeOf(*nodes, member.polygon, (member.corner + 1) % 4));
					AddLink(nodes, (int32_t)n, MultiNodeOf(*nodes, member.polygon, (member.corner + 3) % 4));
					continue;
				}

				if (polygon.partner < 0 || !polygon.quadKnown)
				{
					AddLink(nodes, (int32_t)n, MultiNodeOf(*nodes, member.polygon, (member.corner + 1) % 3));
					AddLink(nodes, (int32_t)n, MultiNodeOf(*nodes, member.polygon, (member.corner + 2) % 3));
					continue;
				}

				int place = -1;

				for (int q = 0; q < 4; q++)
				{
					if (polygon.quad.ownCorners[q] == member.corner)
					{
						place = q;
					}
				}

				if (place < 0)
				{
					continue;
				}

				for (int step : { 1, 3 })
				{
					int const q = (place + step) & 3;
					int32_t const other = polygon.quad.ownCorners[q] >= 0 ? MultiNodeOf(*nodes, member.polygon, polygon.quad.ownCorners[q])
						: polygon.quad.partnerCorners[q] >= 0 ? MultiNodeOf(*nodes, polygon.partner, polygon.quad.partnerCorners[q]) : -1;

					AddLink(nodes, (int32_t)n, other);
				}
			}
		}
	}

	bool MultiNodeLineUp(std::vector<MultiNode> *nodes, std::vector<MultiNodePolygon> *polygons, int32_t width, int32_t height)
	{
		if (width < 1 || height < 1)
		{
			return false;
		}

		std::vector<MultiNode> workNodes = *nodes;
		std::vector<MultiNodePolygon> work = *polygons;
		std::vector<bool> done(workNodes.size(), false);

		for (int pass = 1; pass <= kMultiNodeMostPasses; pass++)
		{
			bool moved = false;

			for (size_t n = 0; n < workNodes.size(); n++)
			{
				if (done[n])
				{
					continue;
				}

				MultiNode &node = workNodes[n];
				int32_t maxU = -1000000, maxV = -1000000;

				for (MultiNodeMember const &member : node.members)
				{
					MultiNodePolygon const &polygon = work[(size_t)member.polygon];

					maxU = maxU <= polygon.u[member.corner] ? polygon.u[member.corner] : maxU;
					maxV = maxV <= polygon.v[member.corner] ? polygon.v[member.corner] : maxV;
				}

				if (node.members.size() > 1)
				{
					for (MultiNodeMember const &member : node.members)
					{
						MultiNodePolygon &polygon = work[(size_t)member.polygon];
						int64_t const du = (int64_t)maxU - polygon.u[member.corner], dv = (int64_t)maxV - polygon.v[member.corner];

						if (!NearAWhole(du, width) || !NearAWhole(dv, height))
						{
							continue;
						}

						int64_t const su = ((du + 1) / width) * width, sv = ((dv + 1) / height) * height;

						if (su == 0 && sv == 0)
						{
							continue;
						}

						moved = true;

						for (int32_t const which : { member.polygon, polygon.partner })
						{
							if (which < 0)
							{
								continue;
							}

							for (int k = 0; k < work[(size_t)which].corners && k < 4; k++)
							{
								work[(size_t)which].u[k] = (int32_t)(work[(size_t)which].u[k] + su);
								work[(size_t)which].v[k] = (int32_t)(work[(size_t)which].v[k] + sv);

								int32_t const owner = MultiNodeOf(workNodes, which, k);

								if (owner >= 0)
								{
									done[(size_t)owner] = false;
								}
							}
						}
					}
				}

				node.u = maxU;
				node.v = maxV;
				done[n] = true;   // 0x574659: done even when this pass's moves cleared it (bug candidate 12, kept: the node's (u, v) is made again on the pass after)
			}

			if (!moved)
			{
				*nodes = workNodes;
				*polygons = work;
				return true;
			}
		}

		return false;
	}

	std::vector<int32_t> MultiNodeWriteBack(std::vector<MultiNode> const &nodes, std::vector<MultiNodePolygon> *polygons)
	{
		std::vector<int32_t> changed;

		for (MultiNode const &node : nodes)
		{
			for (MultiNodeMember const &member : node.members)
			{
				MultiNodePolygon &polygon = (*polygons)[(size_t)member.polygon];

				if (polygon.u[member.corner] != node.u || polygon.v[member.corner] != node.v)
				{
					polygon.u[member.corner] = node.u;
					polygon.v[member.corner] = node.v;

					if (std::find(changed.begin(), changed.end(), member.polygon) == changed.end())
					{
						changed.push_back(member.polygon);
					}
				}
			}
		}

		return changed;
	}
}
