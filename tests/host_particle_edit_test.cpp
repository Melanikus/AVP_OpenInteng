// The test of src/host/particle_edit: inteng's "Particle generator properties" window as numbers (reassess/object-placement.md OP-134). Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_particle_edit_test.exe (no files read)

#include <stdio.h>

#include <string>

#include "particle_edit.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	SpecialObject p;

	p.kind = SpecialKind::ParticleGenerator;
	p.particleType = 4;
	p.particleFlags = 0;
	p.time = 25;
	p.particleProbability = 40;
	p.speed = 325;
	p.quantity = 20;

	ParticleShown s = Particle_Shown(p);

	Check(s.typeName == "Unknown" && s.soundName == "None" && s.active && s.frequency == "2.5" && s.speed == "3.25" && s.probability == 40 && s.quantity == 20,
		"shown (FUN_004e78c0): the type's name (\"Unknown\" past 3), \"None\" with no sound, Active when bit 0 is clear, \"%.1f\" of time x 0.1, \"%.2f\" of speed x 0.01");
	p.hasSound = true;
	p.particleSoundWav = "spark.wav";
	Check(Particle_Shown(p).soundName == "spark.wav", "a sound: its wav name");
	Check(Particle_TypeName(0) == "Intermittent Sparks" && Particle_TypeName(3) == "Flame" && Particle_TypeName(-1) == "Unknown", "FUN_004d42f0's names");

	ParticleTyped t;

	t.type = 4;
	t.active = false;
	t.frequency = "2.3";
	t.speed = "1.15";
	t.probability = 70;
	t.quantity = 500;

	SpecialObject o = Particle_Ok(p, t);

	Check((o.particleFlags & 1) == 1 && o.time == 23 && o.speed == 115 && o.particleProbability == 70 && o.quantity == 500,
		"kept (FUN_004e7750): bit 0 = NOT Active; 2.3 s -> 23 and 1.15 m/s -> 115 (INTENG BUG, FIXED: ftol truncated to 22 / 114)");
	t.frequency = "6000";
	t.speed = "abc";
	t.probability = 101;
	t.quantity = 0;
	o = Particle_Ok(p, t);
	Check(o.time == 25 && o.speed == 325 && o.particleProbability == 40 && o.quantity == 20, "out of range (6000, 101, 0) or no number (INTENG BUG, FIXED): the old values kept");
	t.type = 0;
	t.frequency = "1";
	t.speed = "5";
	t.quantity = 7;
	o = Particle_Ok(p, t);
	Check(o.particleType == 0 && o.speed == 0 && o.quantity == 0 && o.time == 10, "type 0: speed and quantity 0");
	t.type = 2;
	t.probability = 50;
	o = Particle_Ok(p, t);
	Check(o.particleProbability == 0 && o.time == 0 && o.speed == 500, "types 1 .. 3: probability and time 0");

	if (checksFailed)
	{
		printf("FAILED host_particle_edit_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_particle_edit_test\n");
	return 0;
}
