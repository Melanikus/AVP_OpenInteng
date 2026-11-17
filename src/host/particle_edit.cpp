// See particle_edit.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "particle_edit.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

namespace host
{
	namespace
	{
		// sscanf "%f" (0x5d3f10): true when a number is read
		bool ScanFloat(std::string const &text, float *value)
		{
			float read = 0.0f;

			if (sscanf_s(text.c_str(), "%f", &read) != 1)
			{
				return false;
			}

			*value = read;
			return true;
		}

		int32_t Ftol(double v)
		{
			return !(v == v) ? 0 : (v >= 2147483647.0 ? 2147483647 : (v <= -2147483648.0 ? (int32_t)-2147483647 - 1 : (int32_t)v));
		}
	}

	std::string Particle_TypeName(int32_t type)
	{
		static char const *const names[5] = { "Intermittent Sparks", "Steam", "Black smoke", "Flame", "Unknown" };   // 0x5e5124 + type x 0x14

		return names[type < 0 || type > 3 ? 4 : type];
	}

	ParticleShown Particle_Shown(SpecialObject const &generator)
	{
		ParticleShown s;
		char text[64];

		s.typeName = Particle_TypeName(generator.particleType);
		s.soundName = generator.hasSound ? generator.particleSoundWav : std::string("None");   // 0x4e78e8: the sound's name, else "None" (0x5d126c)
		s.active = (generator.particleFlags & 1u) == 0;
		_snprintf_s(text, sizeof(text), _TRUNCATE, "%.1f", (double)generator.time * 0.1);   // 0x4e7924
		s.frequency = text;
		_snprintf_s(text, sizeof(text), _TRUNCATE, "%.2f", (double)generator.speed * 0.01);   // 0x4e7952
		s.speed = text;
		s.probability = generator.particleProbability;
		s.quantity = generator.quantity;
		return s;
	}

	SpecialObject Particle_Ok(SpecialObject const &generator, ParticleTyped const &typed)
	{
		SpecialObject o = generator;
		float v = 0.0f;

		o.particleType = typed.type;
		o.particleFlags = typed.active ? (o.particleFlags & ~1u) : (o.particleFlags | 1u);   // 0x4e7788-0x4e7795

		// INTENG BUG, FIXED: no number keeps the old value (inteng's variable is not set before sscanf)
		if (ScanFloat(typed.frequency, &v) && v >= 0.0f && v < 6000.0f)
		{
			o.time = (uint16_t)Ftol(floor((double)v * 10.0 + 0.5));   // 0x4e77c7-0x4e77f7 (INTENG BUG, FIXED: ftol(v x 10.0f) truncated: 2.3 kept 22)
		}

		if (ScanFloat(typed.speed, &v) && v >= 0.0f && v < 600.0f)
		{
			o.speed = (uint16_t)Ftol(floor((double)v * 100.0 + 0.5));   // 0x4e781c-0x4e784c (rounded, as the time)
		}

		if (typed.probability > -1 && typed.probability < 101)
		{
			o.particleProbability = (uint16_t)typed.probability;   // 0x4e785d
		}

		if (typed.quantity > 0 && typed.quantity < 1001)
		{
			o.quantity = (uint16_t)typed.quantity;   // 0x4e787a
		}

		if (o.particleType == 0)
		{
			o.speed = 0;      // 0x4e78a9
			o.quantity = 0;
		}
		else if (o.particleType > 0 && o.particleType < 4)
		{
			o.particleProbability = 0;   // 0x4e789a
			o.time = 0;
		}

		return o;
	}
}
