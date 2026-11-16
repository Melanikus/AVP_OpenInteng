// topology_match.h: the matching of two patches of polygons by how they are joined, for the Texture Map's Copy Texturing > From Multiple / To Multiple, T ("Copy polygons based on topology"). Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// READ FROM inteng.exe (FUN_00576cb0, with FUN_00575fe0, FUN_00577930, FUN_00577d60): each polygon of a patch is a NODE with its count of corners (3 or 4) and, for each of its edges (edge k runs from corner k to corner k+1), the node of the same patch that shares the edge (the two edges
// coincide, run the opposite ways), or -1. Two patches are matched from a first pair of nodes, one of each (the "reference polygon 1" of each), and a rotation of the second one's edges: the walk goes through the matched pairs in the order they were matched; for each edge k of the first's node it takes the neighbour across it and the second's
// neighbour across edge (rotation + k) mod count, and when both exist, neither is matched yet and they have the same count of corners, they are matched with the rotation that makes the edge they share with the pair they came from line up (the count minus the edge of the first
// that points back, plus the edge of the second that points back, mod the count). A rotation counts only when the "reference polygon 2" of each patch were matched with each other; the one that matches the most pairs is used. inteng tries it twice, the second time with the second patch MIRRORED
// (its corners taken in the opposite order and its edges numbered the opposite way), and takes the better one (the first when they are equal).
#pragma once

#include <stdint.h>

#include <vector>

namespace model
{
	int const kTopologyMostEdges = 4;

	struct TopologyNode
	{
		int corners;                         // 3 or 4
		int neighbour[kTopologyMostEdges];   // for each edge: the node of the same patch across it, or -1
	};

	struct TopologyPair
	{
		int from;       // a node of the first patch
		int to;         // the node of the second patch it was matched with
		int rotation;   // the second's edge rotation for that pair
	};

	struct TopologyMatch
	{
		bool found;
		bool mirrored;                       // the pass that matched best was the second one (the second patch mirrored)
		int rotation;                        // the rotation of the second reference node that did
		std::vector<TopologyPair> pairs;     // in the order they were matched; pairs[0] is the two reference nodes
	};

	// first: the first patch; second and secondMirrored: the second patch's nodes made the usual way and the mirrored way (the same nodes, the corners and edges in the opposite order); firstReference1 / 2 and secondReference1 / 2 are node numbers.
	// A pair of references with different counts of corners, or a number that is no node, finds nothing.
	TopologyMatch MatchByTopology(std::vector<TopologyNode> const &first, int firstReference1, int firstReference2, std::vector<TopologyNode> const &second, std::vector<TopologyNode> const &secondMirrored, int secondReference1, int secondReference2);

	// The corner of the source polygon the destination's corner k takes its texture coordinates from, with the pair's rotation (FUN_00575da0 for two triangles or two quads: the two do NOT turn the same way, as inteng's code has it): the corner count is 3 or 4; mirrored takes them the other way round
	int TopologySourceCorner(int corners, int rotation, bool mirrored, int k);
}
