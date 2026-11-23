// See shape_identity.h. Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The table itself is shape_identity_data.cpp, made by tests/scenarios/verify_shape_identities.

#include "shape_identity.h"

#include <string.h>

namespace host
{
	namespace
	{
		std::string Lower(std::string s)
		{
			for (char &c : s)
			{
				c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}

			return s;
		}
	}

	ShapeIdentityRow const *ShapeIdentity_Find(std::string const &sha256)
	{
		for (size_t i = 0; i < kShapeIdentityRowCount; i++)
		{
			if (sha256 == kShapeIdentityRows[i].sha256)
			{
				return &kShapeIdentityRows[i];
			}
		}

		return nullptr;
	}

	ShapeIdentityStockFile const *ShapeIdentity_StockFile(std::string const &folder, std::string const &fileName)
	{
		std::string const f = Lower(folder), n = Lower(fileName);

		for (size_t i = 0; i < kShapeIdentityStockFileCount; i++)
		{
			if (f == kShapeIdentityStockFiles[i].folder && n == kShapeIdentityStockFiles[i].fileName)
			{
				return &kShapeIdentityStockFiles[i];
			}
		}

		return nullptr;
	}
}
