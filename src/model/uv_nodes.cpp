// uv_nodes.cpp: cycle and reverse a polygon's nodes (uv_nodes.h says what and why). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "uv_nodes.h"

namespace model
{
	namespace
	{
		// a node store: count nodes of (u, v), read and written by place
		template <typename Node>
		void TurnAlone(Node *nodes, int count, NodeTurn turn)
		{
			if (!nodes || (count != 3 && count != 4))
			{
				return;
			}

			if (turn == NodeTurn::Cycle)
			{
				Node const first = nodes[0];

				for (int k = 0; k + 1 < count; k++)
				{
					nodes[k] = nodes[k + 1];
				}

				nodes[count - 1] = first;
				return;
			}

			if (count == 3)
			{
				Node const one = nodes[1];   // a triangle: 1 and 2 swap, 0 stays

				nodes[1] = nodes[2];
				nodes[2] = one;
				return;
			}

			Node const zero = nodes[0], one = nodes[1];   // a file quad as inteng's quad: 0 <-> 3, 1 <-> 2

			nodes[0] = nodes[3];
			nodes[3] = zero;
			nodes[1] = nodes[2];
			nodes[2] = one;
		}

		bool Has(int32_t place)
		{
			return place >= 0 && place < 3;
		}

		template <typename Node>
		Node Get(TextureQuad const &quad, int k, Node const *own, Node const *partner)
		{
			return Has(quad.ownCorners[k]) ? own[quad.ownCorners[k]] : partner[Has(quad.partnerCorners[k]) ? quad.partnerCorners[k] : 0];
		}

		template <typename Node>
		void Set(TextureQuad const &quad, int k, Node *own, Node *partner, Node const &value)
		{
			if (Has(quad.ownCorners[k]))
			{
				own[quad.ownCorners[k]] = value;
			}

			if (Has(quad.partnerCorners[k]))
			{
				partner[quad.partnerCorners[k]] = value;
			}
		}

		// inteng's swap of two corners (0x571b24-...): the near corner read own-first, the far one from the polygon then the partner (the partner's wins), the far set to the near's, the near
		// to the far's
		template <typename Node>
		void Swap(TextureQuad const &quad, int nearCorner, int farCorner, Node *own, Node *partner)
		{
			Node const nearValue = Get(quad, nearCorner, own, partner);
			Node farValue = nearValue;

			if (Has(quad.ownCorners[farCorner]))
			{
				farValue = own[quad.ownCorners[farCorner]];
			}

			if (Has(quad.partnerCorners[farCorner]))
			{
				farValue = partner[quad.partnerCorners[farCorner]];
			}

			Set(quad, farCorner, own, partner, nearValue);
			Set(quad, nearCorner, own, partner, farValue);
		}

		template <typename Node>
		void TurnQuad(TextureQuad const &quad, Node *own, Node *partner, NodeTurn turn)
		{
			if (!own || !partner)
			{
				return;
			}

			for (int k = 0; k < 4; k++)
			{
				if (!Has(quad.ownCorners[k]) && !Has(quad.partnerCorners[k]))
				{
					return;   // a corner neither holds: not a quad of the two
				}
			}

			if (turn == NodeTurn::Cycle)
			{
				Node const first = Get(quad, 0, own, partner);

				for (int k = 0; k < 3; k++)
				{
					Set(quad, k, own, partner, Get(quad, k + 1, own, partner));
				}

				Set(quad, 3, own, partner, first);
				return;
			}

			Swap(quad, 0, 3, own, partner);
			Swap(quad, 1, 2, own, partner);
		}

		struct FrameNode
		{
			int32_t u, v;
		};
	}

	void UvNodes_Alone(UvCorner *nodes, int count, NodeTurn turn)
	{
		TurnAlone(nodes, count, turn);
	}

	void UvNodes_Quad(TextureQuad const &quad, UvCorner *own, UvCorner *partner, NodeTurn turn)
	{
		TurnQuad(quad, own, partner, turn);
	}

	void UvNodes_FrameAlone(int32_t *six, NodeTurn turn)
	{
		if (!six)
		{
			return;
		}

		FrameNode nodes[3] = { { six[0], six[1] }, { six[2], six[3] }, { six[4], six[5] } };

		TurnAlone(nodes, 3, turn);

		for (int k = 0; k < 3; k++)
		{
			six[2 * k] = nodes[k].u;
			six[2 * k + 1] = nodes[k].v;
		}
	}

	void UvNodes_FrameQuad(TextureQuad const &quad, int32_t *ownSix, int32_t *partnerSix, NodeTurn turn)
	{
		if (!ownSix || !partnerSix)
		{
			return;
		}

		FrameNode own[3] = { { ownSix[0], ownSix[1] }, { ownSix[2], ownSix[3] }, { ownSix[4], ownSix[5] } };
		FrameNode partner[3] = { { partnerSix[0], partnerSix[1] }, { partnerSix[2], partnerSix[3] }, { partnerSix[4], partnerSix[5] } };

		TurnQuad(quad, own, partner, turn);

		for (int k = 0; k < 3; k++)
		{
			ownSix[2 * k] = own[k].u;
			ownSix[2 * k + 1] = own[k].v;
			partnerSix[2 * k] = partner[k].u;
			partnerSix[2 * k + 1] = partner[k].v;
		}
	}
}
