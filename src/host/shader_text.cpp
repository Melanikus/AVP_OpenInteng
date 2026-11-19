// See shader_text.h. Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shader_text.h"

#include <string.h>

namespace
{
	size_t served = 0, missing = 0;

	// the two names the same without case (ASCII: every shader name is letters, digits, "_" and one ".")
	bool SameName(char const *a, char const *b)
	{
		for (;; a++, b++)
		{
			char x = *a, y = *b;

			x = (x >= 'A' && x <= 'Z') ? (char)(x - 'A' + 'a') : x;
			y = (y >= 'A' && y <= 'Z') ? (char)(y - 'A' + 'a') : y;

			if (x != y)
			{
				return false;
			}

			if (x == '\0')
			{
				return true;
			}
		}
	}
}

namespace host
{
	bool ShaderText_Find(char const *name, char const **text, size_t *length)
	{
		if (!name || !text || !length)
		{
			missing++;
			return false;
		}

		for (size_t i = 0; i < kShaderTextCount; i++)
		{
			if (SameName(kShaderTexts[i].name, name))
			{
				*text = kShaderTexts[i].text;
				*length = kShaderTexts[i].length;
				served++;
				return true;
			}
		}

		missing++;
		return false;
	}

	size_t ShaderText_Served()
	{
		return served;
	}

	size_t ShaderText_Missing()
	{
		return missing;
	}
}
