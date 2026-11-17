// See special_objects_edit.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "special_objects_edit.h"

#include <string.h>

#include "level_file.h"
#include "level_image.h"
#include "md5.h"

namespace host
{
	using namespace image;

	namespace
	{
		size_t const kMostName = 63;

		bool Plain(std::string const &text)
		{
			if (text.size() > kMostName)
			{
				return false;
			}

			for (char const c : text)
			{
				if ((unsigned char)c < 32 || (unsigned char)c > 126)
				{
					return false;
				}
			}

			return true;
		}

		void AddBytes(Bytes *out, void const *data, size_t size)
		{
			char const *const from = (char const *)data;

			out->insert(out->end(), from, from + size);
		}

		void AddText(Bytes *out, std::string const &text, bool padTo4)
		{
			AddBytes(out, text.c_str(), text.size() + 1);

			if (padTo4)
			{
				// (length + 4) & ~3 bytes in all, the way the game's chunks write a name
				size_t const want = (text.size() + 4) & ~(size_t)3;

				for (size_t i = text.size() + 1; i < want; i++)
				{
					out->push_back(0);
				}
			}
		}

		// The level's REBENVDT and SPECLOBJ. specAt == inside.size() when there is no SPECLOBJ.
		struct Where
		{
			Bytes image;
			std::vector<Span> top, inside, special;
			size_t envAt = 0, specAt = 0;
		};

		SpecialResult Find(OpenLevelFile *file, Where *w)
		{
			if (!file || !file->Level())
			{
				return SpecialResult::BadCall;
			}

			if (!Walk(file, &w->image, &w->top))
			{
				return SpecialResult::CantBuild;
			}

			w->envAt = w->top.size();

			for (size_t i = 0; i < w->top.size() && w->envAt == w->top.size(); i++)
			{
				if (NameIs(w->image, w->top[i].at, "REBENVDT"))
				{
					w->envAt = i;
				}
			}

			if (w->envAt == w->top.size())
			{
				return SpecialResult::NoEnvironment;
			}

			if (!Children(w->image, w->top[w->envAt].at + kHeadingBytes, w->top[w->envAt].at + w->top[w->envAt].size, &w->inside))
			{
				return SpecialResult::CantBuild;
			}

			w->specAt = FirstNamed(w->image, w->inside, "SPECLOBJ");
			w->special.clear();

			if (w->specAt != w->inside.size() && !Children(w->image, w->inside[w->specAt].at + kHeadingBytes, w->inside[w->specAt].at + w->inside[w->specAt].size, &w->special))
			{
				return SpecialResult::CantBuild;
			}

			return SpecialResult::Done;
		}

		// The level with SPECLOBJ's children replaced by `chunks` (SPECLOBJ made at the end of REBENVDT when the level has none), every size right
		SpecialResult Rebuilt(OpenLevelFile *file, Where const &w, std::vector<Bytes> const &chunks)
		{
			Bytes spec;

			if (w.specAt != w.inside.size())
			{
				spec.assign(w.image.begin() + (std::ptrdiff_t)w.inside[w.specAt].at, w.image.begin() + (std::ptrdiff_t)(w.inside[w.specAt].at + kHeadingBytes));
			}
			else
			{
				spec.assign(kHeadingBytes, 0);
				memcpy(spec.data(), "SPECLOBJ", 8);
			}

			for (Bytes const &one : chunks)
			{
				spec.insert(spec.end(), one.begin(), one.end());
			}

			PutIntAt(&spec, 8, (int32_t)spec.size());

			Span const env = w.top[w.envAt];
			Bytes newEnv(w.image.begin() + (std::ptrdiff_t)env.at, w.image.begin() + (std::ptrdiff_t)(env.at + kHeadingBytes));

			for (size_t i = 0; i < w.inside.size(); i++)
			{
				if (i == w.specAt)
				{
					newEnv.insert(newEnv.end(), spec.begin(), spec.end());
				}
				else
				{
					AddCopy(&newEnv, w.image, w.inside[i]);
				}
			}

			if (w.specAt == w.inside.size())
			{
				newEnv.insert(newEnv.end(), spec.begin(), spec.end());
			}

			PutIntAt(&newEnv, 8, (int32_t)newEnv.size());

			Bytes out(w.image.begin(), w.image.begin() + (std::ptrdiff_t)env.at);

			out.insert(out.end(), newEnv.begin(), newEnv.end());
			out.insert(out.end(), w.image.begin() + (std::ptrdiff_t)(env.at + env.size), w.image.end());
			PutIntAt(&out, 8, (int32_t)out.size());

			LevelOpenResult const result = file->ReplaceLevel(out);

			if (result == LevelOpenResult::Opened)
			{
				return SpecialResult::Done;
			}

			return result == LevelOpenResult::CantRead ? SpecialResult::CantBuild : SpecialResult::LoaderRefused;
		}

		// ---- reading one chunk (false when it is too short for its kind)
		bool ReadGenerator(Bytes const &b, Span const &c, SpecialObject *o)
		{
			size_t const at = c.at + kHeadingBytes;
			size_t const length = c.size - kHeadingBytes;

			if (length < 28 + 1)
			{
				return false;
			}

			for (int i = 0; i < 3; i++)
			{
				o->place[i] = IntAt(b, at + (size_t)i * 4);
			}

			o->heading = IntAt(b, at + 12);
			o->type = IntAt(b, at + 16);
			o->flags = (uint32_t)IntAt(b, at + 20);
			o->texture = (uint8_t)b[at + 24];
			o->subType = (uint8_t)b[at + 25];
			o->extra1 = (uint8_t)b[at + 26];
			o->extra2 = (uint8_t)b[at + 27];

			char const *const first = b.data() + at + 28;
			char const *const zero = (char const *)memchr(first, 0, length - 28);

			if (!zero)
			{
				return false;
			}

			o->name.assign(first, (size_t)(zero - first));
			o->kind = o->type == 0 ? SpecialKind::Generator : SpecialKind::BadGuy;
			return true;
		}

		bool ReadStart(Bytes const &b, Span const &c, SpecialObject *o)
		{
			size_t const at = c.at + kHeadingBytes;

			if (c.size - kHeadingBytes < 12 + 36 + 8)
			{
				return false;
			}

			for (int i = 0; i < 3; i++)
			{
				o->place[i] = IntAt(b, at + (size_t)i * 4);
			}

			for (int i = 0; i < 9; i++)
			{
				o->matrix[i] = IntAt(b, at + 12 + (size_t)i * 4);
			}

			memcpy(o->module, b.data() + at + 48, 8);
			o->kind = SpecialKind::PlayerStart;
			return true;
		}

		bool ReadSound(Bytes const &b, Span const &c, SpecialObject *o)
		{
			size_t const at = c.at + kHeadingBytes;
			size_t const length = c.size - kHeadingBytes;

			if (length < 40 + 1)
			{
				return false;
			}

			for (int i = 0; i < 3; i++)
			{
				o->place[i] = IntAt(b, at + (size_t)i * 4);
			}

			o->inner = IntAt(b, at + 12);
			o->outer = IntAt(b, at + 16);
			o->volume = IntAt(b, at + 20);
			o->pitch = IntAt(b, at + 24);
			o->soundFlags = IntAt(b, at + 28);
			o->probability = IntAt(b, at + 32);

			char const *const first = b.data() + at + 40;
			char const *const zero = (char const *)memchr(first, 0, length - 40);

			if (!zero)
			{
				return false;
			}

			o->name.assign(first, (size_t)(zero - first));

			size_t const wavAt = (size_t)(zero - b.data()) + 1;
			size_t const end = c.at + c.size;

			if (wavAt < end)
			{
				char const *const wavZero = (char const *)memchr(b.data() + wavAt, 0, end - wavAt);

				o->wav.assign(b.data() + wavAt, wavZero ? (size_t)(wavZero - (b.data() + wavAt)) : end - wavAt);
			}

			SoundObjectId(o->name, o->id);   // the ID a strategy names it by (the chunk holds none; nothing writes this back)
			o->kind = SpecialKind::Sound;
			return true;
		}

		bool ReadParticle(Bytes const &b, Span const &c, SpecialObject *o)
		{
			std::vector<Span> parts;

			if (!Children(b, c.at + kHeadingBytes, c.at + c.size, &parts))
			{
				return false;
			}

			size_t const dataAt = FirstNamed(b, parts, "PARGENDA");

			if (dataAt == parts.size() || parts[dataAt].size < kHeadingBytes + 68 + 1)
			{
				return false;
			}

			size_t const at = parts[dataAt].at + kHeadingBytes;
			size_t const length = parts[dataAt].size - kHeadingBytes;

			o->particleType = IntAt(b, at);
			o->particleFlags = (uint32_t)IntAt(b, at + 4);
			memcpy(&o->time, b.data() + at + 8, 2);
			memcpy(&o->particleProbability, b.data() + at + 10, 2);
			memcpy(&o->speed, b.data() + at + 12, 2);
			memcpy(&o->quantity, b.data() + at + 14, 2);
			memcpy(o->id, b.data() + at + 24, 8);
			memcpy(o->parentId, b.data() + at + 32, 8);

			for (int i = 0; i < 3; i++)
			{
				o->place[i] = IntAt(b, at + 40 + (size_t)i * 4);
			}

			memcpy(o->turn, b.data() + at + 52, 16);

			char const *const first = b.data() + at + 68;
			char const *const zero = (char const *)memchr(first, 0, length - 68);

			if (!zero)
			{
				return false;
			}

			o->name.assign(first, (size_t)(zero - first));

			size_t const sound = FirstNamed(b, parts, "INDSOUND");

			o->hasSound = sound != parts.size();

			// its wav name: after the index (4 bytes), to the first 0 inside the chunk (a name with no 0 in the chunk is not taken)
			if (o->hasSound && parts[sound].size >= kHeadingBytes + 5)
			{
				char const *const wav = b.data() + parts[sound].at + kHeadingBytes + 4;
				char const *const end = (char const *)memchr(wav, 0, parts[sound].size - kHeadingBytes - 4);

				o->particleSoundWav = end ? std::string(wav, (size_t)(end - wav)) : std::string();

				// THE REST, as Indexed_Sound_Chunk reads it (chunk_sound.cpp:232): the name takes (length + 4) & ~3 bytes, then inner, outer, volume, pitch, flags, the count of extra words
				// and the words. HOSTILE INPUT: every read is checked against the chunk's own end, and a count is taken only when that many words are there; anything short leaves
				// particleSoundRead false (the INDSOUND is then written back as it was)
				size_t const soundEnd = parts[sound].at + parts[sound].size;
				size_t const afterName = end ? (size_t)(end - b.data()) - (parts[sound].at + kHeadingBytes + 4) : 0;
				size_t at2 = parts[sound].at + kHeadingBytes + 4 + ((afterName + 4) & ~(size_t)3);

				if (end && at2 <= soundEnd && soundEnd - at2 >= 24)
				{
					o->particleSoundIndex = IntAt(b, parts[sound].at + kHeadingBytes);
					o->particleSoundInner = (uint32_t)IntAt(b, at2);
					o->particleSoundOuter = (uint32_t)IntAt(b, at2 + 4);
					o->particleSoundVolume = IntAt(b, at2 + 8);
					o->particleSoundPitch = IntAt(b, at2 + 12);
					o->particleSoundFlags = IntAt(b, at2 + 16);

					int32_t const count = IntAt(b, at2 + 20);

					at2 += 24;

					if (count >= 0 && (size_t)count <= (soundEnd - at2) / 4)
					{
						for (int32_t i = 0; i < count; i++)
						{
							o->particleSoundExtra.push_back(IntAt(b, at2 + (size_t)i * 4));
						}

						o->particleSoundRead = true;
					}
				}
			}

			o->kind = SpecialKind::ParticleGenerator;
			return true;
		}

		// ---- making one chunk from an object
		Bytes MakeGenerator(SpecialObject const &o)
		{
			Bytes p;

			for (int i = 0; i < 3; i++)
			{
				AddInt(&p, o.place[i]);
			}

			AddInt(&p, o.heading);
			AddInt(&p, o.kind == SpecialKind::Generator ? 0 : o.type);
			AddInt(&p, (int32_t)o.flags);
			p.push_back((char)o.texture);
			p.push_back((char)o.subType);
			p.push_back((char)o.extra1);
			p.push_back((char)o.extra2);
			AddText(&p, o.name, true);
			return MakeChunk("AVPGENER", p);
		}

		Bytes MakeStart(SpecialObject const &o)
		{
			Bytes p;

			for (int i = 0; i < 3; i++)
			{
				AddInt(&p, o.place[i]);
			}

			for (int i = 0; i < 9; i++)
			{
				AddInt(&p, o.matrix[i]);
			}

			AddBytes(&p, o.module, 8);
			return MakeChunk("AVPSTART", p);
		}

		Bytes MakeSound(SpecialObject const &o)
		{
			Bytes p;

			for (int i = 0; i < 3; i++)
			{
				AddInt(&p, o.place[i]);
			}

			AddInt(&p, o.inner);
			AddInt(&p, o.outer);
			AddInt(&p, o.volume);
			AddInt(&p, o.pitch);
			AddInt(&p, o.soundFlags);
			AddInt(&p, o.probability);
			AddInt(&p, 0);
			AddBytes(&p, o.name.c_str(), o.name.size() + 1);
			AddBytes(&p, o.wav.c_str(), o.wav.size() + 1);

			// Sound_Object_Chunk::size_chunk: the size so far (heading 12 + the numbers + the name + 1 + the wav's length, or 1 for none) gets 4 - size % 4 more, so a chunk always ends with 1 to 4 zero
			// bytes; with a wav name that is one less than the bytes written above (they hold its terminator)
			size_t const base = kHeadingBytes + p.size() - (o.wav.empty() ? 0 : 1);
			size_t const wanted = base + 4 - base % 4;

			while (kHeadingBytes + p.size() < wanted)
			{
				p.push_back(0);
			}

			return MakeChunk("SOUNDOB2", p);
		}

		Bytes MakeParticleData(SpecialObject const &o)
		{
			Bytes p;

			AddInt(&p, o.particleType);
			AddInt(&p, (int32_t)o.particleFlags);
			AddBytes(&p, &o.time, 2);
			AddBytes(&p, &o.particleProbability, 2);
			AddBytes(&p, &o.speed, 2);
			AddBytes(&p, &o.quantity, 2);
			AddInt(&p, 0);
			AddInt(&p, 0);
			AddBytes(&p, o.id, 8);
			AddBytes(&p, o.parentId, 8);

			for (int i = 0; i < 3; i++)
			{
				AddInt(&p, o.place[i]);
			}

			AddBytes(&p, o.turn, 16);
			AddText(&p, o.name, true);
			return MakeChunk("PARGENDA", p);
		}

		// The chunk of an object; a particle generator keeps the children of `old` other than PARGENDA when old is not null
		Bytes MakeChunkFor(SpecialObject const &o, Bytes const *image, Span const *old)
		{
			switch (o.kind)
			{
			case SpecialKind::Generator:
			case SpecialKind::BadGuy: return MakeGenerator(o);
			case SpecialKind::PlayerStart: return MakeStart(o);
			case SpecialKind::Sound: return MakeSound(o);
			case SpecialKind::ParticleGenerator:
			{
				Bytes p = MakeParticleData(o);

				// its other children as they were, but the INDSOUND: inteng's save deletes it and makes it again from the sound record when there is one (FUN_004e6dd0 0x4e70b9-0x4e70dd,
				// FUN_004ec1b0), so it comes last; no sound (Remove Sound), none. One that was never read whole is copied as it was.
				bool const remade = o.hasSound && o.particleSoundRead;

				if (image && old)
				{
					std::vector<Span> parts;

					if (Children(*image, old->at + kHeadingBytes, old->at + old->size, &parts))
					{
						for (Span const &part : parts)
						{
							bool const isSound = NameIs(*image, part.at, "INDSOUND");

							if (!NameIs(*image, part.at, "PARGENDA") && (!isSound || (o.hasSound && !remade)))
							{
								AddCopy(&p, *image, part);
							}
						}
					}
				}

				if (remade)
				{
					Bytes s;

					AddInt(&s, o.particleSoundIndex);
					AddBytes(&s, o.particleSoundWav.c_str(), o.particleSoundWav.size() + 1);

					while (s.size() % 4 != 0)
					{
						s.push_back(0);   // CHUNK_FILL_STRING: the name and its 0, to a whole 4 bytes ((length + 4) & ~3)
					}

					AddInt(&s, (int32_t)o.particleSoundInner);
					AddInt(&s, (int32_t)o.particleSoundOuter);
					AddInt(&s, o.particleSoundVolume);
					AddInt(&s, o.particleSoundPitch);
					AddInt(&s, o.particleSoundFlags);
					AddInt(&s, (int32_t)o.particleSoundExtra.size());

					for (int32_t const word : o.particleSoundExtra)
					{
						AddInt(&s, word);
					}

					Bytes const made = MakeChunk("INDSOUND", s);

					p.insert(p.end(), made.begin(), made.end());
				}

				Bytes chunk(kHeadingBytes, 0);

				memcpy(chunk.data(), "PARGENER", 8);
				chunk.insert(chunk.end(), p.begin(), p.end());
				PutIntAt(&chunk, 8, (int32_t)chunk.size());
				return chunk;
			}
			}

			return Bytes();
		}

		bool IsKindChunk(Bytes const &b, Span const &c, SpecialKind kind, SpecialObject *read)
		{
			SpecialObject one;

			switch (kind)
			{
			case SpecialKind::Generator:
			case SpecialKind::BadGuy:
				if (!NameIs(b, c.at, "AVPGENER") || !ReadGenerator(b, c, &one) || one.kind != kind)
				{
					return false;
				}
				break;

			case SpecialKind::PlayerStart:
				if (!NameIs(b, c.at, "AVPSTART") || !ReadStart(b, c, &one))
				{
					return false;
				}
				break;

			case SpecialKind::Sound:
				if (!NameIs(b, c.at, "SOUNDOB2") || !ReadSound(b, c, &one))
				{
					return false;
				}
				break;

			case SpecialKind::ParticleGenerator:
				if (!NameIs(b, c.at, "PARGENER") || !ReadParticle(b, c, &one))
				{
					return false;
				}
				break;
			}

			if (read)
			{
				*read = one;
			}

			return true;
		}

		// The place in the SPECLOBJ children of the n-th object of that kind, or special.size()
		size_t NthOfKind(Where const &w, SpecialKind kind, size_t number)
		{
			size_t seen = 0;

			for (size_t i = 0; i < w.special.size(); i++)
			{
				if (IsKindChunk(w.image, w.special[i], kind, nullptr) && seen++ == number)
				{
					return i;
				}
			}

			return w.special.size();
		}

		// ---- a generator's AVPGENEX (sibling in SPECLOBJ, found by its AVPGENNM name: avp_chunk.cpp get_extra_data_chunk)
		std::string ExtraName(Bytes const &b, Span const &extra)
		{
			std::vector<Span> parts;

			if (!NameIs(b, extra.at, "AVPGENEX") || !Children(b, extra.at + kHeadingBytes, extra.at + extra.size, &parts))
			{
				return std::string();
			}

			size_t const nameAt = FirstNamed(b, parts, "AVPGENNM");

			if (nameAt == parts.size() || parts[nameAt].size <= kHeadingBytes)
			{
				return std::string();
			}

			char const *const first = b.data() + parts[nameAt].at + kHeadingBytes;
			size_t const room = parts[nameAt].size - kHeadingBytes;
			char const *const zero = (char const *)memchr(first, 0, room);

			return std::string(first, zero ? (size_t)(zero - first) : room);
		}

		size_t ExtraAt(Where const &w, std::string const &name)
		{
			for (size_t i = 0; i < w.special.size(); i++)
			{
				if (NameIs(w.image, w.special[i].at, "AVPGENEX") && ExtraName(w.image, w.special[i]) == name)
				{
					return i;
				}
			}

			return w.special.size();
		}

		// GENEXSET: 20 bytes (rate, increase, limit + 3, two spares), then the weight block (its own size first, then 15 weights). A shorter block leaves the rest 0 (the game does the same).
		bool ReadSettings(Bytes const &b, Span const &extra, SpecialObject *o)
		{
			std::vector<Span> parts;

			if (!Children(b, extra.at + kHeadingBytes, extra.at + extra.size, &parts))
			{
				return false;
			}

			size_t const at = FirstNamed(b, parts, "GENEXSET");

			if (at == parts.size() || parts[at].size < kHeadingBytes + 24)
			{
				return false;
			}

			size_t const start = parts[at].at + kHeadingBytes;
			size_t const length = parts[at].size - kHeadingBytes;

			o->genRate = IntAt(b, start);
			o->genIncrease = IntAt(b, start + 4);
			o->genLimit = (uint8_t)b[start + 8];

			for (int i = 0; i < 15; i++)
			{
				size_t const where = 24 + (size_t)i * 4;

				o->weights[i] = where + 4 <= length ? IntAt(b, start + where) : 0;
			}

			o->hasSettings = true;
			return true;
		}

		Bytes MakeSettings(SpecialObject const &o)
		{
			Bytes p;

			AddInt(&p, o.genRate);
			AddInt(&p, o.genIncrease);
			p.push_back((char)o.genLimit);
			p.push_back(0);
			p.push_back(0);
			p.push_back(0);
			AddInt(&p, 0);
			AddInt(&p, 0);
			AddInt(&p, 0x40);

			for (int i = 0; i < 15; i++)
			{
				AddInt(&p, o.weights[i]);
			}

			return MakeChunk("GENEXSET", p);
		}

		// The AVPGENEX of `o` named o.name: its AVPGENNM and GENEXSET made from `o`, every other child of `old` (an AVPSTRAT, an ALTLOCAT) kept
		Bytes MakeExtra(SpecialObject const &o, Bytes const *image, Span const *old)
		{
			Bytes nameChunk;
			Bytes payload;
			bool keptSettings = false;

			AddText(&nameChunk, o.name, true);
			payload = MakeChunk("AVPGENNM", nameChunk);

			if (image && old)
			{
				std::vector<Span> parts;

				if (Children(*image, old->at + kHeadingBytes, old->at + old->size, &parts))
				{
					for (Span const &part : parts)
					{
						if (NameIs(*image, part.at, "AVPGENNM"))
						{
							continue;
						}

						if (NameIs(*image, part.at, "GENEXSET"))
						{
							if (o.hasSettings)
							{
								continue;
							}

							keptSettings = true;
						}

						AddCopy(&payload, *image, part);
					}
				}
			}

			if (o.hasSettings && !keptSettings)
			{
				Bytes settings = MakeSettings(o);

				payload.insert(payload.end(), settings.begin(), settings.end());
			}

			return MakeChunk("AVPGENEX", payload);
		}

		std::vector<Bytes> Copies(Where const &w)
		{
			std::vector<Bytes> out;

			for (Span const &one : w.special)
			{
				Bytes copy;

				AddCopy(&copy, w.image, one);
				out.push_back(copy);
			}

			return out;
		}
	}

	char const *SpecialWords(SpecialResult result)
	{
		switch (result)
		{
		case SpecialResult::Done: return "done";
		case SpecialResult::BadCall: return "no level to work on";
		case SpecialResult::NoEnvironment: return "the level has no environment data";
		case SpecialResult::NotFound: return "no object of that kind has that number";
		case SpecialResult::BadName: return "a name is 0 to 63 plain letters";
		case SpecialResult::BadValue: return "a number that field cannot hold";
		case SpecialResult::CantBuild: return "the level could not be put together from its bytes";
		case SpecialResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	bool SpecialName_Good(std::string const &name)
	{
		return Plain(name);
	}

	void ParticleGeneratorId(std::string const &name, uint8_t id[8])
	{
		std::string const text = "PARTICLE GEN" + name;
		char digest[16];

		md5_buffer(text.c_str(), (unsigned int)text.size(), digest);
		memcpy(id, digest, 8);
		id[7] = 0;
	}

	void SoundObjectId(std::string const &name, uint8_t id[8])
	{
		char digest[16];

		md5_buffer(name.c_str(), (unsigned int)name.size(), digest);
		memcpy(id, digest, 8);
		id[7] = 0;
	}

	SpecialResult SpecialObjects_Read(OpenLevelFile *file, std::vector<SpecialObject> *out)
	{
		Where w;

		out->clear();

		SpecialResult const found = Find(file, &w);

		if (found != SpecialResult::Done && found != SpecialResult::NoEnvironment)
		{
			return found;
		}

		if (found == SpecialResult::NoEnvironment)
		{
			return SpecialResult::Done;
		}

		size_t counts[5] = { 0, 0, 0, 0, 0 };

		for (Span const &one : w.special)
		{
			SpecialObject made;
			bool ok = false;

			if (NameIs(w.image, one.at, "AVPGENER"))
			{
				ok = ReadGenerator(w.image, one, &made);
			}
			else if (NameIs(w.image, one.at, "AVPSTART"))
			{
				ok = ReadStart(w.image, one, &made);
			}
			else if (NameIs(w.image, one.at, "SOUNDOB2"))
			{
				ok = ReadSound(w.image, one, &made);
			}
			else if (NameIs(w.image, one.at, "PARGENER"))
			{
				ok = ReadParticle(w.image, one, &made);
			}

			if (ok && (made.kind == SpecialKind::Generator || made.kind == SpecialKind::BadGuy) && (made.flags & 0x8000000u))
			{
				size_t const extra = ExtraAt(w, made.name);

				if (extra != w.special.size())
				{
					ReadSettings(w.image, w.special[extra], &made);
				}
			}

			if (ok)
			{
				made.number = counts[(int)made.kind]++;
				out->push_back(made);
			}
		}

		return SpecialResult::Done;
	}

	// Every placed hierarchy of SPECLOBJ, in the file's order; `where` gets each one's place among SPECLOBJ's children
	static void PlacedHierarchiesOf(Where const &w, std::vector<PlacedHierarchyInfo> *out, std::vector<size_t> *where)
	{
		for (size_t s = 0; s < w.special.size(); s++)
		{
			Span const &one = w.special[s];
			std::vector<Span> parts;

			if (!NameIs(w.image, one.at, "PLACHIER") || !Children(w.image, one.at + kHeadingBytes, one.at + one.size, &parts))
			{
				continue;
			}

			size_t const dataAt = FirstNamed(w.image, parts, "PLACHIDT");

			if (dataAt == parts.size() || parts[dataAt].size < kHeadingBytes)
			{
				continue;
			}

			size_t const at = parts[dataAt].at + kHeadingBytes;
			size_t const length = parts[dataAt].size - kHeadingBytes;
			char const *const first = w.image.data() + at;
			char const *const zero = length ? (char const *)memchr(first, 0, length) : nullptr;

			if (!zero)
			{
				continue;
			}

			size_t const nameLength = (size_t)(zero - first);
			size_t const nameBytes = (nameLength + 4) & ~(size_t)3;
			size_t const numberBytes = 4 + 12 + 16 + 8 + 4;   // hierarchy_index, the place, the turn, the ID, the extra count

			if (length < nameBytes || length - nameBytes < numberBytes)
			{
				continue;
			}

			PlacedHierarchyInfo made;
			size_t const p = at + nameBytes;

			made.number = out->size();
			made.name.assign(first, nameLength);
			made.hierarchyIndex = IntAt(w.image, p);

			for (int i = 0; i < 3; i++)
			{
				made.place[i] = IntAt(w.image, p + 4 + (size_t)i * 4);
			}

			memcpy(made.turn, w.image.data() + p + 16, 16);
			memcpy(made.id, w.image.data() + p + 32, 8);

			// Its sequences (PLHISEQU, inteng FUN_004eb690): five numbers (index, sequence, sub, time, flags), a count and that many timed sounds of 3 numbers, then a count and that many
			// extra numbers. Both counts come from the file, so each is measured against the chunk first, and the chunk has to end where they end, as the game's reader checks (avpx hierplace.cpp).
			for (Span const &part : parts)
			{
				if (!NameIs(w.image, part.at, "PLHISEQU") || part.size < kHeadingBytes)
				{
					continue;
				}

				size_t const from = part.at + kHeadingBytes;
				size_t const bytes = part.size - kHeadingBytes;
				size_t const numbersBytes = 5 * 4;

				if (bytes < numbersBytes + 2 * 4)
				{
					continue;
				}

				int32_t const soundCount = IntAt(w.image, from + numbersBytes);
				size_t const room = bytes - numbersBytes - 2 * 4;

				if (soundCount < 0 || (size_t)soundCount > room / 12)
				{
					continue;
				}

				size_t const soundBytes = (size_t)soundCount * 12;
				int32_t const extraCount = IntAt(w.image, from + numbersBytes + 4 + soundBytes);

				if (extraCount < 0 || (size_t)extraCount * 4 != room - soundBytes)
				{
					continue;
				}

				PlacedHierarchySequence sequence;

				sequence.index = IntAt(w.image, from);
				sequence.sequence = IntAt(w.image, from + 4);
				sequence.sub = IntAt(w.image, from + 8);
				sequence.milliseconds = IntAt(w.image, from + 12);
				sequence.flags = IntAt(w.image, from + 16);

				for (int32_t s = 0; s < soundCount; s++)
				{
					size_t const at3 = from + numbersBytes + 4 + (size_t)s * 12;
					PlacedHierarchyTimedSound timed;

					timed.soundIndex = IntAt(w.image, at3);
					timed.start = IntAt(w.image, at3 + 4);
					timed.end = IntAt(w.image, at3 + 8);
					sequence.sounds.push_back(timed);
				}

				made.sequences.push_back(sequence);
			}

			out->push_back(made);

			if (where)
			{
				where->push_back(s);
			}
		}
	}

	SpecialResult PlacedHierarchies_Read(OpenLevelFile *file, std::vector<PlacedHierarchyInfo> *out)
	{
		Where w;

		out->clear();

		SpecialResult const found = Find(file, &w);

		if (found == SpecialResult::NoEnvironment)
		{
			return SpecialResult::Done;
		}

		if (found != SpecialResult::Done)
		{
			return found;
		}

		PlacedHierarchiesOf(w, out, nullptr);
		return SpecialResult::Done;
	}

	SpecialResult PlacedHierarchy_WriteSequences(OpenLevelFile *file, size_t number, std::vector<PlacedHierarchySequence> const &sequences)
	{
		// What inteng's save writes for them (FUN_004e96d0 0x4e98b1-0x4e9a80: every PLHISEQU taken out, then one per record by FUN_004eb9b0 = FUN_00485010(parent, index), the game's
		// Placed_Hierarchy_Sequence_Chunk(parent, index): no extra data), held to what the game's reader takes (avpx hierplace.cpp: index 0..4095)
		if (sequences.size() > 4096)
		{
			return SpecialResult::BadValue;
		}

		for (PlacedHierarchySequence const &one : sequences)
		{
			if (one.index < 0 || one.index > 4095 || one.milliseconds <= 0 || one.sounds.size() > 4096)
			{
				return SpecialResult::BadValue;
			}
		}

		Where w;
		SpecialResult const found = Find(file, &w);

		if (found != SpecialResult::Done)
		{
			return found;
		}

		std::vector<PlacedHierarchyInfo> all;
		std::vector<size_t> where;

		PlacedHierarchiesOf(w, &all, &where);

		if (number >= where.size())
		{
			return SpecialResult::NotFound;
		}

		Span const &one = w.special[where[number]];
		std::vector<Span> parts;

		if (!Children(w.image, one.at + kHeadingBytes, one.at + one.size, &parts))
		{
			return SpecialResult::CantBuild;
		}

		Bytes made(w.image.begin() + (std::ptrdiff_t)one.at, w.image.begin() + (std::ptrdiff_t)(one.at + kHeadingBytes));
		Bytes chunks;

		for (PlacedHierarchySequence const &sequence : sequences)
		{
			Bytes chunk(kHeadingBytes, 0);
			int32_t const numbers[] = { sequence.index, sequence.sequence, sequence.sub, sequence.milliseconds, sequence.flags & 7, (int32_t)sequence.sounds.size() };

			memcpy(chunk.data(), "PLHISEQU", 8);
			AddBytes(&chunk, numbers, sizeof(numbers));

			for (PlacedHierarchyTimedSound const &timed : sequence.sounds)
			{
				int32_t const three[] = { timed.soundIndex, timed.start, timed.end };

				AddBytes(&chunk, three, sizeof(three));
			}

			int32_t const noExtra = 0;

			AddBytes(&chunk, &noExtra, sizeof(noExtra));
			PutIntAt(&chunk, 8, (int32_t)chunk.size());
			chunks.insert(chunks.end(), chunk.begin(), chunk.end());
		}

		// the other children kept in their order; the sequences go where the first one was (at the end when there was none)
		bool placed = false;

		for (Span const &part : parts)
		{
			if (NameIs(w.image, part.at, "PLHISEQU"))
			{
				if (!placed)
				{
					made.insert(made.end(), chunks.begin(), chunks.end());
					placed = true;
				}

				continue;
			}

			made.insert(made.end(), w.image.begin() + (std::ptrdiff_t)part.at, w.image.begin() + (std::ptrdiff_t)(part.at + part.size));
		}

		if (!placed)
		{
			made.insert(made.end(), chunks.begin(), chunks.end());
		}

		PutIntAt(&made, 8, (int32_t)made.size());

		std::vector<Bytes> copies = Copies(w);

		copies[where[number]] = made;
		return Rebuilt(file, w, copies);
	}

	SpecialObject SpecialObject_Fresh(SpecialKind kind, SpecialPlace const &where, double levelScale)
	{
		SpecialObject o;
		double const unit = 100.0 * (levelScale > 0.0 ? levelScale : 1.0);

		o.kind = kind;
		memcpy(o.place, where.place, sizeof(o.place));
		memcpy(o.matrix, where.matrix, sizeof(o.matrix));
		memcpy(o.module, where.module, sizeof(o.module));
		memcpy(o.turn, where.turn, sizeof(o.turn));
		o.heading = where.heading;

		switch (kind)
		{
		case SpecialKind::Generator:
			o.type = 0;
			o.flags = 0x3801;
			break;

		case SpecialKind::BadGuy:
			o.type = 1;        // READ: DAT_005e7a48 = 1 (FUN_004dd600 0x4dd669)
			o.flags = 0x3801;
			break;

		case SpecialKind::Sound:
			o.inner = (int32_t)(500000.0 / unit);    // READ: DAT_005e7a60 = 500000, DAT_005e7a64 = 1000000, DAT_005e7a68 = 127; the save divides by DAT_00607bd8 (100 x scale)
			o.outer = (int32_t)(1000000.0 / unit);
			o.volume = 127;
			break;

		case SpecialKind::ParticleGenerator:
			o.time = 50;       // READ: FUN_004e65c0 0x4e665c (0x32)
			o.particleProbability = 50;
			break;

		case SpecialKind::PlayerStart:
			break;
		}

		return o;
	}

	SpecialResult SpecialObject_Create(OpenLevelFile *file, SpecialKind kind, SpecialPlace const &where, double levelScale, std::string const &name, size_t *number)
	{
		Where w;

		if (!Plain(name))
		{
			return SpecialResult::BadName;
		}

		SpecialResult const found = Find(file, &w);

		if (found != SpecialResult::Done)
		{
			return found;
		}

		SpecialObject made = SpecialObject_Fresh(kind, where, levelScale);

		made.name = name;

		if (kind == SpecialKind::ParticleGenerator)
		{
			ParticleGeneratorId(name, made.id);
		}

		size_t same = 0;

		for (Span const &one : w.special)
		{
			same += IsKindChunk(w.image, one, kind, nullptr) ? 1 : 0;
		}

		std::vector<Bytes> chunks = Copies(w);

		chunks.push_back(MakeChunkFor(made, nullptr, nullptr));

		SpecialResult const done = Rebuilt(file, w, chunks);

		if (done == SpecialResult::Done && number)
		{
			*number = same;
		}

		return done;
	}

	SpecialResult SpecialObject_Write(OpenLevelFile *file, SpecialObject const &object)
	{
		Where w;

		if (object.kind == SpecialKind::Sound && (object.volume < 0 || object.volume > 127 || object.inner < 0 || object.outer < 0 || object.wav.size() > 255))
		{
			return SpecialResult::BadValue;
		}

		SpecialResult const found = Find(file, &w);

		if (found != SpecialResult::Done)
		{
			return found;
		}

		size_t const at = NthOfKind(w, object.kind, object.number);

		if (at == w.special.size())
		{
			return SpecialResult::NotFound;
		}

		// a name of 0 to 63 plain letters; a level of the game's own may hold a longer one, which stays as it is
		SpecialObject current;

		IsKindChunk(w.image, w.special[at], object.kind, &current);

		if (!Plain(object.name) && object.name != current.name)
		{
			return SpecialResult::BadName;
		}

		SpecialObject written = object;

		if (written.kind == SpecialKind::ParticleGenerator)
		{
			ParticleGeneratorId(written.name, written.id);
		}

		std::vector<Bytes> chunks = Copies(w);
		bool const generator = written.kind == SpecialKind::Generator || written.kind == SpecialKind::BadGuy;
		std::string oldName;

		if (generator)
		{
			SpecialObject before;

			if (ReadGenerator(w.image, w.special[at], &before))
			{
				oldName = before.name;
			}
		}

		chunks[at] = MakeChunkFor(written, &w.image, &w.special[at]);

		if (generator)
		{
			// the AVPGENEX that goes with the generator follows its name (the game finds it by that name); made when the generator has settings and none yet
			size_t const extra = ExtraAt(w, oldName);

			if (extra != w.special.size())
			{
				chunks[extra] = MakeExtra(written, &w.image, &w.special[extra]);
			}
			else if (written.hasSettings)
			{
				chunks.push_back(MakeExtra(written, nullptr, nullptr));
			}
		}

		return Rebuilt(file, w, chunks);
	}

	SpecialResult SpecialObject_Delete(OpenLevelFile *file, SpecialKind kind, size_t number)
	{
		Where w;
		SpecialResult const found = Find(file, &w);

		if (found != SpecialResult::Done)
		{
			return found;
		}

		size_t const at = NthOfKind(w, kind, number);

		if (at == w.special.size())
		{
			return SpecialResult::NotFound;
		}

		std::vector<Bytes> chunks = Copies(w);
		size_t extra = w.special.size();
		SpecialObject gone;

		if ((kind == SpecialKind::Generator || kind == SpecialKind::BadGuy) && ReadGenerator(w.image, w.special[at], &gone))
		{
			extra = ExtraAt(w, gone.name);
		}

		// the generator's AVPGENEX goes with it (unless another generator of the same name still needs it)
		if (extra != w.special.size())
		{
			size_t sameName = 0;

			for (size_t i = 0; i < w.special.size(); i++)
			{
				SpecialObject other;

				if (i != at && NameIs(w.image, w.special[i].at, "AVPGENER") && ReadGenerator(w.image, w.special[i], &other) && other.name == gone.name)
				{
					sameName++;
				}
			}

			if (sameName == 0)
			{
				chunks.erase(chunks.begin() + (std::ptrdiff_t)extra);
				chunks.erase(chunks.begin() + (std::ptrdiff_t)(extra < at ? at - 1 : at));
				return Rebuilt(file, w, chunks);
			}
		}

		chunks.erase(chunks.begin() + (std::ptrdiff_t)at);
		return Rebuilt(file, w, chunks);
	}
}
