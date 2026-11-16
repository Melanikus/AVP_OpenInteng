// See light_state.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "light_state.h"

namespace host
{
	// A light number past this is not kept (the engine holds 2000 lights at most: PL_MaxLights, Prelight.c)
	static size_t const kMostLights = 1u << 16;

	void LightState::Reset()
	{
		off.clear();
		closed = false;
		flat = false;
	}

	bool LightState::IsOn(size_t light) const
	{
		return light >= off.size() || !off[light];
	}

	void LightState::SetOn(size_t light, bool on)
	{
		if (light >= kMostLights)
		{
			return;
		}

		if (on && light >= off.size())
		{
			return;
		}

		if (light >= off.size())
		{
			off.resize(light + 1, false);
		}

		off[light] = !on;
	}

	bool LightState::Toggle(size_t light)
	{
		bool const nowOn = !IsOn(light);

		SetOn(light, nowOn);
		return nowOn;
	}

	void LightState::SwitchAllOn()
	{
		off.clear();
	}

	size_t LightState::OffCount(size_t count) const
	{
		size_t offCount = 0;

		for (size_t i = 0; i < off.size() && i < count; i++)
		{
			offCount += off[i] ? 1 : 0;
		}

		return offCount;
	}
}
