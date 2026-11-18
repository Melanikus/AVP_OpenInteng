// See module_link_avp.h. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "module_link_avp.h"

#include <math.h>
#include <string.h>

namespace host
{
	namespace
	{
		void PutInt(std::vector<uint8_t> *out, int32_t value)
		{
			uint32_t const bits = (uint32_t)value;

			out->push_back((uint8_t)(bits & 0xff));
			out->push_back((uint8_t)((bits >> 8) & 0xff));
			out->push_back((uint8_t)((bits >> 16) & 0xff));
			out->push_back((uint8_t)((bits >> 24) & 0xff));
		}

		int32_t IntAt(uint8_t const *bytes, size_t at)
		{
			return (int32_t)((uint32_t)bytes[at] | ((uint32_t)bytes[at + 1] << 8) | ((uint32_t)bytes[at + 2] << 16) | ((uint32_t)bytes[at + 3] << 24));
		}

		// A turn: each row of length 1 and square to the others, within 2% (the game's 16.16 matrices are never exact), and not a mirror
		bool IsATurn(int32_t const m[9])
		{
			double const one = 65536.0;
			double r[3][3];

			for (int i = 0; i < 9; i++)
			{
				// no number past the largest a row of length 1.02 has, checked before any sum (so nothing below can grow large)
				if (m[i] > 66847 || m[i] < -66847)
				{
					return false;
				}

				r[i / 3][i % 3] = m[i] / one;
			}

			for (int a = 0; a < 3; a++)
			{
				double const length = r[a][0] * r[a][0] + r[a][1] * r[a][1] + r[a][2] * r[a][2];

				if (length < 0.98 * 0.98 || length > 1.02 * 1.02)
				{
					return false;
				}

				for (int b = a + 1; b < 3; b++)
				{
					if (fabs(r[a][0] * r[b][0] + r[a][1] * r[b][1] + r[a][2] * r[b][2]) > 0.02)
					{
						return false;
					}
				}
			}

			double const determinant = r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1]) - r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0]) + r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]);

			return determinant > 0.5;
		}
	}

	bool LinkAvp_AaaBytes(std::string const &levelName, std::vector<LinkAvpModule> const &modules, std::vector<uint8_t> *out, std::string *why)
	{
		std::string name = levelName;

		// bug (1): a name and its zero must fit the field; a longer one keeps its part after the last '/' or ':', which is all the game's reader compares (projload.cpp:3236-3256)
		if (name.size() + 1 > kLinkAvpNameBytes)
		{
			size_t const cut = name.find_last_of("/:");

			name = cut == std::string::npos ? name : name.substr(cut + 1);
		}

		if (name.size() + 1 > kLinkAvpNameBytes)
		{
			*why = "the level's name is longer than module.aaa's 60 bytes hold";
			return false;
		}

		std::vector<uint8_t> bytes(kLinkAvpNameBytes, 0);

		memcpy(bytes.data(), name.data(), name.size());

		for (LinkAvpModule const &module : modules)
		{
			if (module.items.size() > 0x7fffffffu)
			{
				*why = "a module has more links than module.aaa's count holds";
				return false;
			}

			PutInt(&bytes, module.index);
			PutInt(&bytes, (int32_t)module.items.size());

			for (LinkAvpItem const &item : module.items)
			{
				PutInt(&bytes, item.objectIndex);
				PutInt(&bytes, item.branchNo);
			}
		}

		out->swap(bytes);
		return true;
	}

	bool LinkAvp_ReadCamera(uint8_t const *bytes, size_t size, LinkAvpCamera *out, std::string *why)
	{
		if (!bytes || size != kLinkAvpCameraBytes)
		{
			*why = "module.bbb is " + std::to_string((unsigned long long)size) + " bytes, not 48";
			return false;
		}

		LinkAvpCamera camera;

		for (int i = 0; i < 3; i++)
		{
			camera.place[i] = IntAt(bytes, (size_t)i * 4);

			if (camera.place[i] > kLinkAvpMostPlace || camera.place[i] < -kLinkAvpMostPlace)
			{
				*why = "its place is outside the camera's limits (16.8 km each way)";
				return false;
			}
		}

		for (int i = 0; i < 9; i++)
		{
			camera.matrix[i] = IntAt(bytes, 12 + (size_t)i * 4);
		}

		if (!IsATurn(camera.matrix))
		{
			*why = "its matrix is not a camera's turn";
			return false;
		}

		*out = camera;
		return true;
	}
}
