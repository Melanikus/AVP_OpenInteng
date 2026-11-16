// key_reader.cpp: see key_reader.h. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "key_reader.h"

#include <windows.h>

namespace host
{
	namespace
	{
		bool RealKeyboard(int vk)
		{
			return GetKeyState(vk) < 0;
		}

		bool (*realReader)(int vk) = RealKeyboard;
	}

	void KeyReader_SetRealReader(bool (*reader)(int vk))
	{
		realReader = reader ? reader : RealKeyboard;
	}

	bool KeyReader_Held(int vk, bool const *table, bool scenarioDriven)
	{
		if (vk < 0 || vk > 0xFF)
		{
			return false;
		}

		if (table && table[vk])
		{
			return true;
		}

		return !scenarioDriven && realReader(vk);
	}
}
