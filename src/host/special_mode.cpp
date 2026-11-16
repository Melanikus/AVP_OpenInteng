// See special_mode.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "special_mode.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <functional>

#include "level_file.h"

namespace host
{
	namespace
	{
		// ---- reading what was typed
		std::string Trim(std::string const &text)
		{
			size_t a = 0, b = text.size();

			while (a < b && isspace((unsigned char)text[a]))
			{
				a++;
			}

			while (b > a && isspace((unsigned char)text[b - 1]))
			{
				b--;
			}

			return text.substr(a, b - a);
		}

		std::string Lower(std::string text)
		{
			for (char &c : text)
			{
				c = (char)tolower((unsigned char)c);
			}

			return text;
		}

		bool ReadNumber(std::string const &typed, double *out)
		{
			std::string const t = Trim(typed);

			if (t.empty())
			{
				return false;
			}

			char *end = nullptr;
			double const v = strtod(t.c_str(), &end);

			if (!end || *end != 0 || !(v == v) || fabs(v) > 1e12)
			{
				return false;
			}

			*out = v;
			return true;
		}

		// yes / no for a check box
		bool ReadYesNo(std::string const &typed, bool *out)
		{
			std::string const t = Lower(Trim(typed));

			if (t == "y" || t == "yes" || t == "1" || t == "on" || t == "true" || t == "x")
			{
				*out = true;
				return true;
			}

			if (t == "n" || t == "no" || t == "0" || t == "off" || t == "false")
			{
				*out = false;
				return true;
			}

			return false;
		}

		std::string Fixed(double v, int places)
		{
			char buffer[64];

			_snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%.*f", places, v);
			return buffer;
		}

		// inteng's ftol: truncation toward zero
		int32_t Ftol(double v)
		{
			return (int32_t)v;
		}

		// ---- a field of a dialog
		struct Choice
		{
			char const *name;
			int32_t value;
		};

		struct Field
		{
			std::string title;                                            // the label and the limits
			std::vector<Choice> choices;                                  // non-empty: the answer is one of these
			std::function<std::string(SpecialObject const &)> show;       // the value now, as the dialog shows it
			std::function<bool(SpecialObject *, std::string const &)> apply;   // false: the answer is out of range (the dialog leaves the field as it was)
			int kind = 0;                                                 // 0 an edit, 1 a check box, 2 a choice (the form's control)
		};

		bool PickChoice(Field const &field, std::string const &typed, int32_t *value)
		{
			std::string const t = Lower(Trim(typed));

			if (t.empty())
			{
				return false;
			}

			double number = 0;

			if (ReadNumber(t, &number) && number >= 1 && number <= (double)field.choices.size() && number == floor(number))
			{
				*value = field.choices[(size_t)number - 1].value;
				return true;
			}

			int found = -1;

			for (size_t i = 0; i < field.choices.size(); i++)
			{
				std::string const name = Lower(field.choices[i].name);

				if (name == t)
				{
					*value = field.choices[i].value;
					return true;
				}

				if (name.find(t) != std::string::npos)
				{
					if (found >= 0)
					{
						return false;
					}

					found = (int)i;
				}
			}

			if (found >= 0)
			{
				*value = field.choices[(size_t)found].value;
				return true;
			}

			return false;
		}

		std::string ChoiceName(Field const &field, int32_t value)
		{
			for (Choice const &c : field.choices)
			{
				if (c.value == value)
				{
					return c.name;
				}
			}

			return std::string();
		}

		Field Text(std::string const &title, std::function<std::string &(SpecialObject *)> get)
		{
			Field f;

			f.title = title;
			f.show = [get](SpecialObject const &o) { return get(const_cast<SpecialObject *>(&o)); };
			f.apply = [get](SpecialObject *o, std::string const &typed)
			{
				std::string const t = Trim(typed);

				if (t.empty())
				{
					return true;
				}

				get(o) = t;
				return true;
			};
			return f;
		}

		// a whole number with limits (both ends included)
		Field Whole(std::string const &title, double lo, double hi, std::function<int32_t(SpecialObject const &)> get, std::function<void(SpecialObject *, int32_t)> set)
		{
			Field f;

			f.title = title + " (" + Fixed(lo, 0) + "-" + Fixed(hi, 0) + ")";
			f.show = [get](SpecialObject const &o) { return std::to_string((long long)get(o)); };
			f.apply = [lo, hi, set](SpecialObject *o, std::string const &typed)
			{
				double v = 0;

				if (Trim(typed).empty())
				{
					return true;
				}

				if (!ReadNumber(typed, &v) || v < lo || v > hi)
				{
					return false;
				}

				set(o, Ftol(v));
				return true;
			};
			return f;
		}

		Field Box(std::string const &title, std::function<bool(SpecialObject const &)> get, std::function<void(SpecialObject *, bool)> set)
		{
			Field f;

			f.title = title + " (y/n)";
			f.kind = 1;
			f.show = [get](SpecialObject const &o) { return std::string(get(o) ? "y" : "n"); };
			f.apply = [set](SpecialObject *o, std::string const &typed)
			{
				bool v = false;

				if (Trim(typed).empty())
				{
					return true;
				}

				if (!ReadYesNo(typed, &v))
				{
					return false;
				}

				set(o, v);
				return true;
			};
			return f;
		}

		Field Pick(std::string const &title, std::vector<Choice> choices, std::function<int32_t(SpecialObject const &)> get, std::function<void(SpecialObject *, int32_t)> set)
		{
			Field f;

			f.title = title;
			f.choices = choices;
			f.kind = 2;
			Field *const self = nullptr;
			(void)self;
			f.show = [get, choices](SpecialObject const &o)
			{
				int32_t const v = get(o);

				for (Choice const &c : choices)
				{
					if (c.value == v)
					{
						return std::string(c.name);
					}
				}

				return std::string();
			};
			f.apply = [f, set](SpecialObject *o, std::string const &typed)
			{
				int32_t v = 0;

				if (Trim(typed).empty())
				{
					return true;
				}

				if (!PickChoice(f, typed, &v))
				{
					return false;
				}

				set(o, v);
				return true;
			};
			return f;
		}

		// ---- the flag bits of the AVP generator and bad guy dialogs (edobtype-dialogs.md sections 2 and 3)
		uint32_t const kMarine = 0x800, kAlien = 0x1000, kPredator = 0x2000;
		uint32_t const kNotEasy = 0x100000, kNotMedium = 0x200000, kNotHard = 0x400000;
		uint32_t const kUseOwnLimit = 0x2000000, kUseOwnRate = 0x4000000, kAdvanced = 0x8000000, kMultiplayerStart = 0x40000000, kInactive = 0x80000000u;

		Field Flag(std::string const &title, uint32_t bit, bool checkedWhenClear)
		{
			return Box(title, [bit, checkedWhenClear](SpecialObject const &o) { return ((o.flags & bit) != 0) != checkedWhenClear; },
				[bit, checkedWhenClear](SpecialObject *o, bool checked)
				{
					bool const set = checked != checkedWhenClear;

					o->flags = set ? (o->flags | bit) : (o->flags & ~bit);
				});
		}

		std::string GeneratorName(SpecialKind kind)
		{
			switch (kind)
			{
			case SpecialKind::Generator: return "AVP generator";
			case SpecialKind::BadGuy: return "AVP Bad Guy";
			case SpecialKind::PlayerStart: return "Player starting position";
			case SpecialKind::Sound: return "Sound";
			case SpecialKind::ParticleGenerator: return "Particle generator";
			}

			return "Object";
		}

		// The fields of the dialog of this object, in the dialog's order. Fields after the type depend on it, so the list is made again from the draft at every step.
		std::vector<Field> Fields(SpecialObject const &o, double levelScale)
		{
			std::vector<Field> f;
			double const unit = 100.0 * (levelScale > 0.0 ? levelScale : 1.0);

			f.push_back(Text("Name (0 to 63 plain letters)", [](SpecialObject *x) -> std::string & { return x->name; }));

			if (o.kind == SpecialKind::Generator)
			{
				// edobtype.dll dialog 133 "Generator Settings", controls in the order of their numbers
				f.push_back(Flag("Easy", kNotEasy, true));
				f.push_back(Flag("Medium", kNotMedium, true));
				f.push_back(Flag("Hard", kNotHard, true));

				static char const *const names[15] = { "Marine Pulse Rifle", "Marine Flamer", "Marine Smartgun", "Marine Sadar", "Marine Grenade", "Marine Minigun", "Civilian Shotgun",
					"Civilian Pistol", "Civilian Flamer", "Civilian Unarmed", "Civilian Molotov", "Alien", "Pred Alien", "Praetorian", "Marine Pistol" };
				int const order[15] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };

				// weights 1027 to 1041 (the Marine Pistol, control 1049, comes after the settings)
				for (int k = 0; k < 14; k++)
				{
					int const i = order[k];

					f.push_back(Whole(std::string(names[i]) + " weight", 0, 1000, [i](SpecialObject const &x) { return x.weights[i]; }, [i](SpecialObject *x, int32_t v) { x->weights[i] = v; }));
				}

				// the rate and its increase are kept whole and shown x 0.01, 0 to 50.0 (DDV_MinMaxFloat)
				Field rate;

				rate.title = "Initial generation rate per minute (0-50)";
				rate.show = [](SpecialObject const &x) { return Fixed(x.genRate * 0.01, 2); };
				rate.apply = [](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < 0.0 || v > 50.0)
					{
						return false;
					}

					x->genRate = (int32_t)floor(v * 100.0 + 0.5);
					return true;
				};
				f.push_back(rate);

				Field rise;

				rise.title = "Increase in rate per minute (0-50)";
				rise.show = [](SpecialObject const &x) { return Fixed(x.genIncrease * 0.01, 2); };
				rise.apply = [](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < 0.0 || v > 50.0)
					{
						return false;
					}

					x->genIncrease = (int32_t)floor(v * 100.0 + 0.5);
					return true;
				};
				f.push_back(rise);

				f.push_back(Flag("Marine", kMarine, false));
				f.push_back(Flag("Alien", kAlien, false));
				f.push_back(Flag("Predator", kPredator, false));
				// "Use global generator times" is the flag USEOWNRATE read the other way round; "Use own max npc setting" is USEOWNLIMIT. (inteng.exe's dialog saves these two on each other's bit: edobtype
				// 0x10002410. Melanikus ruled 2026-11-15 that both are read and written on the SAME bit, so editing a generator never flips them: AvP-docs\engine\inteng\unfinished-in-inteng.md.)
				f.push_back(Flag("Use global generator times", kUseOwnRate, true));
				f.push_back(Flag("Starts inactive", kInactive, false));
				f.push_back(Whole("Marine Pistol weight", 0, 1000, [](SpecialObject const &x) { return x.weights[14]; }, [](SpecialObject *x, int32_t v) { x->weights[14] = v; }));
				f.push_back(Whole("Max NPCs", 0, 255, [](SpecialObject const &x) { return (int32_t)x.genLimit; }, [](SpecialObject *x, int32_t v) { x->genLimit = (uint8_t)v; }));
				f.push_back(Flag("Use own max npc setting", kUseOwnLimit, false));
				f.push_back(Flag("Multiplayer start", kMultiplayerStart, false));
			}
			else if (o.kind == SpecialKind::BadGuy)
			{
				// edobtype.dll dialog 129 in the AvP mode
				f.push_back(Pick("Type of Object", { { "Alien", 1 }, { "Marine", 7 }, { "Predator", 2 }, { "Facehugger", 3 }, { "Alien Queen", 6 }, { "Xenoborg", 4 }, { "Predator-Alien", 5 },
					{ "Pretorian", 8 }, { "Sentry Gun", 9 } }, [](SpecialObject const &x) { return x.type; }, [](SpecialObject *x, int32_t v) { x->type = v; }));

				switch (o.type)
				{
				case 2:
				{
					std::vector<Choice> const weapons = { { "Pistol", 0 }, { "Wristblade", 1 }, { "Plasma Caster", 2 }, { "Staff", 3 } };

					f.push_back(Pick("Primary", weapons, [](SpecialObject const &x) { return (int32_t)(x.subType & 7); }, [](SpecialObject *x, int32_t v) { x->subType = (uint8_t)((x->subType & 0x38) | v); }));
					f.push_back(Pick("Secondary", weapons, [](SpecialObject const &x) { return (int32_t)((x.subType >> 3) & 7); },
						[](SpecialObject *x, int32_t v) { x->subType = (uint8_t)((x->subType & 7) | (v << 3)); }));
					break;
				}

				case 4:
					f.push_back(Whole("Active Time 0=default", 0, 1275, [](SpecialObject const &x) { return (int32_t)x.extra1 * 5; },
						[](SpecialObject *x, int32_t v) { x->extra1 = (uint8_t)(v / 5 > 255 ? 255 : v / 5); }));
					f.push_back(Whole("Module Range - 0=default", 0, 255, [](SpecialObject const &x) { return (int32_t)x.extra2; }, [](SpecialObject *x, int32_t v) { x->extra2 = (uint8_t)v; }));
					break;

				case 9:
					f.push_back(Whole("Ammo (30000+ =infinite)", 0, 30000, [](SpecialObject const &x) { return (int32_t)x.extra1 * 256 + (int32_t)x.extra2; },
						[](SpecialObject *x, int32_t v) { x->extra2 = (uint8_t)(v & 0xff); x->extra1 = (uint8_t)(v >> 8); }));
					break;

				case 7:
					f.push_back(Pick("Subtype", { { "Pulse Rifle", 0 }, { "Pistol", 5 }, { "Smartgun", 10 }, { "Grenade Launcher", 20 }, { "Flamethrower", 50 }, { "Sadar", 60 }, { "Minigun", 70 },
						{ "Android", 120 }, { "Civ - Shotgun 1", 30 }, { "Civ - Shotgun 2", 31 }, { "Civ - Shotgun 3", 32 }, { "Civ - Pistol 1", 40 }, { "Civ - Pistol 2", 41 },
						{ "Civ - Flamer", 100 }, { "Civ - Molotov", 80 }, { "Civ - Unarmed", 110 }, { "Civ - Scientist 1", 90 }, { "Civ - Scientist 2", 91 } },
						[](SpecialObject const &x) { return (int32_t)x.subType; }, [](SpecialObject *x, int32_t v) { x->subType = (uint8_t)v; }));
					// the names come from hnpcmarine.txt / hnpc_civvie.txt, which are not here: the number stands (0 = Random)
					f.push_back(Whole("Texturing ID (0 = Random)", 0, 255, [](SpecialObject const &x) { return (int32_t)x.texture; }, [](SpecialObject *x, int32_t v) { x->texture = (uint8_t)v; }));
					break;

				default:
					break;
				}

				f.push_back(Flag("Marine (game mode)", kMarine, false));
				f.push_back(Flag("Alien (game mode)", kAlien, false));
				f.push_back(Flag("Predator (game mode)", kPredator, false));
				f.push_back(Flag("Starts Inactive", kInactive, false));
				f.push_back(Flag("Easy", kNotEasy, true));
				f.push_back(Flag("Medium", kNotMedium, true));
				f.push_back(Flag("Hard", kNotHard, true));
			}
			else if (o.kind == SpecialKind::Sound)
			{
				// edobtype.dll dialog 132 "Sound Edit" (edobtype-dialogs.md section 1); the file's units are inteng's divided by 100 x the level scale
				f.push_back(Text("Wav file name", [](SpecialObject *x) -> std::string & { return x->wav; }));

				Field inner;

				inner.title = "Inner Range (m) (0-1000)";
				inner.show = [unit](SpecialObject const &x) { return Fixed(x.inner * unit * 0.00001, 2); };
				inner.apply = [unit](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < 0.0 || v > 1000.0)
					{
						return false;
					}

					x->inner = (int32_t)(Ftol(v * 100000.0) / unit);
					return true;
				};
				f.push_back(inner);

				Field outer;

				outer.title = "Outer Range (m) (0-2000)";
				outer.show = [unit](SpecialObject const &x) { return Fixed(x.outer * unit * 0.00001, 2); };
				outer.apply = [unit](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < 0.0 || v > 2000.0)
					{
						return false;
					}

					x->outer = (int32_t)(Ftol(v * 100000.0) / unit);
					return true;
				};
				f.push_back(outer);

				Field pitch;

				pitch.title = "Pitch +/- semitones (-48 to 48)";
				pitch.show = [](SpecialObject const &x) { return Fixed(x.pitch / 128.0, 2); };
				pitch.apply = [](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < -48.0 || v > 48.0)
					{
						return false;
					}

					x->pitch = Ftol(v * 128.0);
					return true;
				};
				f.push_back(pitch);

				Field volume;

				volume.title = "Volume % (0-100)";
				volume.show = [](SpecialObject const &x) { return std::to_string((long long)Ftol(x.volume * 100.0 / 127.0 + 0.5)); };
				volume.apply = [](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < 0.0 || v > 100.0)
					{
						return false;
					}

					x->volume = Ftol((v + 50.0 / 127.0) * 0.01 * 127.0);
					return true;
				};
				f.push_back(volume);

				// the flag bit 0 is "does not play at the start" and bit 1 "does not loop": the boxes are checked when the bit is clear
				f.push_back(Box("Playing at Start", [](SpecialObject const &x) { return (x.soundFlags & 1) == 0; },
					[](SpecialObject *x, bool checked) { x->soundFlags = checked ? (x->soundFlags & ~1) : (x->soundFlags | 1); }));
				f.push_back(Box("Sound Loops", [](SpecialObject const &x) { return (x.soundFlags & 2) == 0; },
					[](SpecialObject *x, bool checked) { x->soundFlags = checked ? (x->soundFlags & ~2) : (x->soundFlags | 2); }));
			}
			else if (o.kind == SpecialKind::ParticleGenerator)
			{
				// inteng.exe FUN_004e68f0 (object-placement-reads.md): type, frequency, speed, probability, quantity, active
				f.push_back(Pick("Select Particle Generator Type", { { "Intermittent Sparks", 0 }, { "Steam", 1 }, { "Black smoke", 2 }, { "Flame", 3 } },
					[](SpecialObject const &x) { return x.particleType; }, [](SpecialObject *x, int32_t v) { x->particleType = v; }));

				Field frequency;

				frequency.title = "Frequency (seconds) (0-6000)";
				frequency.show = [](SpecialObject const &x) { return Fixed(x.time * 0.1, 1); };
				frequency.apply = [](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < 0.0 || v >= 6000.0)
					{
						return false;
					}

					x->time = (uint16_t)Ftol(v * 10.0);
					return true;
				};
				f.push_back(frequency);

				Field speed;

				speed.title = "Speed (m/s) (0-600)";
				speed.show = [](SpecialObject const &x) { return Fixed(x.speed * 0.01, 2); };
				speed.apply = [](SpecialObject *x, std::string const &typed)
				{
					double v = 0;

					if (Trim(typed).empty())
					{
						return true;
					}

					if (!ReadNumber(typed, &v) || v < 0.0 || v >= 600.0)
					{
						return false;
					}

					x->speed = (uint16_t)Ftol(v * 100.0);
					return true;
				};
				f.push_back(speed);

				f.push_back(Whole("Probability", 0, 100, [](SpecialObject const &x) { return (int32_t)x.particleProbability; }, [](SpecialObject *x, int32_t v) { x->particleProbability = (uint16_t)v; }));
				f.push_back(Whole("Quantity", 1, 1000, [](SpecialObject const &x) { return (int32_t)x.quantity; }, [](SpecialObject *x, int32_t v) { x->quantity = (uint16_t)v; }));
				// the flag bit 0 is "inactive"; the box is "Active"
				Field active = Box("Active", [](SpecialObject const &x) { return (x.particleFlags & 1u) == 0; },
					[](SpecialObject *x, bool checked) { x->particleFlags = checked ? (x->particleFlags & ~1u) : (x->particleFlags | 1u); });

				f.push_back(active);
			}

			return f;
		}

		// what the dialogs do when OK is pressed, after the last field
		void Finish(SpecialObject *o)
		{
			if (o->kind == SpecialKind::Generator)
			{
				o->flags |= kAdvanced;       // the dialog's constructor forces the advanced generator on (edobtype 0x10001ed0)
				o->hasSettings = true;
			}
			else if (o->kind == SpecialKind::BadGuy)
			{
				// edobtype FUN_10005de0 / FUN_10005400: the Texturing ID combo only holds anything for a marine; the other types read it back as "Random"
				if (o->type != 7)
				{
					o->texture = 0;
				}

				// types with a single entry in their Subtype combo read it back as that entry (value 0); a predator's is its two weapons; xenoborg and sentry gun leave it alone
				if (o->type == 1 || o->type == 3 || o->type == 5 || o->type == 6 || o->type == 8)
				{
					o->subType = 0;
				}
				else if (o->type == 2)
				{
					o->subType = (uint8_t)(o->subType & 0x3f);
				}
			}
			else if (o->kind == SpecialKind::Sound)
			{
				o->probability = 0;          // AvP's exe path is neither rainbow6 nor gunlok: the dialog hides Probability and writes 0 (edobtype 0x10006c50)
			}
			else if (o->kind == SpecialKind::ParticleGenerator)
			{
				if (o->particleType == 0)
				{
					o->speed = 0;
					o->quantity = 0;
				}
				else if (o->particleType >= 1 && o->particleType <= 3)
				{
					o->particleProbability = 0;
					o->time = 0;
				}
			}
		}

		// THE NAME OF A NEW SPECIAL OBJECT (Vasquez 2026-11-16, reassess/object-placement.md OP-24 / OP-27..OP-31): inteng asks for it BEFORE making the object: Create > AVP generator / Bad Guy / Sound / Particle
		// Generator each call FUN_004d53d0 (FUN_004d9af0 0x4daa1b, 0x4dab31, 0x4dad35, 0x4dac43), the "Name Object" dialog (FUN_004d4c70), whose typed string the constructor copies into the object's name (FUN_004dd4d0 ->
		// FUN_004e4ae0 0x4e4b0c-0x4e4b1a); Cancel makes no object. The player start takes no name dialog (0x4db273: FUN_004216d0 names it "Default name"). The editor's dialog is NameObjectDialog (editor_main.cpp); it puts
		// the typed name in mode->createName. (This replaces the empty name of the earlier reading, which had taken FUN_004d53d0 for a gate.)
		std::string CreateName(SpecialMode const *mode, SpecialKind kind)
		{
			return kind == SpecialKind::PlayerStart ? std::string("Default name") : mode->createName;
		}

		bool Fetch(OpenLevelFile *file, SpecialMode const &mode, SpecialObject *out)
		{
			std::vector<SpecialObject> all;

			if (SpecialObjects_Read(file, &all) != SpecialResult::Done)
			{
				return false;
			}

			for (SpecialObject const &each : all)
			{
				if (each.kind == mode.kind && each.number == mode.number)
				{
					*out = each;
					return true;
				}
			}

			return false;
		}

		SpecialStep Answer(std::string const &words)
		{
			SpecialStep s;

			s.answer = words;
			return s;
		}

		// the question for field `mode->step` of the draft; false when the dialog has no more fields
		bool Ask(SpecialMode *mode, SpecialStep *out)
		{
			std::vector<Field> const fields = Fields(mode->draft, mode->levelScale);

			if (mode->step >= fields.size())
			{
				return false;
			}

			Field const &f = fields[mode->step];

			out->prompt = true;
			out->title = GeneratorName(mode->draft.kind) + " - " + f.title;
			out->defaultText = f.show(mode->draft);
			mode->list.clear();

			for (size_t i = 0; i < f.choices.size(); i++)
			{
				mode->list.push_back(std::to_string((unsigned long long)(i + 1)) + "  " + f.choices[i].name);
			}

			return true;
		}
	}

	std::string SpecialObject_Words(SpecialObject const &o)
	{
		std::string words = GeneratorName(o.kind);

		if (!o.name.empty())
		{
			words += ": " + o.name;
		}
		else if (o.kind == SpecialKind::PlayerStart)
		{
			words += " " + std::to_string((unsigned long long)(o.number + 1));
		}

		return words;
	}

	bool SpecialMode_Does(int32_t command)
	{
		return command == 0x34 || command == 0x35 || command == 0x3a || command == 0x37 || command == 0x39 || command == 0x18 || command == 0x16 || command == 0x19;
	}

	void SpecialMode_Reset(SpecialMode *mode)
	{
		mode->haveCurrent = false;
		mode->number = 0;
		mode->ask = SpecialAsk::None;
		mode->step = 0;
		mode->list.clear();
		mode->found.clear();
	}

	void SpecialMode_Cancel(SpecialMode *mode)
	{
		mode->ask = SpecialAsk::None;
		mode->step = 0;
		mode->list.clear();
	}

	SpecialStep SpecialMode_Command(OpenLevelFile *file, SpecialMode *mode, int32_t command)
	{
		SpecialStep none;

		if (!SpecialMode_Does(command))
		{
			none.passOn = true;
			return none;
		}

		mode->ask = SpecialAsk::None;

		SpecialKind kind = SpecialKind::Generator;
		bool create = true;

		switch (command)
		{
		case 0x34: kind = SpecialKind::Generator; break;
		case 0x35: kind = SpecialKind::BadGuy; break;
		case 0x3a: kind = SpecialKind::PlayerStart; break;
		case 0x37: kind = SpecialKind::Sound; break;
		case 0x39: kind = SpecialKind::ParticleGenerator; break;
		default: create = false; break;
		}

		if (create)
		{
			size_t number = 0;
			std::string const name = CreateName(mode, kind);
			SpecialResult const done = SpecialObject_Create(file, kind, mode->where, mode->levelScale, name, &number);

			if (done != SpecialResult::Done)
			{
				return Answer(std::string(GeneratorName(kind)) + ": " + SpecialWords(done));
			}

			mode->haveCurrent = true;
			mode->kind = kind;
			mode->number = number;
			return Answer(std::string(GeneratorName(kind)) + (name.empty() || kind == SpecialKind::PlayerStart ? "" : " (" + name + ")") + " made at the view and selected; Object > Edit changes it, Object > Delete takes it away");
		}

		if (command == 0x19)
		{
			std::vector<SpecialObject> all;

			mode->found.clear();
			mode->list.clear();
			SpecialObjects_Read(file, &all);

			for (SpecialObject const &each : all)
			{
				// Object Placement's Find (FUN_004d9af0 0x4da337: FUN_00475bf0 "Select Object" over the objects of the placement list) lists every special object; the sounds screen's only the sounds
				if (!mode->soundsOnly || each.kind == SpecialKind::Sound)
				{
					mode->found.push_back(each);
					mode->list.push_back(std::to_string((unsigned long long)mode->found.size()) + "  " + SpecialObject_Words(each) + (each.wav.empty() ? "" : " (" + each.wav + ")"));
				}
			}

			if (mode->found.empty())
			{
				return Answer(mode->soundsOnly ? "Find: this level has no sounds" : "Find: this level has no special objects");
			}

			mode->ask = SpecialAsk::PickObject;

			SpecialStep s;

			s.prompt = true;
			s.title = "Select Object";
			s.defaultText = "1";
			return s;
		}

		if (!mode->haveCurrent)
		{
			none.passOn = true;
			return none;
		}

		if (command == 0x16)
		{
			SpecialResult const done = SpecialObject_Delete(file, mode->kind, mode->number);
			std::string const words = GeneratorName(mode->kind);

			mode->haveCurrent = false;
			return Answer(done == SpecialResult::Done ? words + " deleted" : words + ": " + SpecialWords(done));
		}

		// Edit
		SpecialObject current;

		if (!Fetch(file, *mode, &current))
		{
			mode->haveCurrent = false;
			return Answer("Edit: the selected object is no longer in the level");
		}

		if (current.kind == SpecialKind::PlayerStart)
		{
			return Answer("Player starting position has no settings to edit (inteng's has no dialog); Move relative or the view's place sets it");
		}

		mode->draft = current;
		mode->step = 0;
		mode->ask = SpecialAsk::Field;

		SpecialStep s;

		if (!mode->useForm)
		{
			Ask(mode, &s);
			return s;
		}

		s.form = true;   // the object's dialog is one window with all its fields (SpecialMode_FormItems), as edobtype.dll's and inteng's are
		return s;
	}

	SpecialStep SpecialMode_MoveCurrent(OpenLevelFile *file, SpecialMode *mode, int32_t const place[3])
	{
		SpecialObject current;

		if (!mode->haveCurrent || !Fetch(file, *mode, &current))
		{
			return Answer("Snap To Vertex: no special object is selected");
		}

		memcpy(current.place, place, sizeof(current.place));

		SpecialResult const done = SpecialObject_Write(file, current);

		return Answer(done == SpecialResult::Done ? GeneratorName(current.kind) + " moved onto the vertex" : GeneratorName(current.kind) + ": " + SpecialWords(done));
	}

	SpecialStep SpecialMode_SetParent(OpenLevelFile *file, SpecialMode *mode, uint8_t const id[8])
	{
		SpecialObject current;

		if (!mode->haveCurrent || mode->kind != SpecialKind::ParticleGenerator || !Fetch(file, *mode, &current))
		{
			return Answer("Set particle generator parent: select a particle generator first");
		}

		memcpy(current.parentId, id, 8);

		SpecialResult const done = SpecialObject_Write(file, current);

		return Answer(done == SpecialResult::Done ? "Particle generator parent set" : std::string("Particle generator parent: ") + SpecialWords(done));
	}

	bool SpecialMode_FormItems(SpecialMode const &mode, std::string *title, std::vector<SpecialFormItem> *items)
	{
		if (mode.ask != SpecialAsk::Field || !items)
		{
			return false;
		}

		std::vector<Field> const fields = Fields(mode.draft, mode.levelScale);

		items->clear();

		for (Field const &f : fields)
		{
			if (f.title.compare(0, 4, "Name") == 0)
			{
				continue;   // the DLLs' dialogs have no name control
			}

			SpecialFormItem item;

			item.label = f.title;
			item.kind = f.kind;
			item.value = f.show(mode.draft);

			if (f.kind == 1 && item.label.size() > 6 && item.label.compare(item.label.size() - 6, 6, " (y/n)") == 0)
			{
				item.label.resize(item.label.size() - 6);
			}

			items->push_back(item);
		}

		if (title)
		{
			// the DLLs' captions: resource 133 "Generator Settings", 129 "Edit object properties", 132 "Sound Edit" with the object's name after it (the title is "Sound Edit - %s", the name the struct points to)
			*title = mode.draft.kind == SpecialKind::Generator ? "Generator Settings" : mode.draft.kind == SpecialKind::BadGuy ? "Edit object properties" :
				mode.draft.kind == SpecialKind::Sound ? "Sound Edit - " + mode.draft.name : GeneratorName(mode.draft.kind);
		}

		return true;
	}

	SpecialStep SpecialMode_FormAnswer(OpenLevelFile *file, SpecialMode *mode, std::vector<std::string> const &values)
	{
		if (mode->ask != SpecialAsk::Field)
		{
			return Answer(std::string());
		}

		SpecialObject draft = mode->draft;
		std::vector<std::string> shown;

		for (Field const &each : Fields(mode->draft, mode->levelScale))
		{
			shown.push_back(each.title);
		}

		for (size_t v = 0; v < values.size(); v++)
		{
			std::vector<Field> const fields = Fields(draft, mode->levelScale);

			// the answers are the controls of the window, which has no Name control: they go to the fields after it
			size_t const i = v + (!fields.empty() && fields[0].title.compare(0, 4, "Name") == 0 ? 1 : 0);

			if (i >= fields.size())
			{
				break;
			}

			SpecialObject next = draft;
			bool good = fields[i].apply(&next, values[v]);
			std::string why = fields[i].title + ": out of range";

			if (good && fields[i].kind == 0 && fields[i].title.compare(0, 4, "Name") == 0 && !SpecialName_Good(next.name))
			{
				good = false;
				why = "Name: plain letters only, 63 at most";
			}

			if (!good)
			{
				mode->draft = draft;

				SpecialStep again;

				again.form = true;
				again.answer = why;
				return again;
			}

			draft = next;

			// the controls under a choice that was changed are other controls now: the window is shown again at once, the answers after it are not applied to them
			std::vector<std::string> after;

			for (Field const &each : Fields(draft, mode->levelScale))
			{
				after.push_back(each.title);
			}

			if (after != shown)
			{
				mode->draft = draft;

				SpecialStep again;

				again.form = true;
				return again;
			}
		}

		// A choice that changes the controls under it (the Bad Guy's type: its subtype, texturing and flags are another set for each) is the dialog's combo box changing them as it is chosen: the window is shown again
		// with the new controls, and OK on that one writes
		std::vector<std::string> now;

		for (Field const &each : Fields(draft, mode->levelScale))
		{
			now.push_back(each.title);
		}

		if (now != shown)
		{
			mode->draft = draft;

			SpecialStep again;

			again.form = true;
			return again;
		}

		SpecialObject final = draft;

		Finish(&final);

		SpecialResult const done = SpecialObject_Write(file, final);
		std::string const words = GeneratorName(final.kind) + (final.name.empty() ? "" : " (" + final.name + ")");

		SpecialMode_Cancel(mode);
		return Answer(done == SpecialResult::Done ? words + " saved" : words + ": " + SpecialWords(done));
	}

	SpecialStep SpecialMode_Typed(OpenLevelFile *file, SpecialMode *mode, std::string const &typed)
	{
		if (mode->ask == SpecialAsk::PickObject)
		{
			std::string const t = Lower(Trim(typed));
			int at = -1;
			double number = 0;

			if (ReadNumber(t, &number) && number >= 1 && number <= (double)mode->found.size() && number == floor(number))
			{
				at = (int)number - 1;
			}
			else
			{
				for (size_t i = 0; i < mode->found.size() && !t.empty(); i++)
				{
					if (Lower(SpecialObject_Words(mode->found[i])).find(t) != std::string::npos)
					{
						if (at >= 0)
						{
							at = -2;
							break;
						}

						at = (int)i;
					}
				}
			}

			if (at < 0)
			{
				SpecialStep again;

				again.prompt = true;
				again.title = at == -2 ? "Find a sound - more than one fits, give its number" : "Find a sound - not in the list, give its number";
				again.defaultText = typed;
				return again;
			}

			mode->haveCurrent = true;
			mode->kind = mode->found[(size_t)at].kind;
			mode->number = mode->found[(size_t)at].number;
			mode->ask = SpecialAsk::None;
			mode->list.clear();
			return Answer(SpecialObject_Words(mode->found[(size_t)at]) + " selected");
		}

		if (mode->ask != SpecialAsk::Field)
		{
			return Answer(std::string());
		}

		std::vector<Field> const fields = Fields(mode->draft, mode->levelScale);

		if (mode->step >= fields.size())
		{
			SpecialMode_Cancel(mode);
			return Answer(std::string());
		}

		SpecialObject next = mode->draft;
		bool const good = fields[mode->step].apply(&next, typed);

		if (!good)
		{
			// the dialog leaves a field that is out of range as it was
			SpecialStep again;

			Ask(mode, &again);
			again.title += "  [out of range - try again or Esc]";
			again.defaultText = typed;
			return again;
		}

		// a name is 0 to 63 plain letters (and not empty for a generator, which the game finds by it)
		if (mode->step == 0 && !SpecialName_Good(next.name))
		{
			SpecialStep again;

			Ask(mode, &again);
			again.title += "  [plain letters only, 63 at most]";
			return again;
		}

		mode->draft = next;
		mode->step++;

		SpecialStep s;

		if (Ask(mode, &s))
		{
			return s;
		}

		// OK
		SpecialObject final = mode->draft;

		Finish(&final);

		SpecialResult const done = SpecialObject_Write(file, final);
		std::string const words = GeneratorName(final.kind) + (final.name.empty() ? "" : " (" + final.name + ")");

		SpecialMode_Cancel(mode);
		return Answer(done == SpecialResult::Done ? words + " saved" : words + ": " + SpecialWords(done));
	}
}
