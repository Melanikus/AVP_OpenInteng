// shot_read.cpp: a screenshot's context read back, every field bounded (shot_read.h says what and why). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "shot_read.h"

#include <string.h>

#include <utility>
#include <vector>

#include "png_meta.h"

namespace host
{
	namespace
	{
		struct Value
		{
			enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
			std::string text;                                    // a string's bytes (\u00XX made the byte XX), or a number's own text
			std::vector<std::pair<std::string, Value>> members;  // an object's, in order
			std::vector<Value> items;                            // an array's
		};

		size_t const kMostDepth = 64;
		size_t const kMostValues = 4096;
		size_t const kMostString = 4096;

		struct Parser
		{
			std::string const &json;
			size_t at = 0;
			size_t values = 0;
			std::string why;

			explicit Parser(std::string const &text) : json(text) {}

			bool Fail(char const *words)
			{
				if (why.empty())
				{
					why = std::string(words) + " at byte " + std::to_string((unsigned long long)at);
				}

				return false;
			}

			void Space()
			{
				while (at < json.size() && (json[at] == ' ' || json[at] == '\t' || json[at] == '\r' || json[at] == '\n'))
				{
					at++;
				}
			}

			static int Hex(char c)
			{
				return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
			}

			bool StringValue(std::string *out)
			{
				if (at >= json.size() || json[at] != '"')
				{
					return Fail("a string was looked for");
				}

				at++;
				out->clear();

				while (at < json.size() && json[at] != '"')
				{
					if (out->size() >= kMostString)
					{
						return Fail("a string longer than 4096 bytes");
					}

					char c = json[at++];

					if ((unsigned char)c < 0x20)
					{
						return Fail("a control byte in a string");
					}

					if (c == '\\')
					{
						if (at >= json.size())
						{
							return Fail("a string's escape cut off");
						}

						char const e = json[at++];

						if (e == 'u')
						{
							if (at + 4 > json.size())
							{
								return Fail("a \\u escape cut off");
							}

							int value = 0;

							for (int k = 0; k < 4; k++)
							{
								int const h = Hex(json[at + k]);

								if (h < 0)
								{
									return Fail("a \\u escape that is not hex");
								}

								value = value * 16 + h;
							}

							at += 4;

							if (value == 0 || value > 0xff)
							{
								return Fail("a \\u escape outside 1 to 255 (the shot writes only \\u00XX)");
							}

							c = (char)value;
						}
						else
						{
							switch (e)
							{
							case '"': c = '"'; break;
							case '\\': c = '\\'; break;
							case '/': c = '/'; break;
							case 'b': c = '\b'; break;
							case 'f': c = '\f'; break;
							case 'n': c = '\n'; break;
							case 'r': c = '\r'; break;
							case 't': c = '\t'; break;
							default: return Fail("an unknown escape in a string");
							}
						}
					}

					out->push_back(c);
				}

				if (at >= json.size())
				{
					return Fail("a string with no end");
				}

				at++;
				return true;
			}

			bool Parse(Value *out, size_t depth)
			{
				if (depth > kMostDepth)
				{
					return Fail("nested deeper than 64");
				}

				if (++values > kMostValues)
				{
					return Fail("more than 4096 values");
				}

				Space();

				if (at >= json.size())
				{
					return Fail("the text ends where a value was looked for");
				}

				char const c = json[at];

				if (c == '{' || c == '[')
				{
					bool const object = c == '{';

					out->kind = object ? Value::Object : Value::Array;
					at++;
					Space();

					if (at < json.size() && json[at] == (object ? '}' : ']'))
					{
						at++;
						return true;
					}

					while (true)
					{
						Space();

						if (object)
						{
							std::string key;
							Value member;

							if (!StringValue(&key))
							{
								return false;
							}

							Space();

							if (at >= json.size() || json[at] != ':')
							{
								return Fail("a member with no ':'");
							}

							at++;

							if (!Parse(&member, depth + 1))
							{
								return false;
							}

							out->members.emplace_back(key, std::move(member));
						}
						else
						{
							Value item;

							if (!Parse(&item, depth + 1))
							{
								return false;
							}

							out->items.push_back(std::move(item));
						}

						Space();

						if (at < json.size() && json[at] == ',')
						{
							at++;
							continue;
						}

						if (at < json.size() && json[at] == (object ? '}' : ']'))
						{
							at++;
							return true;
						}

						return Fail(object ? "an object with no '}'" : "an array with no ']'");
					}
				}

				if (c == '"')
				{
					out->kind = Value::String;
					return StringValue(&out->text);
				}

				if (json.compare(at, 4, "true") == 0 || json.compare(at, 4, "null") == 0)
				{
					out->kind = json[at] == 'n' ? Value::Null : Value::Bool;
					at += 4;
					return true;
				}

				if (json.compare(at, 5, "false") == 0)
				{
					out->kind = Value::Bool;
					at += 5;
					return true;
				}

				size_t const start = at;

				while (at < json.size() && at - start < 32 && (json[at] == '-' || json[at] == '+' || json[at] == '.' || json[at] == 'e' || json[at] == 'E' || (json[at] >= '0' && json[at] <= '9')))
				{
					at++;
				}

				if (at == start)
				{
					return Fail("a value that is not JSON");
				}

				out->kind = Value::Number;
				out->text = json.substr(start, at - start);
				return true;
			}
		};

		Value const *Member(Value const &object, char const *name)
		{
			for (auto const &one : object.members)
			{
				if (one.first == name)
				{
					return &one.second;
				}
			}

			return nullptr;
		}

		// a whole number's text (an optional -, then 1 to 10 digits) within [least, most]
		bool WholeText(std::string const &text, long long least, long long most, int32_t *out)
		{
			if (text.empty() || text.size() > 11)
			{
				return false;
			}

			size_t k = text[0] == '-' ? 1 : 0;
			long long n = 0;

			if (k == text.size())
			{
				return false;
			}

			for (; k < text.size(); k++)
			{
				if (text[k] < '0' || text[k] > '9')
				{
					return false;
				}

				n = n * 10 + (text[k] - '0');
			}

			n = text[0] == '-' ? -n : n;

			if (n < least || n > most)
			{
				return false;
			}

			*out = (int32_t)n;
			return true;
		}

		bool WholeNumber(Value const *value, long long least, long long most, int32_t *out)
		{
			return value && value->kind == Value::Number && WholeText(value->text, least, most, out);
		}

		bool Printable(std::string const &text, size_t most)
		{
			if (text.empty() || text.size() > most)
			{
				return false;
			}

			for (char const c : text)
			{
				if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7e)
				{
					return false;
				}
			}

			return true;
		}
	}

	bool ShotRead_PlainName(std::string const &name)
	{
		return Printable(name, 128) && name != "." && name != ".." && name.find_first_of("\\/:*?\"<>|") == std::string::npos;
	}

	bool ShotRead_Json(std::string const &json, ShotPlace *out, std::string *why)
	{
		Parser parser(json);
		Value root;
		ShotPlace made;
		int32_t format = 0;

		if (!parser.Parse(&root, 0) || root.kind != Value::Object)
		{
			*why = "the context is not one JSON object: " + parser.why;
			return false;
		}

		if (!WholeNumber(Member(root, "format"), 1, 1, &format))
		{
			*why = "the context's format is not 1";
			return false;
		}

		Value const *const map = Member(root, "map");
		Value const *const sha = Member(root, "level_sha256");
		Value const *const screen = Member(root, "screen");
		Value const *const pos = Member(root, "pos");
		Value const *const selected = Member(root, "selected");

		if (!map || map->kind != Value::String || !ShotRead_PlainName(map->text))
		{
			*why = "the map is not a plain file name of 1 to 128 printable bytes";
			return false;
		}

		bool hex = sha && sha->kind == Value::String && sha->text.size() == 64;

		for (size_t k = 0; hex && k < 64; k++)
		{
			hex = (sha->text[k] >= '0' && sha->text[k] <= '9') || (sha->text[k] >= 'a' && sha->text[k] <= 'f');
		}

		if (!hex)
		{
			*why = "level_sha256 is not 64 lower-case hex letters (the editor could not read the level when it took the shot)";
			return false;
		}

		if (!screen || screen->kind != Value::String || !Printable(screen->text, 64))
		{
			*why = "the screen is not 1 to 64 printable bytes";
			return false;
		}

		if (!pos || pos->kind != Value::Object || !WholeNumber(Member(*pos, "x"), -kShotReadMostPlace, kShotReadMostPlace, &made.pos[0]) ||
			!WholeNumber(Member(*pos, "y"), -kShotReadMostPlace, kShotReadMostPlace, &made.pos[1]) || !WholeNumber(Member(*pos, "z"), -kShotReadMostPlace, kShotReadMostPlace, &made.pos[2]))
		{
			*why = "pos is not {x, y, z} whole numbers within 16,777,216 (2^24, the camera's limit)";
			return false;
		}

		if (!WholeNumber(Member(root, "turn"), 0, 4095, &made.turn) || !WholeNumber(Member(root, "look"), -kShotReadMostLook, kShotReadMostLook, &made.look))
		{
			*why = "turn is not 0 to 4095, or look not -1024 to 1024";
			return false;
		}

		if (!selected || selected->kind != Value::Array)
		{
			*why = "selected is not a list";
			return false;
		}

		if (!selected->items.empty())
		{
			Value const &first = selected->items[0];

			if (first.kind != Value::String || !Printable(first.text, 128))
			{
				*why = "the first selected name is not 1 to 128 printable ASCII bytes";
				return false;
			}

			made.selected = first.text;
		}

		made.map = map->text;
		made.sha256 = sha->text;
		made.screen = screen->text;
		*out = made;
		return true;
	}

	bool ShotRead_File(char const *path, ShotPlace *out, std::string *why)
	{
		std::string json, pngWhy;
		PngMetaResult const read = PngMeta_ReadFile(path, &json, &pngWhy);

		if (read != PngMetaResult::Done)
		{
			*why = std::string("the picture's context is refused: ") + PngMetaWords(read) + (pngWhy.empty() ? "" : ": " + pngWhy);
			return false;
		}

		return ShotRead_Json(json, out, why);
	}

	bool ShotRead_Numbers(char const *const words[5], ShotPlace *out, std::string *why)
	{
		ShotPlace made = *out;

		for (int k = 0; k < 3; k++)
		{
			if (!words[k] || !WholeText(words[k], -kShotReadMostPlace, kShotReadMostPlace, &made.pos[k]))
			{
				*why = "-at takes x y z turn look: x, y and z whole numbers of millimetres within 16,777,216 (2^24, the camera's limit)";
				return false;
			}
		}

		if (!words[3] || !WholeText(words[3], 0, 4095, &made.turn) || !words[4] || !WholeText(words[4], -kShotReadMostLook, kShotReadMostLook, &made.look))
		{
			*why = "-at takes x y z turn look: turn 0 to 4095 (0 looks along z, 1024 along x), look -1024 to 1024 (a quarter turn up or down)";
			return false;
		}

		*out = made;
		return true;
	}
}
