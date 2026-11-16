// See autolink_pass.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autolink_pass.h"

#include <math.h>
#include <string.h>

namespace host
{
	namespace
	{
		int const kWidth = 320;               // FUN_0057f520's 0x140 and 0xf0 (FUN_004c6380 0x4c6c00)
		int const kHeight = 240;
		double const kCentreX = 160.0;        // 0xa0, 0x78
		double const kCentreY = 120.0;
		double const kLens = 160.0;           // 0xa0 (not proven: see the header)
		double const kNear = 1.0;

		int32_t const kOne = 0x10000;
		int32_t const kSin = 0x5798;          // the turn's matrix (0x4c7c64-0x4c7d54)
		int32_t const kCos = 0xf08f;

		// The six axis matrices (0x4c7ad3-0x4c7d5b): rows are the camera's right, up and forward vectors in the level
		int32_t const kAxisMatrix[3][2][9] =
		{
			{ { 0, 0, kOne, 0, kOne, 0, -kOne, 0, 0 },          // axis 0, direction -1: forward -x
			  { 0, 0, -kOne, 0, kOne, 0, kOne, 0, 0 } },        // axis 0, direction +1: forward +x
			{ { 0, 0, -kOne, kOne, 0, 0, 0, -kOne, 0 },         // axis 1, -1: forward -y
			  { 0, 0, kOne, kOne, 0, 0, 0, kOne, 0 } },         // axis 1, +1: forward +y
			{ { -kOne, 0, 0, 0, kOne, 0, 0, 0, -kOne },         // axis 2, -1: forward -z
			  { kOne, 0, 0, 0, kOne, 0, 0, 0, kOne } },         // axis 2, +1: forward +z
		};

		// The four turns (0x4c7c64-0x4c7d54), in the order the pass uses them: local_174, local_150, local_c0, local_78
		int32_t const kTurn[4][9] =
		{
			{ kSin, 0, -kCos, 0, kOne, 0, kCos, 0, kSin },
			{ kSin, 0, kCos, 0, kOne, 0, -kCos, 0, kSin },
			{ kOne, 0, 0, 0, kSin, -kCos, 0, kCos, kSin },
			{ kOne, 0, 0, 0, kSin, kCos, 0, -kCos, kSin },
		};

		int32_t Mul(int32_t a, int32_t b)
		{
			return (int32_t)(((int64_t)a * (int64_t)b) >> 16);   // FUN_00499b30: a fixed-point product
		}

		// FUN_004b34b0(base, turn, out): out = turn x base, row by row (the decompile's sums, 0x4b34bd-0x4b36f1)
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

		int32_t HalfToZero(int64_t sum)
		{
			return (int32_t)(sum / 2);   // C division: toward zero, as the code's CDQ / SUB / SAR
		}

		// One picture: the ID buffer and the depth buffer
		struct Picture
		{
			std::vector<int32_t> ids;
			std::vector<double> depth;   // 1 / z, larger is nearer

			Picture() : ids((size_t)kWidth * kHeight, 0), depth((size_t)kWidth * kHeight, 0.0)
			{
			}
		};

		struct Vertex
		{
			double x, y, z;   // camera space
		};

		void FillTriangle(Picture *picture, int32_t id, Vertex const &a, Vertex const &b, Vertex const &c)
		{
			double sx[3], sy[3], iz[3];
			Vertex const *const v[3] = { &a, &b, &c };

			for (int i = 0; i < 3; i++)
			{
				sx[i] = kCentreX + v[i]->x / v[i]->z * kLens;
				sy[i] = kCentreY - v[i]->y / v[i]->z * kLens;
				iz[i] = 1.0 / v[i]->z;
			}

			double const area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);

			if (fabs(area) < 1e-9)
			{
				return;
			}

			double minX = sx[0] < sx[1] ? sx[0] : sx[1];
			double maxX = sx[0] < sx[1] ? sx[1] : sx[0];
			double minY = sy[0] < sy[1] ? sy[0] : sy[1];
			double maxY = sy[0] < sy[1] ? sy[1] : sy[0];

			minX = sx[2] < minX ? sx[2] : minX;
			maxX = sx[2] > maxX ? sx[2] : maxX;
			minY = sy[2] < minY ? sy[2] : minY;
			maxY = sy[2] > maxY ? sy[2] : maxY;

			int const x0 = minX < 0 ? 0 : (int)floor(minX);
			int const x1 = maxX > kWidth - 1 ? kWidth - 1 : (int)ceil(maxX);
			int const y0 = minY < 0 ? 0 : (int)floor(minY);
			int const y1 = maxY > kHeight - 1 ? kHeight - 1 : (int)ceil(maxY);

			for (int y = y0; y <= y1; y++)
			{
				for (int x = x0; x <= x1; x++)
				{
					double const px = x + 0.5;
					double const py = y + 0.5;
					double const w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
					double const w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
					double const w2 = 1.0 - w0 - w1;

					if (w0 < 0.0 || w1 < 0.0 || w2 < 0.0)
					{
						continue;
					}

					double const nearness = w0 * iz[0] + w1 * iz[1] + w2 * iz[2];
					size_t const at = (size_t)y * kWidth + (size_t)x;

					if (nearness > picture->depth[at])
					{
						picture->depth[at] = nearness;
						picture->ids[at] = id;
					}
				}
			}
		}

		// A convex polygon (camera space) clipped to the near plane and filled as a fan
		void FillPolygon(Picture *picture, int32_t id, std::vector<Vertex> const &corners)
		{
			std::vector<Vertex> clipped;

			for (size_t i = 0; i < corners.size(); i++)
			{
				Vertex const &now = corners[i];
				Vertex const &next = corners[(i + 1) % corners.size()];
				bool const nowIn = now.z >= kNear;
				bool const nextIn = next.z >= kNear;

				if (nowIn)
				{
					clipped.push_back(now);
				}

				if (nowIn != nextIn)
				{
					double const t = (kNear - now.z) / (next.z - now.z);
					Vertex cut;

					cut.x = now.x + t * (next.x - now.x);
					cut.y = now.y + t * (next.y - now.y);
					cut.z = kNear;
					clipped.push_back(cut);
				}
			}

			for (size_t i = 1; i + 1 < clipped.size(); i++)
			{
				FillTriangle(picture, id, clipped[0], clipped[i], clipped[i + 1]);
			}
		}
	}

	AutoLinkPass::AutoLinkPass()
	{
	}

	void AutoLinkPass::Build(std::vector<AutoLinkModule> const &list)
	{
		modules = list;
		polygons.clear();

		ranges.clear();

		for (size_t m = 0; m < modules.size(); m++)
		{
			PortalModule const &one = modules[m].module;
			Range range;

			range.first = polygons.size();
			range.end = polygons.size();

			for (int i = 0; i < 3; i++)
			{
				range.lo[i] = (double)one.place[i] + one.extents[2 * i + 1];
				range.hi[i] = (double)one.place[i] + one.extents[2 * i];
			}

			ranges.push_back(range);

			if (!one.shape)
			{
				continue;
			}

			// the module's own pointer to its shape survives the copy: the shapes are kept by the caller
			for (size_t k = 0; k < one.shape->polygons.size(); k++)
			{
				Polygon polygon;

				polygon.id = modules[m].number + 1;

				for (size_t j = 0; j < one.shape->polygons[k].size(); j++)
				{
					int32_t const point = one.shape->polygons[k][j];

					if (point < 0 || (size_t)point * 3 + 2 >= one.shape->points.size())
					{
						polygon.corners.clear();
						break;
					}

					polygon.corners.push_back((double)one.place[0] + one.shape->points[(size_t)point * 3]);
					polygon.corners.push_back((double)one.place[1] + one.shape->points[(size_t)point * 3 + 1]);
					polygon.corners.push_back((double)one.place[2] + one.shape->points[(size_t)point * 3 + 2]);
				}

				if (polygon.corners.size() >= 9)
				{
					polygons.push_back(polygon);
				}
			}

			ranges[m].end = polygons.size();
		}
	}

	void AutoLinkPass::SeenByCamera(int32_t const place[3], int32_t const turn[9], std::set<int32_t> *seen) const
	{
		Picture picture;
		double axes[9];

		for (int i = 0; i < 9; i++)
		{
			axes[i] = turn[i] / 65536.0;
		}

		for (size_t m = 0; m < ranges.size(); m++)
		{
			Range const &range = ranges[m];

			if (range.first == range.end)
			{
				continue;
			}

			// a module whose box is wholly behind the near plane or wholly outside one side of the view is not drawn (a conservative test on the box's 8 corners)
			bool behind = true, left = true, right = true, below = true, above = true;

			for (int corner = 0; corner < 8; corner++)
			{
				double const dx = ((corner & 1) ? range.hi[0] : range.lo[0]) - place[0];
				double const dy = ((corner & 2) ? range.hi[1] : range.lo[1]) - place[1];
				double const dz = ((corner & 4) ? range.hi[2] : range.lo[2]) - place[2];
				double const cx = dx * axes[0] + dy * axes[1] + dz * axes[2];
				double const cy = dx * axes[3] + dy * axes[4] + dz * axes[5];
				double const cz = dx * axes[6] + dy * axes[7] + dz * axes[8];

				behind = behind && cz < kNear;
				left = left && cx * kLens < -cz * kCentreX;
				right = right && cx * kLens > cz * kCentreX;
				below = below && cy * kLens < -cz * kCentreY;
				above = above && cy * kLens > cz * kCentreY;
			}

			if (behind || left || right || below || above)
			{
				continue;
			}

			for (size_t p = range.first; p < range.end; p++)
			{
				Polygon const &polygon = polygons[p];
				std::vector<Vertex> corners;

				for (size_t j = 0; j + 2 < polygon.corners.size(); j += 3)
				{
					double const dx = polygon.corners[j] - place[0];
					double const dy = polygon.corners[j + 1] - place[1];
					double const dz = polygon.corners[j + 2] - place[2];
					Vertex v;

					v.x = dx * axes[0] + dy * axes[1] + dz * axes[2];
					v.y = dx * axes[3] + dy * axes[4] + dz * axes[5];
					v.z = dx * axes[6] + dy * axes[7] + dz * axes[8];
					corners.push_back(v);
				}

				FillPolygon(&picture, polygon.id, corners);
			}
		}

		// FUN_004c6fb0 0x4c70c7-0x4c70ed: a module is seen only where a 3 x 3 block holds one number
		for (int y = 0; y + 2 < kHeight; y++)
		{
			for (int x = 0; x + 2 < kWidth; x++)
			{
				int32_t const id = picture.ids[(size_t)y * kWidth + (size_t)x];

				if (id == 0 || seen->count(id - 1))
				{
					continue;
				}

				bool same = true;

				for (int dy = 0; dy < 3 && same; dy++)
				{
					for (int dx = 0; dx < 3 && same; dx++)
					{
						same = picture.ids[(size_t)(y + dy) * kWidth + (size_t)(x + dx)] == id;
					}
				}

				if (same)
				{
					seen->insert(id - 1);
				}
			}
		}
	}

	void AutoLinkPass::SeenFrom(size_t from, std::set<int32_t> *seen, size_t *views, size_t *portals) const
	{
		size_t drawn = 0, met = 0;

		seen->clear();

		if (from < modules.size())
		{
			AutoLinkModule const &a = modules[from];

			for (size_t b = 0; b < modules.size(); b++)
			{
				Portal portal;

				if (b == from || !AutoLink_ModulesMeet(a.module, modules[b].module, &portal))
				{
					continue;
				}

				met++;

				// the view's place (0x4c7db3-0x4c7e56): the middle of the portal's rectangle, on the contact axis the plane's middle 10000 back into A
				int32_t position[3];

				for (int i = 0; i < 3; i++)
				{
					position[i] = i == portal.axis ? HalfToZero((int64_t)portal.planeB + portal.planeA) - portal.direction * 10000 : HalfToZero((int64_t)portal.lo[i] + portal.hi[i]);
				}

				int32_t const *const base = kAxisMatrix[portal.axis][portal.direction == 1 ? 1 : 0];
				int32_t view[9];

				// five views: straight, then the four turns in the pass's order (0x4c7f59, 0x4c803b, 0x4c811d, 0x4c81f1, 0x4c82c5)
				SeenByCamera(position, base, seen);
				drawn++;

				for (int t = 0; t < 4; t++)
				{
					Compose(base, kTurn[t], view);
					SeenByCamera(position, view, seen);
					drawn++;
				}

				// two more for a contact on the x or the z axis, moved 500 beyond the portal's side (0x4c82d1-0x4c8677); the turn is guessed (named in the header)
				if (portal.axis == 0 || portal.axis == 2)
				{
					int const side = portal.axis == 0 ? 2 : 0;
					int32_t first[3] = { position[0], position[1], position[2] };
					int32_t second[3] = { position[0], position[1], position[2] };

					if (portal.axis == 0)
					{
						first[side] = portal.direction == -1 ? portal.hi[side] + 500 : portal.lo[side] - 500;
						second[side] = portal.direction == 1 ? portal.hi[side] + 500 : portal.lo[side] - 500;
					}
					else
					{
						first[side] = portal.direction == 1 ? portal.hi[side] + 500 : portal.lo[side] - 500;
						second[side] = portal.direction == -1 ? portal.hi[side] + 500 : portal.lo[side] - 500;
					}

					Compose(base, kTurn[1], view);
					SeenByCamera(first, view, seen);
					drawn++;
					Compose(base, kTurn[0], view);
					SeenByCamera(second, view, seen);
					drawn++;
				}
			}
		}

		seen->erase(modules.size() > from ? modules[from].number : -1);

		if (views)
		{
			*views = drawn;
		}

		if (portals)
		{
			*portals = met;
		}
	}
}
