// sequence_names.cpp: see sequence_names.h. Generated from avpx 3dc/avp/sequences.h (the enum names with their prefixes taken off and split into words).
#include "sequence_names.h"

#include <stddef.h>

namespace model
{
	namespace
	{
		struct TypeNames
		{
			char const *name;
			char const *subs[64];
			TypeNames(char const *n, std::initializer_list<char const *> list) : name(n), subs()
			{
				size_t at = 0;

				for (char const *each : list)
				{
					if (at < 63)
					{
						subs[at++] = each;
					}
				}
			}
		};

		size_t Most(TypeNames const &type)
		{
			size_t n = 0;

			while (n < 63 && type.subs[n])
			{
				n++;
			}

			return n;
		}

		TypeNames const kTypes[] =
		{
	{ "Alien Run", { "Standard", "Dies", "Attack Swipe", "Jump", "Standard II", "Left Hobble", "Right Hobble" } },
	{ "Alien Crawl", { "Standard", "Dies", "Attack Bite", "Attack Tail", "Pain Fall Fwd", "Pain Fall Back", "Pain Fall Left", "Pain Fall Right", "Boom Fall Fwd", "Boom Fall Back", "Boom Fall Left", "Boom Fall Right", "Attack Swipe", "Crawl Hurt", "Scamper" } },
	{ "Alien Stand", { "Standard", "Dies", "Attack Right Swipe In", "Attack Bite", "Pain Fall Fwd", "Pain Fall Back", "Pain Fall Left", "Pain Fall Right", "Boom Fall Fwd", "Boom Fall Back", "Boom Fall Left", "Boom Fall Right", "Spin Clockwise", "Spin Anticlockwise", "Feed", "Taunt", "Burning Death", "Standard Elevation", "Fidget A", "Fidget B", "Attack Left Swipe In", "Attack Tail", "Dormant", "Unfurl", "Attack Both In", "Attack Both Down", "Spasm", "Spear Fly Fwrd", "Spear Fly Back", "Spear Hit Bck", "Spear Hit Frnt", "Attack Low Left Swipe", "Attack Low Right Swipe", "Taunt2", "Taunt3", "Fear", "Hit Left", "Hit Right" } },
	{ "Alien Crouch", { "Standard", "Dies", "Attack Bite", "Attack Tail", "Attack Swipe", "Dies Thrash", "Standard Elevation", "Pounce", "Hit Left", "Hit Right", "Taunt" } },
	{ "Hugger", { "Stand", "Run", "Dies", "Jump", "Attack", "Die On Fire", "Floats" } },
	{ "Marine Run", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Walk", "Tem Run On Fire", "Tem Run On Fire B", "Tem Run On Fire C", "Mooch Bored", "Mooch Alert", "Sprint", "Sprint Head Delta", "Fire From Hips" } },
	{ "Marine Crawl", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Fire From Hips" } },
	{ "Marine Stand", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Die Secondary", "Fidget A", "Fidget B", "Fidget C", "Tem Back Death", "Tem Front Death", "Tem Sum Death", "Hit Left Leg", "Hit Right Leg", "Hit Left Arm", "Hit Right Arm", "Hit Chest Front", "Hit Chest Back", "Hit Head Front", "Hit Head Back", "Attack Secondary", "Stand To Fidget", "Tem Left Sholdr", "Tem Right Sholdr", "Tem Left Thigh", "Tem Right Thigh", "Tem Left Forarm", "Tem Right Forarm", "Tem Left Shin", "Tem Right Shin", "Tem Burning", "Taunt One", "Wait Alert", "Minigun Delta", "Wild Fire 0", "Spear Fly Fwrd", "Spear Fly Back", "Spear Hit Wall B", "Spear Hit Wall F", "Spasm", "Fire From Hips", "Hip Fire Elevation", "Wild Fire 45", "Wild Fire 67", "Wild Fire 90", "Reload", "Pump Action", "Get Weapon", "Panic One", "Panic Two", "Tem Electric Death One", "Tem Electric Death Two", "Wild Fire 22", "Panic Reload" } },
	{ "Marine Crouch", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Hit Left Leg", "Hit Right Leg", "Hit Left Arm", "Hit Right Arm", "Hit Chest Front", "Hit Chest Back", "Hit Head Front", "Hit Head Back", "Attack Secondary", "Pump Action", "Tem Electric Death One", "Fire From Hips", "Hip Fire Elevation" } },
	{ "Predator HUD", { "Stand", "Run", "Come", "Go", "Attack Primary", "Attack Secondary", "Program", "Attack Jab", "Fidget", "Attack Primary Two", "Pull Back", "Hold", "Attack Secondary Weak One", "Attack Secondary Weak Two", "Attack Secondary Strong One", "Attack Secondary Strong Two" } },
	{ "Marine HUD", { "Stationary", "Standard Reload", "Standard Fire", "Come", "Go", "Fidget", "Secondary Fire", "Tertiary Fire", "Right Out", "Left Out" } },
	{ "Predator Run", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Attack Offence Sweep", "Attack Defence Stab", "Attack Defence Sweep", "Walk" } },
	{ "Predator Crawl", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Attack Offence Sweep", "Attack Defence Stab", "Attack Defence Sweep" } },
	{ "Predator Stand", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Get Weapon", "Hit Left Leg", "Hit Right Leg", "Hit Left Arm", "Hit Right Arm", "Hit Chest Front", "Hit Chest Back", "Hit Head Front", "Hit Head Back", "Tem Death Fwrd", "Tem Death Bwrd", "Tem Left Arm", "Tem Left Leg", "Tem Right Arm", "Tem Right Leg", "Tem Riddled", "Tem Burning", "Taunt One", "Attack Offense Sweep", "Attack Defence Stab", "Attack Defence Sweep", "Attack Quick Jab", "Attack Uppercut", "Jump Up", "Spasm" } },
	{ "Predator Crouch", { "Standard", "Dies Standard", "Jump", "Attack Primary", "Elevation", "Get Weapon", "Hit Left Leg", "Hit Right Leg", "Hit Left Arm", "Hit Right Arm", "Hit Chest Front", "Hit Chest Back", "Hit Head Front", "Hit Head Back", "Attack Offence Sweep", "Attack Defence Stab", "Attack Defence Sweep", "Det Prog", "Det Laugh", "Det Die1" } },
	{ "Queen Left Stance Template", { "Standard", "Forward L2R", "Backward L2R", "Right L2L", "Left L2L", "Left Swipe", "Right Swipe", "Right Hit", "Left Hit" } },
	{ "Queen Left Stance Full", { "Standard Hiss", "Taunt", "Forward L2R", "Backward L2R", "Right L2L", "Left L2L" } },
	{ "Queen Right Stance Template", { "Standard", "Forward R2L", "Backward R2L", "Right R2R", "Left R2R", "Left Swipe", "Right Swipe", "Right Hit", "Left Hit", "Left Swipe Low", "Right Swipe Low" } },
	{ "Queen Right Stance Full", { "Standard Hiss", "Taunt", "Forward R2L", "Backward R2L", "Right R2R", "Left R2R" } },
	{ "Queen General", { "Squash Death", "Face Death", "Fall Death", "Butt Connect", "Run Butt Attack", "Walk", "Explode Death", "Sprint", "Stop To Left", "Stop To Right", "Walk II", "Sprint II", "Spine Elevation", "Search Floor", "Fire Flinch", "Fire Steps", "Sprint Full", "Explosion Stun", "Climb Out" } },
	{ "Alien HUD", { "Left Swipe Down", "Right Swipe Down", "Left Swipe In", "Right Swipe In", "Both In", "Both Down", "Tail Come", "Tail Hold", "Tail Strike", "Pounce In", "Pounce Down", "Hor Delta", "Ver Delta", "Eat" } },
	{ "Xenoborg", { "Die Backwards", "Die Forwards", "Fire Bolter", "Head Horizontal Delta", "Head Vertical Delta", "Left Arm Horizontal Delta", "Left Arm Vertical Delta", "Fire Plasma", "Power Up", "Power Down", "Right Arm Horizontal Delta", "Right Arm Vertical Delta", "Powered Down Standard", "Powered Up Standard", "Standing Death", "Torso Delta", "Turn Left", "Turn Right", "Walking", "Left Leg Missing Death", "Right Leg Missing Death" } },
		};
	}

	char const *SequenceTypeName(int type)
	{
		return type >= 0 && (size_t)type < sizeof(kTypes) / sizeof(kTypes[0]) ? kTypes[type].name : "";
	}

	char const *SequenceSubName(int type, int sub)
	{
		if (type < 0 || (size_t)type >= sizeof(kTypes) / sizeof(kTypes[0]) || sub < 0 || (size_t)sub >= Most(kTypes[type]))
		{
			return "";
		}

		return kTypes[type].subs[sub];
	}

	std::string SequenceWords(int type, int sub)
	{
		std::string const typeName = SequenceTypeName(type);
		std::string const subName = SequenceSubName(type, sub);

		if (typeName.empty())
		{
			return std::to_string(type) + "." + std::to_string(sub);
		}

		return typeName + ": " + (subName.empty() ? std::to_string(sub) : subName);
	}
}
