// See hierarchy_edit.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "hierarchy_edit.h"

#include <string.h>

#include <limits>
#include <math.h>

#include "frame_sound_index.h"
#include "hierarchy_name.h"
#include "level_file.h"   // OpenLevelFile: BuildLevel, ReplaceLevel

namespace host
{
	namespace
	{
		typedef std::vector<char> Image;

		size_t const kHeadingBytes = 12;          // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const kMostHierarchyNameBytes = 255;
		size_t const kMostLevelBytes = (size_t)std::numeric_limits<int32_t>::max();

		struct Span
		{
			size_t at;
			size_t size;
		};

		int32_t IntAt(Image const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Image *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		bool ChunkSizeAt(Image const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < kHeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)kHeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Image const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		bool Children(Image const &bytes, Span const &parent, std::vector<Span> *out)
		{
			out->clear();

			for (size_t at = parent.at + kHeadingBytes; at < parent.at + parent.size; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, parent.at + parent.size, &size))
				{
					return false;
				}

				Span const span = { at, size };

				out->push_back(span);
				at += size;
			}

			return true;
		}

		// The whole level as the root chunk (REBINFF2)
		bool Root(Image const &image, Span *root)
		{
			size_t size = 0;

			if (image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2") || !ChunkSizeAt(image, 0, image.size(), &size) || size != image.size())
			{
				return false;
			}

			root->at = 0;
			root->size = size;
			return true;
		}

		// The first child of `parent` with that name; false when there is none or the chunk's children cannot be walked (*walkable says which)
		bool FirstChild(Image const &image, Span const &parent, char const *name, Span *found, bool *walkable)
		{
			std::vector<Span> inside;

			*walkable = Children(image, parent, &inside);

			if (!*walkable)
			{
				return false;
			}

			for (Span const &child : inside)
			{
				if (NameIs(image, child.at, name))
				{
					*found = child;
					return true;
				}
			}

			return false;
		}

		// The name an OBHIERNM chunk holds: its bytes up to the first 0, which must be inside the chunk
		bool NameOfChunk(Image const &image, Span const &chunk, std::string *name)
		{
			char const *const first = image.data() + chunk.at + kHeadingBytes;
			size_t const length = chunk.size - kHeadingBytes;
			char const *const zero = (char const *)memchr(first, 0, length);

			if (!zero)
			{
				return false;
			}

			name->assign(first, (size_t)(zero - first));
			return true;
		}

		struct HierarchyAt
		{
			Span span;
			Span nameChunk;
			std::string name;
		};

		// Every top-level OBJCHIER that has a readable OBHIERNM, in file order
		bool Hierarchies(Image const &image, Span const &root, std::vector<HierarchyAt> *out)
		{
			std::vector<Span> top;

			out->clear();

			if (!Children(image, root, &top))
			{
				return false;
			}

			for (Span const &child : top)
			{
				HierarchyAt each;
				bool walkable = false;

				if (!NameIs(image, child.at, "OBJCHIER"))
				{
					continue;
				}

				if (!FirstChild(image, child, "OBHIERNM", &each.nameChunk, &walkable))
				{
					if (!walkable)
					{
						return false;
					}

					continue;   // no name: inteng finds no object for it either
				}

				if (!NameOfChunk(image, each.nameChunk, &each.name))
				{
					return false;
				}

				each.span = child;
				out->push_back(each);
			}

			return true;
		}

		// Every top-level HSETCOLL: its span and the collection it holds
		struct CollectionAt
		{
			Span span;
			model::ShapeSetCollection collection;
		};

		bool Collections(Image const &image, Span const &root, std::vector<CollectionAt> *out)
		{
			std::vector<Span> top;

			out->clear();

			if (!Children(image, root, &top))
			{
				return false;
			}

			for (Span const &child : top)
			{
				if (!NameIs(image, child.at, "HSETCOLL"))
				{
					continue;
				}

				CollectionAt each;

				each.span = child;

				if (model::ReadCollection((uint8_t const *)image.data() + child.at + kHeadingBytes, child.size - kHeadingBytes, &each.collection) != model::CollectionResult::Ok)
				{
					return false;
				}

				out->push_back(each);
			}

			return true;
		}

		// The image with [at, at + oldSize) replaced by `data`, and the stored size of every ancestor (the chunk positions, outermost first, all before `at`) made right
		bool Spliced(Image const &image, std::vector<size_t> const &ancestors, size_t at, size_t oldSize, Image const &data, Image *out)
		{
			if (at > image.size() || oldSize > image.size() - at)
			{
				return false;
			}

			size_t const total = image.size() - oldSize + data.size();

			if (total > kMostLevelBytes)
			{
				return false;
			}

			out->assign(image.begin(), image.begin() + (std::ptrdiff_t)at);
			out->insert(out->end(), data.begin(), data.end());
			out->insert(out->end(), image.begin() + (std::ptrdiff_t)(at + oldSize), image.end());

			for (size_t const ancestor : ancestors)
			{
				int64_t const size = (int64_t)IntAt(image, ancestor + 8) - (int64_t)oldSize + (int64_t)data.size();

				if (size < (int64_t)kHeadingBytes || size > (int64_t)kMostLevelBytes)
				{
					return false;
				}

				PutIntAt(out, ancestor + 8, (int32_t)size);
			}

			return true;
		}

		// One chunk: its heading (the 8-letter name, the size) and the payload
		Image MakeChunk(char const *name, uint8_t const *payload, size_t size)
		{
			Image chunk(kHeadingBytes + size);

			memcpy(chunk.data(), name, 8);
			PutIntAt(&chunk, 8, (int32_t)(kHeadingBytes + size));

			if (size)
			{
				memcpy(chunk.data() + kHeadingBytes, payload, size);
			}

			return chunk;
		}

		// An OBHIERNM chunk as the game writes it: 12 + length + 4 - length % 4 bytes, the name then zeros
		Image MakeNameChunk(std::string const &name)
		{
			size_t const payload = name.size() + 4 - name.size() % 4;
			Image chunk(kHeadingBytes + payload, 0);

			memcpy(chunk.data(), "OBHIERNM", 8);
			PutIntAt(&chunk, 8, (int32_t)(kHeadingBytes + payload));
			memcpy(chunk.data() + kHeadingBytes, name.data(), name.size());
			return chunk;
		}

		bool PlainText(std::string const &name)
		{
			if (name.empty() || name.size() > kMostHierarchyNameBytes)
			{
				return false;
			}

			for (char const c : name)
			{
				if ((unsigned char)c < 32 || (unsigned char)c > 126)
				{
					return false;
				}
			}

			return true;
		}

		HierarchyEditResult Replace(OpenLevelFile *file, Image const &out)
		{
			LevelOpenResult const result = file->ReplaceLevel(out);

			if (result == LevelOpenResult::Opened)
			{
				return HierarchyEditResult::Done;
			}

			return result == LevelOpenResult::CantRead ? HierarchyEditResult::CantBuild : HierarchyEditResult::LoaderRefused;
		}

		// The level's bytes and root, or the answer to give
		HierarchyEditResult Open(OpenLevelFile *file, Image *image, Span *root)
		{
			if (!file || !file->Level())
			{
				return HierarchyEditResult::BadCall;
			}

			if (!file->BuildLevel(*image) || !Root(*image, root))
			{
				return HierarchyEditResult::CantBuild;
			}

			return HierarchyEditResult::Done;
		}

		void Why(char const **why, char const *words)
		{
			if (why)
			{
				*why = words;
			}
		}

		char const *CollectionWhy(model::CollectionResult result)
		{
			return model::CollectionWords(result);
		}

		// The collections of the level, or the answer to give
		HierarchyEditResult ReadCollections(OpenLevelFile *file, Image *image, Span *root, std::vector<CollectionAt> *list)
		{
			HierarchyEditResult const opened = Open(file, image, root);

			if (opened != HierarchyEditResult::Done)
			{
				return opened;
			}

			return Collections(*image, *root, list) ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
		}

		std::vector<model::ShapeSetCollection> Plain(std::vector<CollectionAt> const &list)
		{
			std::vector<model::ShapeSetCollection> plain;

			for (CollectionAt const &each : list)
			{
				plain.push_back(each.collection);
			}

			return plain;
		}

		// Every top-level OBHALTSH: its number and name (4 bytes, then the name, which must end inside the chunk)
		bool AlternateSets(Image const &image, Span const &root, std::vector<model::AlternateSetInfo> *out)
		{
			std::vector<Span> top;

			out->clear();

			if (!Children(image, root, &top))
			{
				return false;
			}

			for (Span const &child : top)
			{
				if (!NameIs(image, child.at, "OBHALTSH"))
				{
					continue;
				}

				if (child.size < kHeadingBytes + 8)
				{
					return false;
				}

				model::AlternateSetInfo each;
				size_t const room = child.size - kHeadingBytes - 4;
				char const *const first = image.data() + child.at + kHeadingBytes + 4;
				char const *const zero = (char const *)memchr(first, 0, room);

				if (!zero)
				{
					return false;
				}

				each.number = IntAt(image, child.at + kHeadingBytes);
				each.name.assign(first, (size_t)(zero - first));
				out->push_back(each);
			}

			return true;
		}

		// Where an OBHALTSH chunk's flag word is: after the number (4), the name, the count and that many pairs of names, each name zero-ended and padded to (length + 4) & ~3
		// (hierarchy_chunk.cpp:398-484). false when a name has no end inside the chunk or the word does not fit.
		bool ShapeSetFlagsAt(Image const &image, Span const &chunk, size_t *at)
		{
			size_t const end = chunk.at + chunk.size;
			size_t p = chunk.at + kHeadingBytes + 4;

			auto skipName = [&]() -> bool
			{
				if (p >= end)
				{
					return false;
				}

				char const *const first = image.data() + p;
				char const *const zero = (char const *)memchr(first, 0, end - p);

				if (!zero)
				{
					return false;
				}

				size_t const length = (size_t)(zero - first);
				size_t const room = (length + 4) & ~(size_t)3;

				if (room > end - p)
				{
					return false;
				}

				p += room;
				return true;
			};

			if (!skipName() || end - p < 4)
			{
				return false;
			}

			int32_t const count = IntAt(image, p);

			p += 4;

			if (count < 0 || (size_t)count > (end - p) / 8)
			{
				return false;
			}

			for (int32_t i = 0; i < count; i++)
			{
				if (!skipName() || !skipName())
				{
					return false;
				}
			}

			if (end - p < 16)
			{
				return false;
			}

			*at = p;
			return true;
		}

		// The collection chunk of that number, or list.size()
		size_t IndexOfNumber(std::vector<CollectionAt> const &list, int32_t number)
		{
			for (size_t i = 0; i < list.size(); i++)
			{
				if (list[i].collection.number == number)
				{
					return i;
				}
			}

			return list.size();
		}

		// The chunk of a collection as the game writes it
		bool CollectionChunk(model::ShapeSetCollection const &collection, Image *chunk)
		{
			std::vector<uint8_t> payload;

			if (model::CollectionBytes(collection, &payload) != model::CollectionResult::Ok)
			{
				return false;
			}

			*chunk = MakeChunk("HSETCOLL", payload.data(), payload.size());
			return true;
		}

		// A collection's chunk made again from `changed` (its number is looked up in the file as it was)
		HierarchyEditResult WriteChanged(OpenLevelFile *file, Image const &image, Span const &root, std::vector<CollectionAt> const &list, size_t index,
			model::ShapeSetCollection const &changed)
		{
			Image chunk;
			Image out;
			std::vector<size_t> const ancestors = { root.at };

			if (!CollectionChunk(changed, &chunk) || !Spliced(image, ancestors, list[index].span.at, list[index].span.size, chunk, &out))
			{
				return HierarchyEditResult::CantBuild;
			}

			return Replace(file, out);
		}

		// The OBANSEQC of that number in the named hierarchy's OBANSEQS: ancestors (root, hierarchy, OBANSEQS) and the sequence's span
		HierarchyEditResult FindSequence(Image const &image, Span const &root, std::string const &hierarchy, size_t sequence, std::vector<size_t> *ancestors, Span *found)
		{
			std::vector<HierarchyAt> all;

			if (!Hierarchies(image, root, &all))
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			for (HierarchyAt const &each : all)
			{
				if (each.name != hierarchy)
				{
					continue;
				}

				// the sequences with their times and speeds are in the hierarchy's OWN OBANSEQS, beside its name (the game's build_time_list reads them there, and
				// all 23 stock models keep them there); the sections inside hold the frames
				bool walkable = false;
				Span list;

				if (!FirstChild(image, each.span, "OBANSEQS", &list, &walkable))
				{
					return walkable ? HierarchyEditResult::NoSequence : HierarchyEditResult::BadLevelBytes;
				}

				std::vector<Span> sequences;

				if (!Children(image, list, &sequences))
				{
					return HierarchyEditResult::BadLevelBytes;
				}

				size_t seen = 0;

				for (Span const &sequenceSpan : sequences)
				{
					if (!NameIs(image, sequenceSpan.at, "OBANSEQC"))
					{
						continue;
					}

					if (seen++ == sequence)
					{
						*found = sequenceSpan;
						*ancestors = { root.at, each.span.at, list.at };
						return HierarchyEditResult::Done;
					}
				}

				return HierarchyEditResult::NoSequence;
			}

			return HierarchyEditResult::NoHierarchy;
		}
	}

	char const *HierarchyEditWords(HierarchyEditResult result)
	{
		switch (result)
		{
		case HierarchyEditResult::Done: return "done";
		case HierarchyEditResult::BadCall: return "nothing to change";
		case HierarchyEditResult::NoHierarchy: return "no hierarchy has that name";
		case HierarchyEditResult::NoSequence: return "the hierarchy has no such sequence";
		case HierarchyEditResult::NoCollection: return "no shape set collection has that number";
		case HierarchyEditResult::Refused: return "the command was refused";
		case HierarchyEditResult::BadLevelBytes: return "a chunk of the level could not be read";
		case HierarchyEditResult::CantBuild: return "the level couldn't be put together from its bytes";
		case HierarchyEditResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	HierarchyEditResult HierarchyEdit_Names(OpenLevelFile *file, std::vector<std::string> *names)
	{
		Image image;
		Span root;
		std::vector<HierarchyAt> all;

		if (!names)
		{
			return HierarchyEditResult::BadCall;
		}

		names->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		if (!Hierarchies(image, root, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (HierarchyAt const &each : all)
		{
			names->push_back(each.name);
		}

		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_Copy(OpenLevelFile *file, std::string const &from, std::string const &newName, char const **why)
	{
		Image image;
		Span root;

		Why(why, nullptr);

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<HierarchyAt> all;

		if (!Hierarchies(image, root, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		std::vector<std::string> names;
		HierarchyAt const *source = nullptr;

		for (HierarchyAt const &each : all)
		{
			names.push_back(each.name);

			if (!source && each.name == from)
			{
				source = &each;
			}
		}

		if (!source)
		{
			return HierarchyEditResult::NoHierarchy;
		}

		model::HierarchyNameResult const rule = model::CheckCopyName(names, newName);

		if (rule != model::HierarchyNameResult::Ok)
		{
			Why(why, model::HierarchyNameWords(rule));
			return HierarchyEditResult::Refused;
		}

		if (!PlainText(newName))
		{
			Why(why, "the name must be 1 to 255 plain letters");
			return HierarchyEditResult::Refused;
		}

		// The copy: the hierarchy's bytes with its name chunk made again, put at the end of the level
		Image copy(image.begin() + (std::ptrdiff_t)source->span.at, image.begin() + (std::ptrdiff_t)(source->span.at + source->span.size));
		size_t const nameAt = source->nameChunk.at - source->span.at;
		Image renamed;
		std::vector<size_t> const own = { 0 };

		if (!Spliced(copy, own, nameAt, source->nameChunk.size, MakeNameChunk(newName), &renamed))
		{
			return HierarchyEditResult::CantBuild;
		}

		Image out;
		std::vector<size_t> const ancestors = { root.at };

		if (!Spliced(image, ancestors, image.size(), 0, renamed, &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		return Replace(file, out);
	}

	HierarchyEditResult HierarchyEdit_Sequences(OpenLevelFile *file, std::string const &hierarchy, std::vector<SequenceInfo> *sequences)
	{
		Image image;
		Span root;

		if (!sequences)
		{
			return HierarchyEditResult::BadCall;
		}

		sequences->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		for (size_t index = 0; index < 100000; index++)
		{
			std::vector<size_t> ancestors;
			Span span;
			HierarchyEditResult const found = FindSequence(image, root, hierarchy, index, &ancestors, &span);

			if (found == HierarchyEditResult::NoSequence)
			{
				return HierarchyEditResult::Done;
			}

			if (found != HierarchyEditResult::Done)
			{
				return found;
			}

			SequenceInfo info = { 0, 0, 0, false };
			bool walkable = false;
			Span part;

			if (FirstChild(image, span, "OBASEQHD", &part, &walkable) && part.size >= kHeadingBytes + 12)
			{
				info.frames = IntAt(image, part.at + kHeadingBytes);
				info.number = IntAt(image, part.at + kHeadingBytes + 4);
				info.subNumber = IntAt(image, part.at + kHeadingBytes + 8);

				// the name: from payload byte 16 to the first 0 (or the chunk's end)
				size_t const nameAt = part.at + kHeadingBytes + 16;
				size_t const end = part.at + part.size;

				for (size_t at = nameAt; part.size >= kHeadingBytes + 17 && at < end && image[at] != 0; at++)
				{
					info.name.push_back(image[at]);
				}
			}
			else if (!walkable)
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			info.hasPlatform = FirstChild(image, span, "OBASEQSP", &part, &walkable);
			sequences->push_back(info);
		}

		return HierarchyEditResult::BadLevelBytes;   // more sequences than any file has
	}

	HierarchyEditResult HierarchyEdit_Collections(OpenLevelFile *file, std::vector<model::ShapeSetCollection> *list)
	{
		Image image;
		Span root;
		std::vector<CollectionAt> all;

		if (!list)
		{
			return HierarchyEditResult::BadCall;
		}

		list->clear();

		HierarchyEditResult const read = ReadCollections(file, &image, &root, &all);

		if (read == HierarchyEditResult::Done)
		{
			*list = Plain(all);
		}

		return read;
	}

	HierarchyEditResult HierarchyEdit_CreateCollection(OpenLevelFile *file, std::string const &name, int32_t *number, char const **why)
	{
		Image image;
		Span root;
		std::vector<CollectionAt> all;

		Why(why, nullptr);

		HierarchyEditResult const read = ReadCollections(file, &image, &root, &all);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		model::ShapeSetCollection created;
		model::CollectionResult const rule = model::CreateCollection(Plain(all), name, &created);

		if (rule != model::CollectionResult::Ok)
		{
			Why(why, CollectionWhy(rule));
			return HierarchyEditResult::Refused;
		}

		Image chunk;
		Image out;
		std::vector<size_t> const ancestors = { root.at };

		if (!CollectionChunk(created, &chunk) || !Spliced(image, ancestors, image.size(), 0, chunk, &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		HierarchyEditResult const done = Replace(file, out);

		if (done == HierarchyEditResult::Done && number)
		{
			*number = created.number;
		}

		return done;
	}

	HierarchyEditResult HierarchyEdit_RenameCollection(OpenLevelFile *file, int32_t number, std::string const &newName, char const **why)
	{
		Image image;
		Span root;
		std::vector<CollectionAt> all;

		Why(why, nullptr);

		HierarchyEditResult const read = ReadCollections(file, &image, &root, &all);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		size_t const index = IndexOfNumber(all, number);

		if (index == all.size())
		{
			return HierarchyEditResult::NoCollection;
		}

		std::vector<model::ShapeSetCollection> plain = Plain(all);
		model::CollectionResult const rule = model::RenameCollection(&plain, number, newName);

		if (rule != model::CollectionResult::Ok)
		{
			Why(why, CollectionWhy(rule));
			return HierarchyEditResult::Refused;
		}

		return WriteChanged(file, image, root, all, index, plain[index]);
	}

	HierarchyEditResult HierarchyEdit_DeleteCollection(OpenLevelFile *file, int32_t number, char const **why)
	{
		Image image;
		Span root;
		std::vector<CollectionAt> all;

		Why(why, nullptr);

		HierarchyEditResult const read = ReadCollections(file, &image, &root, &all);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		size_t const index = IndexOfNumber(all, number);

		if (index == all.size())
		{
			return HierarchyEditResult::NoCollection;
		}

		std::vector<model::ShapeSetCollection> plain = Plain(all);
		model::CollectionResult const rule = model::DeleteCollection(&plain, number);

		if (rule != model::CollectionResult::Ok)
		{
			Why(why, CollectionWhy(rule));
			return HierarchyEditResult::Refused;
		}

		Image out;
		std::vector<size_t> const ancestors = { root.at };

		if (!Spliced(image, ancestors, all[index].span.at, all[index].span.size, Image(), &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		return Replace(file, out);
	}

	HierarchyEditResult HierarchyEdit_SetCollectionVoiceIndex(OpenLevelFile *file, int32_t number, int32_t voiceIndex, char const **why)
	{
		Image image;
		Span root;
		std::vector<CollectionAt> all;

		Why(why, nullptr);

		HierarchyEditResult const read = ReadCollections(file, &image, &root, &all);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		size_t const index = IndexOfNumber(all, number);

		if (index == all.size())
		{
			return HierarchyEditResult::NoCollection;
		}

		std::vector<model::ShapeSetCollection> plain = Plain(all);
		model::CollectionResult const rule = model::SetCollectionVoiceIndex(&plain, number, voiceIndex);

		if (rule != model::CollectionResult::Ok)
		{
			Why(why, CollectionWhy(rule));
			return HierarchyEditResult::Refused;
		}

		return WriteChanged(file, image, root, all, index, plain[index]);
	}

	HierarchyEditResult HierarchyEdit_AlternateSets(OpenLevelFile *file, std::vector<model::AlternateSetInfo> *sets)
	{
		Image image;
		Span root;

		if (!sets)
		{
			return HierarchyEditResult::BadCall;
		}

		sets->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		return AlternateSets(image, root, sets) ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
	}

	HierarchyEditResult HierarchyEdit_ReadShapeSetFlags(OpenLevelFile *file, int32_t setNumber, uint32_t *flags)
	{
		return HierarchyEdit_ChangeShapeSetFlags(file, setNumber, 0, 0, flags);
	}

	HierarchyEditResult HierarchyEdit_ChangeShapeSetFlags(OpenLevelFile *file, int32_t setNumber, uint32_t setMask, uint32_t clearMask, uint32_t *flags)
	{
		Image image;
		Span root;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<Span> top;

		if (!Children(image, root, &top))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (Span const &child : top)
		{
			if (!NameIs(image, child.at, "OBHALTSH"))
			{
				continue;
			}

			if (child.size < kHeadingBytes + 4 || IntAt(image, child.at + kHeadingBytes) != setNumber)
			{
				continue;
			}

			size_t at = 0;

			if (!ShapeSetFlagsAt(image, child, &at))
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			uint32_t const old = (uint32_t)IntAt(image, at);
			uint32_t const now = (old | setMask) & ~clearMask;

			if (flags)
			{
				*flags = now;
			}

			if (setMask == 0 && clearMask == 0)
			{
				return HierarchyEditResult::Done;
			}

			// the same bytes with the one word made again (the chunk keeps its size)
			Image out(image);
			std::vector<size_t> const none;

			PutIntAt(&out, at, (int32_t)now);
			(void)none;
			return Replace(file, out);
		}

		return HierarchyEditResult::NoCollection;   // no shape set has that number
	}

	HierarchyEditResult HierarchyEdit_AddShapeSet(OpenLevelFile *file, int32_t collectionNumber, int32_t setNumber, char const **why)
	{
		Image image;
		Span root;
		std::vector<CollectionAt> all;
		std::vector<model::AlternateSetInfo> alternates;

		Why(why, nullptr);

		HierarchyEditResult const read = ReadCollections(file, &image, &root, &all);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		if (!AlternateSets(image, root, &alternates))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		size_t const index = IndexOfNumber(all, collectionNumber);

		if (index == all.size())
		{
			return HierarchyEditResult::NoCollection;
		}

		std::vector<model::ShapeSetCollection> plain = Plain(all);
		model::CollectionResult const rule = model::AddShapeSet(&plain, collectionNumber, setNumber, alternates);

		if (rule != model::CollectionResult::Ok)
		{
			Why(why, CollectionWhy(rule));
			return HierarchyEditResult::Refused;
		}

		return WriteChanged(file, image, root, all, index, plain[index]);
	}

	HierarchyEditResult HierarchyEdit_RemoveShapeSet(OpenLevelFile *file, int32_t collectionNumber, int32_t setNumber, char const **why)
	{
		Image image;
		Span root;
		std::vector<CollectionAt> all;

		Why(why, nullptr);

		HierarchyEditResult const read = ReadCollections(file, &image, &root, &all);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		size_t const index = IndexOfNumber(all, collectionNumber);

		if (index == all.size())
		{
			return HierarchyEditResult::NoCollection;
		}

		std::vector<model::ShapeSetCollection> plain = Plain(all);
		model::CollectionResult const rule = model::RemoveShapeSet(&plain, collectionNumber, setNumber);

		if (rule != model::CollectionResult::Ok)
		{
			Why(why, CollectionWhy(rule));
			return HierarchyEditResult::Refused;
		}

		return WriteChanged(file, image, root, all, index, plain[index]);
	}

	HierarchyEditResult HierarchyEdit_ReadSequenceFlags(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, uint32_t *flags, bool *present)
	{
		Image image;
		Span root;

		if (!flags || !present)
		{
			return HierarchyEditResult::BadCall;
		}

		*flags = 0;
		*present = false;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<size_t> ancestors;
		Span sequenceSpan;
		HierarchyEditResult const found = FindSequence(image, root, hierarchy, sequence, &ancestors, &sequenceSpan);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;

		if (!FirstChild(image, sequenceSpan, "OBASEQFL", &existing, &walkable))
		{
			return walkable ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
		}

		if (existing.size < kHeadingBytes + 4)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		*flags = (uint32_t)IntAt(image, existing.at + kHeadingBytes);
		*present = true;
		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_ChangeSequenceFlags(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, uint32_t setMask, uint32_t clearMask, uint32_t *flags)
	{
		Image image;
		Span root;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<size_t> ancestors;
		Span sequenceSpan;
		HierarchyEditResult const found = FindSequence(image, root, hierarchy, sequence, &ancestors, &sequenceSpan);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;
		bool const has = FirstChild(image, sequenceSpan, "OBASEQFL", &existing, &walkable);

		if (!has && !walkable)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		if (has && existing.size < kHeadingBytes + 4)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		uint32_t const old = has ? (uint32_t)IntAt(image, existing.at + kHeadingBytes) : 0u;
		uint32_t const now = (old | setMask) & ~clearMask;
		Image out;
		bool spliced;

		ancestors.push_back(sequenceSpan.at);

		if (has)
		{
			// the same chunk, its first four bytes made again (any bytes after them stay)
			Image chunk(image.begin() + (std::ptrdiff_t)existing.at, image.begin() + (std::ptrdiff_t)(existing.at + existing.size));

			PutIntAt(&chunk, kHeadingBytes, (int32_t)now);
			spliced = Spliced(image, ancestors, existing.at, existing.size, chunk, &out);
		}
		else
		{
			// a new chunk goes at the end of the sequence (the game's chunk is 16 bytes: the heading and the flag word)
			uint8_t bytes[4];

			memcpy(bytes, &now, sizeof(bytes));
			spliced = Spliced(image, ancestors, sequenceSpan.at + sequenceSpan.size, 0, MakeChunk("OBASEQFL", bytes, sizeof(bytes)), &out);
		}

		if (!spliced)
		{
			return HierarchyEditResult::CantBuild;
		}

		HierarchyEditResult const done = Replace(file, out);

		if (done == HierarchyEditResult::Done && flags)
		{
			*flags = now;
		}

		return done;
	}

	HierarchyEditResult HierarchyEdit_SetPlatform(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, model::PlatformState const &state,
		model::SequenceSpeed *written)
	{
		Image image;
		Span root;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<size_t> ancestors;
		Span sequenceSpan;
		HierarchyEditResult const found = FindSequence(image, root, hierarchy, sequence, &ancestors, &sequenceSpan);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;
		bool const has = FirstChild(image, sequenceSpan, "OBASEQSP", &existing, &walkable);

		if (!has && !walkable)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		model::SequenceSpeed chunk = { 0, 0, 0 };
		std::vector<uint8_t> payload(model::kSequenceSpeedBytes, 0);

		if (has)
		{
			payload.assign((uint8_t const *)image.data() + existing.at + kHeadingBytes, (uint8_t const *)image.data() + existing.at + existing.size);

			if (model::ReadSequenceSpeed(payload.data(), payload.size(), &chunk) != model::SequenceSpeedResult::Ok)
			{
				return HierarchyEditResult::BadLevelBytes;
			}
		}

		model::ApplyPlatformState(state, &chunk);

		if (model::PutSequenceSpeed(chunk, &payload) != model::SequenceSpeedResult::Ok)
		{
			return HierarchyEditResult::CantBuild;
		}

		Image out;
		Image const made = MakeChunk("OBASEQSP", payload.data(), payload.size());
		bool spliced;

		if (has)
		{
			ancestors.push_back(sequenceSpan.at);
			spliced = Spliced(image, ancestors, existing.at, existing.size, made, &out);
		}
		else
		{
			// a new chunk goes at the end of the sequence, as the game's constructor puts it
			ancestors.push_back(sequenceSpan.at);
			spliced = Spliced(image, ancestors, sequenceSpan.at + sequenceSpan.size, 0, made, &out);
		}

		if (!spliced)
		{
			return HierarchyEditResult::CantBuild;
		}

		HierarchyEditResult const done = Replace(file, out);

		if (done == HierarchyEditResult::Done && written)
		{
			*written = chunk;
		}

		return done;
	}

	HierarchyEditResult HierarchyEdit_ReadPlatform(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, model::SequenceSpeed *out, bool *present)
	{
		Image image;
		Span root;

		if (!out || !present)
		{
			return HierarchyEditResult::BadCall;
		}

		*out = model::SequenceSpeed{ 0, 0, 0 };
		*present = false;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<size_t> ancestors;
		Span sequenceSpan;
		HierarchyEditResult const found = FindSequence(image, root, hierarchy, sequence, &ancestors, &sequenceSpan);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;

		if (!FirstChild(image, sequenceSpan, "OBASEQSP", &existing, &walkable))
		{
			return walkable ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
		}

		if (model::ReadSequenceSpeed((uint8_t const *)image.data() + existing.at + kHeadingBytes, existing.size - kHeadingBytes, out) != model::SequenceSpeedResult::Ok)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		*present = true;
		return HierarchyEditResult::Done;
	}

	namespace
	{
		size_t const kCompactFrameBytes = 36;   // OBANALLS frame: orientation 16, transform 12, start time 4, flags 4
		size_t const kFrameChunkFlagsAt = 36;   // OBASEQFR payload: orientation 16, transform 12, start time 4, frame number 4, then the flags

		struct FrameAt
		{
			int32_t at;
			size_t flagsAt;   // the flag word's place in the image
			size_t baseAt;    // the frame's first byte: its orientation (4 floats x, y, z, w: inteng's chunk +0x28..+0x34, FUN_00480450 0x48056c-0x4805cb), then its transform (3 ints)
		};

		// The root section (the first OBJCHIER inside the hierarchy's own OBJCHIER) of the named hierarchy
		HierarchyEditResult RootSection(Image const &image, Span const &root, std::string const &hierarchy, Span *section)
		{
			std::vector<HierarchyAt> all;

			if (!Hierarchies(image, root, &all))
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			for (HierarchyAt const &each : all)
			{
				if (each.name != hierarchy)
				{
					continue;
				}

				bool walkable = false;

				if (!FirstChild(image, each.span, "OBJCHIER", section, &walkable))
				{
					return walkable ? HierarchyEditResult::NoSequence : HierarchyEditResult::BadLevelBytes;
				}

				return HierarchyEditResult::Done;
			}

			return HierarchyEditResult::NoHierarchy;
		}

		// The frames of the sequence with that number and sub-number in the root section's chunks, each with its start time and the place of its flag word
		HierarchyEditResult FramesOf(Image const &image, Span const &section, int32_t number, int32_t subNumber, std::vector<FrameAt> *frames)
		{
			std::vector<Span> inside;

			frames->clear();

			if (!Children(image, section, &inside))
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			bool seen = false;

			for (Span const &child : inside)
			{
				if (NameIs(image, child.at, "OBANALLS"))
				{
					size_t at = child.at + kHeadingBytes;
					size_t const end = child.at + child.size;

					if (end - at < 4)
					{
						return HierarchyEditResult::BadLevelBytes;
					}

					int32_t const count = IntAt(image, at);

					at += 4;

					for (int32_t i = 0; i < count; i++)
					{
						if (end - at < 16)
						{
							return HierarchyEditResult::BadLevelBytes;
						}

						int32_t const frameCount = IntAt(image, at);
						bool const mine = IntAt(image, at + 4) == number && IntAt(image, at + 8) == subNumber;

						at += 16;

						if (frameCount < 0 || (size_t)frameCount > (end - at) / kCompactFrameBytes)
						{
							return HierarchyEditResult::BadLevelBytes;
						}

						if (mine && !seen)
						{
							seen = true;

							for (int32_t f = 0; f < frameCount; f++)
							{
								size_t const frameAt = at + (size_t)f * kCompactFrameBytes;
								FrameAt made = { IntAt(image, frameAt + 28), frameAt + 32, frameAt };

								frames->push_back(made);
							}
						}

						at += (size_t)frameCount * kCompactFrameBytes;
					}
				}
				else if (NameIs(image, child.at, "OBANSEQS") && !seen)
				{
					std::vector<Span> sequences;

					if (!Children(image, child, &sequences))
					{
						return HierarchyEditResult::BadLevelBytes;
					}

					for (Span const &sequence : sequences)
					{
						Span header;
						bool walkable = false;

						if (!NameIs(image, sequence.at, "OBANSEQC") || !FirstChild(image, sequence, "OBASEQHD", &header, &walkable) || header.size < kHeadingBytes + 12)
						{
							continue;
						}

						if (IntAt(image, header.at + kHeadingBytes + 4) != number || IntAt(image, header.at + kHeadingBytes + 8) != subNumber)
						{
							continue;
						}

						std::vector<Span> parts;

						if (!Children(image, sequence, &parts))
						{
							return HierarchyEditResult::BadLevelBytes;
						}

						seen = true;

						for (Span const &part : parts)
						{
							if (NameIs(image, part.at, "OBASEQFR"))
							{
								if (part.size < kHeadingBytes + kFrameChunkFlagsAt + 4)
								{
									return HierarchyEditResult::BadLevelBytes;
								}

								FrameAt made = { IntAt(image, part.at + kHeadingBytes + 28), part.at + kHeadingBytes + kFrameChunkFlagsAt, part.at + kHeadingBytes };

								frames->push_back(made);
							}
						}

						break;
					}
				}
			}

			return seen ? HierarchyEditResult::Done : HierarchyEditResult::NoSequence;
		}

		// The frame whose range holds position: from its own start to the next frame's start (the last to 65536)
		bool FrameHolding(std::vector<FrameAt> const &frames, int32_t position, size_t *index)
		{
			for (size_t i = 0; i < frames.size(); i++)
			{
				int64_t const end = i + 1 < frames.size() ? (int64_t)frames[i + 1].at : 65536;

				if ((int64_t)frames[i].at <= position && position < end)
				{
					*index = i;
					return true;
				}
			}

			return false;
		}

		struct SoundAt
		{
			Span span;
			SoundInfo sound;
		};

		// Every top-level INDSOUND of the file
		bool SoundChunks(Image const &image, Span const &root, std::vector<SoundAt> *out)
		{
			std::vector<Span> top;

			out->clear();

			if (!Children(image, root, &top))
			{
				return false;
			}

			for (Span const &child : top)
			{
				if (!NameIs(image, child.at, "INDSOUND"))
				{
					continue;
				}

				size_t at = child.at + kHeadingBytes;
				size_t const end = child.at + child.size;

				if (end - at < 4)
				{
					return false;
				}

				SoundAt made;

				made.span = child;
				made.sound.index = IntAt(image, at);
				at += 4;

				char const *const first = image.data() + at;
				char const *const zero = (char const *)memchr(first, 0, end - at);

				if (!zero)
				{
					return false;
				}

				made.sound.wav.assign(first, (size_t)(zero - first));

				size_t const padded = (made.sound.wav.size() + 4) & ~(size_t)3;

				if (end - at < padded || end - at - padded < 24)
				{
					return false;
				}

				at += padded;
				made.sound.inner = IntAt(image, at);
				made.sound.outer = IntAt(image, at + 4);
				made.sound.volume = IntAt(image, at + 8);
				made.sound.pitch = IntAt(image, at + 12);
				made.sound.flags = (uint32_t)IntAt(image, at + 16);
				out->push_back(made);
			}

			return true;
		}

		// An INDSOUND chunk as the game writes it (chunk_sound.cpp): index, the name padded to (length + 4) & ~3, five numbers, an empty extra-data list
		Image SoundChunk(SoundInfo const &sound)
		{
			size_t const padded = (sound.wav.size() + 4) & ~(size_t)3;
			Image chunk(kHeadingBytes + 4 + padded + 20 + 4, 0);

			memcpy(chunk.data(), "INDSOUND", 8);
			PutIntAt(&chunk, 8, (int32_t)chunk.size());
			PutIntAt(&chunk, kHeadingBytes, sound.index);
			memcpy(chunk.data() + kHeadingBytes + 4, sound.wav.data(), sound.wav.size());

			size_t const at = kHeadingBytes + 4 + padded;

			PutIntAt(&chunk, at, sound.inner);
			PutIntAt(&chunk, at + 4, sound.outer);
			PutIntAt(&chunk, at + 8, sound.volume);
			PutIntAt(&chunk, at + 12, sound.pitch);
			PutIntAt(&chunk, at + 16, (int32_t)sound.flags);
			PutIntAt(&chunk, at + 20, 0);
			return chunk;
		}

		bool PlainSoundName(std::string const &wav)
		{
			if (wav.empty() || wav.size() > 255)
			{
				return false;
			}

			for (char const c : wav)
			{
				if ((unsigned char)c < 32 || (unsigned char)c > 126)
				{
					return false;
				}
			}

			return true;
		}
	}

	HierarchyEditResult HierarchyEdit_RootFrames(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, std::vector<FrameInfo> *frames)
	{
		Image image;
		Span root;

		if (!frames)
		{
			return HierarchyEditResult::BadCall;
		}

		frames->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		Span section;
		HierarchyEditResult const found = RootSection(image, root, hierarchy, &section);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		std::vector<FrameAt> list;
		HierarchyEditResult const read = FramesOf(image, section, number, subNumber, &list);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		for (FrameAt const &each : list)
		{
			FrameInfo made = { each.at, (uint32_t)IntAt(image, each.flagsAt) };

			frames->push_back(made);
		}

		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_ChangeFrameAt(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t position, uint32_t setMask,
		uint32_t clearMask, int32_t sound, size_t *frame)
	{
		Image image;
		Span root;

		if (sound < -1 || sound > 127)
		{
			return HierarchyEditResult::BadCall;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		Span section;
		HierarchyEditResult const found = RootSection(image, root, hierarchy, &section);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		std::vector<FrameAt> list;
		HierarchyEditResult const read = FramesOf(image, section, number, subNumber, &list);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		size_t index = 0;

		if (!FrameHolding(list, position, &index))
		{
			return HierarchyEditResult::NoSequence;
		}

		uint32_t flags = ((uint32_t)IntAt(image, list[index].flagsAt) | setMask) & ~clearMask;

		if (sound >= 0)
		{
			flags = (flags & 0x80ffffffu) | ((uint32_t)sound << 24);
		}

		Image out = image;

		PutIntAt(&out, list[index].flagsAt, (int32_t)flags);

		HierarchyEditResult const done = Replace(file, out);

		if (done == HierarchyEditResult::Done && frame)
		{
			*frame = index;
		}

		return done;
	}

	namespace
	{
		// The root section's frames of one sequence, each changed in place by `change` (the image, the frame's first byte); the file is replaced once, after every frame
		template <typename Change>
		HierarchyEditResult ChangeRootFrames(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, Change change)
		{
			Image image;
			Span root;
			HierarchyEditResult const opened = Open(file, &image, &root);

			if (opened != HierarchyEditResult::Done)
			{
				return opened;
			}

			Span section;
			HierarchyEditResult const found = RootSection(image, root, hierarchy, &section);

			if (found != HierarchyEditResult::Done)
			{
				return found;
			}

			std::vector<FrameAt> list;
			HierarchyEditResult const read = FramesOf(image, section, number, subNumber, &list);

			if (read != HierarchyEditResult::Done)
			{
				return read;
			}

			Image out = image;

			for (FrameAt const &each : list)
			{
				// the orientation (16) and the transform (12) lie inside the frame (FramesOf checked each frame's size)
				if (each.baseAt + 28 > out.size())
				{
					return HierarchyEditResult::BadLevelBytes;
				}

				change(&out, each.baseAt);
			}

			return Replace(file, out);
		}
	}

	HierarchyEditResult HierarchyEdit_TurnRootFrames(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber)
	{
		return ChangeRootFrames(file, hierarchy, number, subNumber, [](Image *out, size_t at)
		{
			// FUN_00480eb0 on the frame's chunk (+0x4c): its floats +0x28..+0x34 (x, y, z, w) become (-z, -w, x, y), a minus being a flip of the sign bit, as the x87's FCHS makes it; its
			// transform's x (+0x38) and z (+0x40) are negated (32-bit, as the exe's NEG)
			uint32_t q[4];

			for (int i = 0; i < 4; i++)
			{
				q[i] = (uint32_t)IntAt(*out, at + (size_t)i * 4);
			}

			uint32_t const turned[4] = { q[2] ^ 0x80000000u, q[3] ^ 0x80000000u, q[0], q[1] };

			for (int i = 0; i < 4; i++)
			{
				PutIntAt(out, at + (size_t)i * 4, (int32_t)turned[i]);
			}

			PutIntAt(out, at + 16, (int32_t)(0u - (uint32_t)IntAt(*out, at + 16)));
			PutIntAt(out, at + 24, (int32_t)(0u - (uint32_t)IntAt(*out, at + 24)));
		});
	}

	HierarchyEditResult HierarchyEdit_LowerRootFrames(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t by)
	{
		return ChangeRootFrames(file, hierarchy, number, subNumber, [by](Image *out, size_t at)
		{
			// FUN_004795d0 0x479699-0x4796a5: the chunk's transform y (+0x3c) less the lowest point (32-bit, as the exe's SUB)
			PutIntAt(out, at + 20, (int32_t)((uint32_t)IntAt(*out, at + 20) - (uint32_t)by));
		});
	}

	HierarchyEditResult HierarchyEdit_Sounds(OpenLevelFile *file, std::vector<SoundInfo> *sounds)
	{
		Image image;
		Span root;

		if (!sounds)
		{
			return HierarchyEditResult::BadCall;
		}

		sounds->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<SoundAt> all;

		if (!SoundChunks(image, root, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (SoundAt const &each : all)
		{
			sounds->push_back(each.sound);
		}

		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_NewSound(OpenLevelFile *file, int32_t index, double levelScale, char const **why)
	{
		Image image;
		Span root;

		Why(why, nullptr);

		if (!(levelScale > 0.0) || levelScale > 1000000.0)
		{
			return HierarchyEditResult::BadCall;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<SoundAt> all;

		if (!SoundChunks(image, root, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		std::vector<int32_t> used;

		for (SoundAt const &each : all)
		{
			used.push_back(each.sound.index);
		}

		model::SoundIndexResult const rule = model::CheckNewSoundIndex(used, index);

		if (rule != model::SoundIndexResult::Ok)
		{
			Why(why, model::SoundIndexWords(rule));
			return HierarchyEditResult::Refused;
		}

		SoundInfo made;

		made.index = index;
		made.wav = "Not Set";
		made.inner = (int32_t)(500000.0 / (100.0 * levelScale));
		made.outer = (int32_t)(4000000.0 / (100.0 * levelScale));
		made.volume = 127;
		made.pitch = 0;
		made.flags = 0;

		Image out;
		std::vector<size_t> const ancestors = { root.at };

		if (!Spliced(image, ancestors, image.size(), 0, SoundChunk(made), &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		return Replace(file, out);
	}

	HierarchyEditResult HierarchyEdit_EditSound(OpenLevelFile *file, int32_t index, std::string const &wav, int32_t inner, int32_t outer, int32_t volume, int32_t pitch, char const **why)
	{
		Image image;
		Span root;

		Why(why, nullptr);

		if (!PlainSoundName(wav) || inner < 0 || outer < 0 || volume < 0 || volume > 127)
		{
			Why(why, "a sound's name is 1 to 255 plain letters, its ranges are not negative and its volume is 0 to 127");
			return HierarchyEditResult::Refused;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<SoundAt> all;

		if (!SoundChunks(image, root, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (SoundAt const &each : all)
		{
			if (each.sound.index != index)
			{
				continue;
			}

			SoundInfo changed = each.sound;

			changed.wav = wav;
			changed.inner = inner;
			changed.outer = outer;
			changed.volume = volume;
			changed.pitch = pitch;
			changed.flags &= ~1u;

			Image out;
			std::vector<size_t> const ancestors = { root.at };

			if (!Spliced(image, ancestors, each.span.at, each.span.size, SoundChunk(changed), &out))
			{
				return HierarchyEditResult::CantBuild;
			}

			return Replace(file, out);
		}

		Why(why, "no sound has that index");
		return HierarchyEditResult::Refused;
	}

	HierarchyEditResult HierarchyEdit_DeleteSound(OpenLevelFile *file, int32_t index, char const **why)
	{
		Image image;
		Span root;

		Why(why, nullptr);

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<SoundAt> all;

		if (!SoundChunks(image, root, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (SoundAt const &each : all)
		{
			if (each.sound.index == index)
			{
				Image out;
				std::vector<size_t> const ancestors = { root.at };

				if (!Spliced(image, ancestors, each.span.at, each.span.size, Image(), &out))
				{
					return HierarchyEditResult::CantBuild;
				}

				return Replace(file, out);
			}
		}

		Why(why, "no sound has that index");
		return HierarchyEditResult::Refused;
	}

	HierarchyEditResult HierarchyEdit_ReadSequenceTime(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, int32_t *milliseconds, bool *present)
	{
		Image image;
		Span root;

		if (!milliseconds || !present)
		{
			return HierarchyEditResult::BadCall;
		}

		*milliseconds = 0;
		*present = false;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<size_t> ancestors;
		Span sequenceSpan;
		HierarchyEditResult const found = FindSequence(image, root, hierarchy, sequence, &ancestors, &sequenceSpan);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;

		if (!FirstChild(image, sequenceSpan, "OBASEQTM", &existing, &walkable))
		{
			return walkable ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
		}

		if (existing.size < kHeadingBytes + 4)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		*milliseconds = IntAt(image, existing.at + kHeadingBytes);
		*present = true;
		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_SetSequenceTime(OpenLevelFile *file, std::string const &hierarchy, size_t sequence, int32_t milliseconds)
	{
		Image image;
		Span root;

		if (milliseconds < 1)
		{
			return HierarchyEditResult::BadCall;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<size_t> ancestors;
		Span sequenceSpan;
		HierarchyEditResult const found = FindSequence(image, root, hierarchy, sequence, &ancestors, &sequenceSpan);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;
		bool const has = FirstChild(image, sequenceSpan, "OBASEQTM", &existing, &walkable);

		if (!has && !walkable)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		if (has && existing.size < kHeadingBytes + 4)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		Image out;
		bool spliced;

		ancestors.push_back(sequenceSpan.at);

		if (has)
		{
			Image chunk(image.begin() + (std::ptrdiff_t)existing.at, image.begin() + (std::ptrdiff_t)(existing.at + existing.size));

			PutIntAt(&chunk, kHeadingBytes, milliseconds);
			spliced = Spliced(image, ancestors, existing.at, existing.size, chunk, &out);
		}
		else
		{
			uint8_t bytes[4];

			memcpy(bytes, &milliseconds, sizeof(bytes));
			spliced = Spliced(image, ancestors, sequenceSpan.at + sequenceSpan.size, 0, MakeChunk("OBASEQTM", bytes, sizeof(bytes)), &out);
		}

		if (!spliced)
		{
			return HierarchyEditResult::CantBuild;
		}

		return Replace(file, out);
	}

	namespace
	{
		struct SectionAt
		{
			Span chunk;                        // its OBJHIERD
			std::string name;                  // the object's name
			std::vector<size_t> ancestors;     // the chunks around it, outermost first (the file, the hierarchy, the section chunks down to the one holding it)
		};

		// The sections under the section chunk `holder` (an OBJCHIER): its OBJHIERD, then its children's, depth first
		bool SectionsUnder(Image const &image, Span const &holder, std::vector<size_t> const &above, std::vector<SectionAt> *out, int depth)
		{
			std::vector<Span> inside;

			if (depth > 64 || !Children(image, holder, &inside))
			{
				return false;
			}

			std::vector<size_t> mine = above;

			mine.push_back(holder.at);

			for (Span const &child : inside)
			{
				if (NameIs(image, child.at, "OBJHIERD"))
				{
					if (child.size < kHeadingBytes + 4)
					{
						return false;
					}

					int32_t const extra = IntAt(image, child.at + kHeadingBytes);

					if (extra < 0 || (size_t)extra > (child.size - kHeadingBytes - 4) / 4)
					{
						return false;
					}

					size_t const nameAt = child.at + kHeadingBytes + 4 + (size_t)extra * 4;
					char const *const first = image.data() + nameAt;
					char const *const zero = (char const *)memchr(first, 0, child.at + child.size - nameAt);

					if (!zero)
					{
						return false;
					}

					SectionAt made;

					made.chunk = child;
					made.name.assign(first, (size_t)(zero - first));
					made.ancestors = mine;
					out->push_back(made);
				}
			}

			for (Span const &child : inside)
			{
				if (NameIs(image, child.at, "OBJCHIER") && !SectionsUnder(image, child, mine, out, depth + 1))
				{
					return false;
				}
			}

			return true;
		}

		HierarchyEditResult SectionsOf(Image const &image, Span const &root, std::string const &hierarchy, std::vector<SectionAt> *out)
		{
			Span section;
			HierarchyEditResult const found = RootSection(image, root, hierarchy, &section);

			if (found != HierarchyEditResult::Done)
			{
				return found;
			}

			std::vector<HierarchyAt> all;

			Hierarchies(image, root, &all);

			std::vector<size_t> above = { root.at };

			for (HierarchyAt const &each : all)
			{
				if (each.name == hierarchy)
				{
					above.push_back(each.span.at);
					break;
				}
			}

			out->clear();
			return SectionsUnder(image, section, above, out, 0) ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
		}

		// The names of the level's RBOBJECT chunks (the name is in the OBJHEAD1 child, 60 bytes into its data)
		bool ObjectNamesOf(Image const &image, Span const &root, std::vector<std::string> *names)
		{
			std::vector<Span> top;

			names->clear();

			if (!Children(image, root, &top))
			{
				return false;
			}

			for (Span const &child : top)
			{
				if (!NameIs(image, child.at, "RBOBJECT"))
				{
					continue;
				}

				Span head;
				bool walkable = false;

				if (!FirstChild(image, child, "OBJHEAD1", &head, &walkable))
				{
					if (!walkable)
					{
						return false;
					}

					continue;
				}

				size_t const nameAt = head.at + kHeadingBytes + 60;

				if (head.size < kHeadingBytes + 61)
				{
					return false;
				}

				char const *const first = image.data() + nameAt;
				char const *const zero = (char const *)memchr(first, 0, head.at + head.size - nameAt);

				if (!zero)
				{
					return false;
				}

				names->push_back(std::string(first, (size_t)(zero - first)));
			}

			return true;
		}
	}

	HierarchyEditResult HierarchyEdit_SectionObjects(OpenLevelFile *file, std::string const &hierarchy, std::vector<std::string> *names)
	{
		Image image;
		Span root;

		if (!names)
		{
			return HierarchyEditResult::BadCall;
		}

		names->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<SectionAt> sections;
		HierarchyEditResult const read = SectionsOf(image, root, hierarchy, &sections);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		for (SectionAt const &each : sections)
		{
			names->push_back(each.name);
		}

		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_ObjectNames(OpenLevelFile *file, std::vector<std::string> *names)
	{
		Image image;
		Span root;

		if (!names)
		{
			return HierarchyEditResult::BadCall;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		return ObjectNamesOf(image, root, names) ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
	}

	HierarchyEditResult HierarchyEdit_ChangeSectionObject(OpenLevelFile *file, std::string const &hierarchy, std::string const &oldName, std::string const &newName, char const **why)
	{
		Image image;
		Span root;

		Why(why, nullptr);

		if (newName.empty() || newName.size() > 255)
		{
			Why(why, "an object name is 1 to 255 letters");
			return HierarchyEditResult::Refused;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<SectionAt> sections;
		HierarchyEditResult const read = SectionsOf(image, root, hierarchy, &sections);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		SectionAt const *target = nullptr;

		for (SectionAt const &each : sections)
		{
			if (each.name == newName)
			{
				Why(why, "That object is already used by this hierarchy");
				return HierarchyEditResult::Refused;
			}

			if (each.name == oldName && !target)
			{
				target = &each;
			}
		}

		if (!target)
		{
			Why(why, "the hierarchy has no section with that object");
			return HierarchyEditResult::Refused;
		}

		// the chunk as the game writes it: the extra numbers as they are, then the name padded to length + 4 - length % 4
		int32_t const extra = IntAt(image, target->chunk.at + kHeadingBytes);
		size_t const padded = newName.size() + 4 - newName.size() % 4;
		Image chunk(kHeadingBytes + 4 + (size_t)extra * 4 + padded, 0);

		memcpy(chunk.data(), "OBJHIERD", 8);
		PutIntAt(&chunk, 8, (int32_t)chunk.size());
		memcpy(chunk.data() + kHeadingBytes, image.data() + target->chunk.at + kHeadingBytes, 4 + (size_t)extra * 4);
		memcpy(chunk.data() + kHeadingBytes + 4 + (size_t)extra * 4, newName.data(), newName.size());

		Image out;

		if (!Spliced(image, target->ancestors, target->chunk.at, target->chunk.size, chunk, &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		return Replace(file, out);
	}

	HierarchyEditResult HierarchyEdit_CopyFrameSounds(OpenLevelFile *file, std::string const &from, std::string const &to, size_t *copied, std::vector<std::string> *errors)
	{
		Image image;
		Span root;

		if (!copied || !errors || from == to)
		{
			return HierarchyEditResult::BadCall;
		}

		*copied = 0;
		errors->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<SequenceInfo> fromSequences, toSequences;
		HierarchyEditResult const a = HierarchyEdit_Sequences(file, from, &fromSequences);
		HierarchyEditResult const b = HierarchyEdit_Sequences(file, to, &toSequences);

		if (a != HierarchyEditResult::Done)
		{
			return a;
		}

		if (b != HierarchyEditResult::Done)
		{
			return b;
		}

		Span fromSection, toSection;
		HierarchyEditResult const fromFound = RootSection(image, root, from, &fromSection);
		HierarchyEditResult const toFound = RootSection(image, root, to, &toSection);

		if (fromFound != HierarchyEditResult::Done)
		{
			return fromFound;
		}

		if (toFound != HierarchyEditResult::Done)
		{
			return toFound;
		}

		Image out = image;
		bool changed = false;

		for (SequenceInfo const &one : fromSequences)
		{
			bool shared = false;

			for (SequenceInfo const &other : toSequences)
			{
				shared = shared || (other.number == one.number && other.subNumber == one.subNumber);
			}

			if (!shared)
			{
				continue;
			}

			std::vector<FrameAt> fromFrames, toFrames;

			if (FramesOf(image, fromSection, one.number, one.subNumber, &fromFrames) != HierarchyEditResult::Done || FramesOf(image, toSection, one.number, one.subNumber, &toFrames) != HierarchyEditResult::Done)
			{
				continue;
			}

			if (fromFrames.size() != toFrames.size())
			{
				errors->push_back(std::to_string((long long)one.number) + "." + std::to_string((long long)one.subNumber) + ": the first has " + std::to_string((unsigned long long)fromFrames.size()) +
					" frames, the second " + std::to_string((unsigned long long)toFrames.size()));
				continue;
			}

			for (FrameAt const &source : fromFrames)
			{
				uint32_t const sound = ((uint32_t)IntAt(image, source.flagsAt) >> 24) & 0x7f;

				if (sound == 0)
				{
					continue;
				}

				for (FrameAt const &target : toFrames)
				{
					if (target.at == source.at)
					{
						uint32_t const flags = ((uint32_t)IntAt(out, target.flagsAt) & 0x80ffffffu) | (sound << 24);

						PutIntAt(&out, target.flagsAt, (int32_t)flags);
						changed = true;
						break;
					}
				}
			}

			(*copied)++;
		}

		return changed ? Replace(file, out) : HierarchyEditResult::Done;
	}

	namespace
	{
		// Every OBJCHIER of the section tree under `holder` (itself first), depth first
		bool SectionChunks(Image const &image, Span const &holder, std::vector<Span> *out, int depth)
		{
			std::vector<Span> inside;

			if (depth > 64 || !Children(image, holder, &inside))
			{
				return false;
			}

			out->push_back(holder);

			for (Span const &child : inside)
			{
				if (NameIs(image, child.at, "OBJCHIER") && !SectionChunks(image, child, out, depth + 1))
				{
					return false;
				}
			}

			return true;
		}
	}

	HierarchyEditResult HierarchyEdit_SetDelta(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, DeltaMode mode, int32_t position, size_t *sections)
	{
		Image image;
		Span root;

		if (!sections)
		{
			return HierarchyEditResult::BadCall;
		}

		*sections = 0;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		Span top;
		HierarchyEditResult const found = RootSection(image, root, hierarchy, &top);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		std::vector<Span> all;

		if (!SectionChunks(image, top, &all, 0))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		Image out = image;

		for (Span const &one : all)
		{
			std::vector<FrameAt> frames;

			if (FramesOf(image, one, number, subNumber, &frames) != HierarchyEditResult::Done || frames.empty())
			{
				continue;
			}

			(*sections)++;

			for (FrameAt const &frame : frames)
			{
				PutIntAt(&out, frame.flagsAt, (int32_t)((uint32_t)IntAt(out, frame.flagsAt) & 0x7fffffffu));
			}

			size_t pick = 0;
			bool set = true;

			switch (mode)
			{
			case DeltaMode::None:
				set = false;
				break;

			case DeltaMode::First:
				pick = 0;
				break;

			case DeltaMode::Middle:
				pick = frames.size() / 2;
				break;

			default:
				pick = frames.size() - 1;
				FrameHolding(frames, position, &pick);
				break;
			}

			if (set)
			{
				PutIntAt(&out, frames[pick].flagsAt, (int32_t)((uint32_t)IntAt(out, frames[pick].flagsAt) | 0x80000000u));
			}
		}

		if (*sections == 0)
		{
			return HierarchyEditResult::NoSequence;
		}

		return Replace(file, out);
	}

	HierarchyEditResult HierarchyEdit_DeltaFrame(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t *frame)
	{
		std::vector<FrameInfo> frames;

		if (!frame)
		{
			return HierarchyEditResult::BadCall;
		}

		*frame = -1;

		HierarchyEditResult const read = HierarchyEdit_RootFrames(file, hierarchy, number, subNumber, &frames);

		if (read != HierarchyEditResult::Done)
		{
			return read;
		}

		for (size_t i = 0; i < frames.size(); i++)
		{
			if ((frames[i].flags & 0x80000000u) != 0)
			{
				*frame = (int32_t)i;
				break;
			}
		}

		return HierarchyEditResult::Done;
	}

	namespace
	{
		// The span of the chunk that holds a sequence's (or the hierarchy's) HIERBBOX, with the chunks around it
		HierarchyEditResult BoxHolder(Image const &image, Span const &root, std::string const &hierarchy, int sequence, Span *holder, std::vector<size_t> *ancestors)
		{
			if (sequence >= 0)
			{
				HierarchyEditResult const found = FindSequence(image, root, hierarchy, (size_t)sequence, ancestors, holder);

				if (found == HierarchyEditResult::Done)
				{
					ancestors->push_back(holder->at);
				}

				return found;
			}

			std::vector<HierarchyAt> all;

			if (!Hierarchies(image, root, &all))
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			for (HierarchyAt const &each : all)
			{
				if (each.name == hierarchy)
				{
					*holder = each.span;
					*ancestors = { root.at, each.span.at };
					return HierarchyEditResult::Done;
				}
			}

			return HierarchyEditResult::NoHierarchy;
		}
	}

	HierarchyEditResult HierarchyEdit_WriteBoundingBox(OpenLevelFile *file, std::string const &hierarchy, int sequence, int32_t const minimum[3], int32_t const maximum[3])
	{
		Image image;
		Span root;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		Span holder;
		std::vector<size_t> ancestors;
		HierarchyEditResult const found = BoxHolder(image, root, hierarchy, sequence, &holder, &ancestors);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;
		bool const has = FirstChild(image, holder, "HIERBBOX", &existing, &walkable);

		if (!has && !walkable)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		if (has && existing.size < kHeadingBytes + 24)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		uint8_t bytes[24];

		memcpy(bytes, minimum, 12);
		memcpy(bytes + 12, maximum, 12);

		Image out;
		bool spliced;

		if (has)
		{
			Image chunk(image.begin() + (std::ptrdiff_t)existing.at, image.begin() + (std::ptrdiff_t)(existing.at + existing.size));

			memcpy(chunk.data() + kHeadingBytes, bytes, 24);
			spliced = Spliced(image, ancestors, existing.at, existing.size, chunk, &out);
		}
		else
		{
			spliced = Spliced(image, ancestors, holder.at + holder.size, 0, MakeChunk("HIERBBOX", bytes, sizeof(bytes)), &out);
		}

		return spliced ? Replace(file, out) : HierarchyEditResult::CantBuild;
	}

	HierarchyEditResult HierarchyEdit_ReadBoundingBox(OpenLevelFile *file, std::string const &hierarchy, int sequence, int32_t minimum[3], int32_t maximum[3], bool *present)
	{
		Image image;
		Span root;

		if (!present)
		{
			return HierarchyEditResult::BadCall;
		}

		*present = false;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		Span holder;
		std::vector<size_t> ancestors;
		HierarchyEditResult const found = BoxHolder(image, root, hierarchy, sequence, &holder, &ancestors);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		bool walkable = false;
		Span existing;

		if (!FirstChild(image, holder, "HIERBBOX", &existing, &walkable))
		{
			return walkable ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
		}

		if (existing.size < kHeadingBytes + 24)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (int i = 0; i < 3; i++)
		{
			minimum[i] = IntAt(image, existing.at + kHeadingBytes + (size_t)i * 4);
			maximum[i] = IntAt(image, existing.at + kHeadingBytes + 12 + (size_t)i * 4);
		}

		*present = true;
		return HierarchyEditResult::Done;
	}

	namespace
	{
		typedef double Matrix3[9];

		// FUN_00430d70: a quaternion (x, y, z, w as the file's floats) as a 3 x 3 of doubles, the float and double steps as the decompile shows them
		void QuatToMatrix(float const q[4], Matrix3 m)
		{
			float const x = q[0];
			double const y = (double)q[1];
			double const z = (double)q[2];
			float const w = q[3];
			double const y2 = y + y;
			double const z2 = z + z;
			double const xw = (double)((x + x) * w);
			float const xx = (x + x) * x;

			m[0] = 1.0 - (z2 * z + y2 * y);
			m[3] = (double)((float)y2 * x) + (double)((float)z2 * w);
			m[6] = (double)(x * (float)z2 - (float)y2 * w);
			m[1] = (double)((float)y2 * x) - (double)((float)z2 * w);
			m[4] = 1.0 - (z2 * z + (double)xx);
			m[7] = z2 * y + xw;
			m[2] = (double)(x * (float)z2 + (float)y2 * w);
			m[5] = z2 * y - xw;
			m[8] = 1.0 - (y2 * y + (double)xx);
		}

		int32_t Truncated(double value)
		{
			return (int32_t)value;   // __ftol: toward zero
		}

		// FUN_00430e80: a whole-number vector turned by a matrix (row by row), each number cut toward zero
		void RotateVector(int32_t v[3], Matrix3 const m)
		{
			double const x = v[0], y = v[1], z = v[2];

			v[0] = Truncated(x * m[0] + y * m[1] + z * m[2]);
			v[1] = Truncated(x * m[3] + y * m[4] + z * m[5]);
			v[2] = Truncated(x * m[6] + y * m[7] + z * m[8]);
		}

		// FUN_00430fa0: out = a x b
		void MultiplyMatrices(Matrix3 out, Matrix3 const a, Matrix3 const b)
		{
			double result[9];

			for (int row = 0; row < 3; row++)
			{
				for (int column = 0; column < 3; column++)
				{
					result[row * 3 + column] = a[row * 3 + 2] * b[6 + column] + b[3 + column] * a[row * 3 + 1] + b[column] * a[row * 3];
				}
			}

			memcpy(out, result, sizeof(result));
		}

		// FUN_00430f60: the matrix turned over its diagonal (a turn's inverse)
		void TransposeMatrix(Matrix3 m)
		{
			std::swap(m[1], m[3]);
			std::swap(m[2], m[6]);
			std::swap(m[5], m[7]);
		}

		struct ReferenceFrame
		{
			float quaternion[4];
			int32_t offset[3];
		};

		// The frames of a section for the sequence with that number and sub-number: how many there are and the first (the section's chunk holds them as OBANALLS or OBANSEQS > OBANSEQC > OBASEQFR)
		int FramesFor(Image const &image, Span const &section, int32_t number, int32_t subNumber, ReferenceFrame *first)
		{
			std::vector<Span> inside;

			if (!Children(image, section, &inside))
			{
				return -1;
			}

			for (Span const &child : inside)
			{
				if (NameIs(image, child.at, "OBANALLS"))
				{
					size_t at = child.at + kHeadingBytes;
					size_t const end = child.at + child.size;

					if (end - at < 4)
					{
						return -1;
					}

					int32_t const count = IntAt(image, at);

					at += 4;

					for (int32_t i = 0; i < count; i++)
					{
						if (end - at < 16)
						{
							return -1;
						}

						int32_t const frames = IntAt(image, at);
						bool const mine = IntAt(image, at + 4) == number && IntAt(image, at + 8) == subNumber;

						at += 16;

						if (frames < 0 || (size_t)frames > (end - at) / 36)
						{
							return -1;
						}

						if (mine)
						{
							if (frames >= 1)
							{
								memcpy(first->quaternion, image.data() + at, 16);
								memcpy(first->offset, image.data() + at + 16, 12);
							}

							return frames;
						}

						at += (size_t)frames * 36;
					}
				}
				else if (NameIs(image, child.at, "OBANSEQS"))
				{
					std::vector<Span> sequences;

					if (!Children(image, child, &sequences))
					{
						return -1;
					}

					for (Span const &sequence : sequences)
					{
						Span header;
						bool walkable = false;

						if (!NameIs(image, sequence.at, "OBANSEQC") || !FirstChild(image, sequence, "OBASEQHD", &header, &walkable) || header.size < kHeadingBytes + 12 ||
							IntAt(image, header.at + kHeadingBytes + 4) != number || IntAt(image, header.at + kHeadingBytes + 8) != subNumber)
						{
							continue;
						}

						std::vector<Span> parts;

						if (!Children(image, sequence, &parts))
						{
							return -1;
						}

						int frames = 0;

						for (Span const &part : parts)
						{
							if (NameIs(image, part.at, "OBASEQFR") && part.size >= kHeadingBytes + 28)
							{
								if (frames == 0)
								{
									memcpy(first->quaternion, image.data() + part.at + kHeadingBytes, 16);
									memcpy(first->offset, image.data() + part.at + kHeadingBytes + 16, 12);
								}

								frames++;
							}
						}

						return frames;
					}
				}
			}

			return 0;
		}

		struct Bone
		{
			std::string name;
			int32_t low[3];
			int32_t high[3];
			int32_t place[3];
			Matrix3 inverse;
		};

		// FUN_0041edb0: the bone of a section (and, first, its children's), from the one-frame reference sequence. Gives false (nothing made for it) when the section has no object name or
		// not exactly one frame of the sequence. *frame gets the section's frame: its parent reads the offset from it.
		bool MakeBones(Image const &image, Span const &section, int32_t number, int32_t subNumber, Matrix3 const parentMatrix, int32_t const parentPlace[3], std::vector<Bone> *bones, ReferenceFrame *frame, int depth)
		{
			std::vector<Span> inside;

			if (depth > 64 || !Children(image, section, &inside))
			{
				return false;
			}

			// the section's object name (its OBJHIERD)
			std::string name;
			bool named = false;

			for (Span const &child : inside)
			{
				if (NameIs(image, child.at, "OBJHIERD") && child.size >= kHeadingBytes + 4)
				{
					int32_t const extra = IntAt(image, child.at + kHeadingBytes);

					if (extra >= 0 && (size_t)extra <= (child.size - kHeadingBytes - 4) / 4)
					{
						size_t const nameAt = child.at + kHeadingBytes + 4 + (size_t)extra * 4;
						char const *const zero = (char const *)memchr(image.data() + nameAt, 0, child.at + child.size - nameAt);

						if (zero)
						{
							name.assign(image.data() + nameAt, (size_t)(zero - (image.data() + nameAt)));
							named = true;
						}
					}

					break;
				}
			}

			ReferenceFrame mine;

			if (!named || FramesFor(image, section, number, subNumber, &mine) != 1)
			{
				return false;
			}

			Matrix3 local, absolute;

			QuatToMatrix(mine.quaternion, local);

			int32_t offset[3] = { mine.offset[0], mine.offset[1], mine.offset[2] };

			RotateVector(offset, parentMatrix);

			int32_t place[3] = { parentPlace[0] + offset[0], parentPlace[1] + offset[1], parentPlace[2] + offset[2] };

			MultiplyMatrices(absolute, parentMatrix, local);

			int32_t low[3] = { -10, -10, -10 };
			int32_t high[3] = { 10, 10, 10 };
			size_t childSections = 0;

			for (Span const &child : inside)
			{
				if (!NameIs(image, child.at, "OBJCHIER"))
				{
					continue;
				}

				childSections++;

				ReferenceFrame childFrame;

				if (MakeBones(image, child, number, subNumber, absolute, place, bones, &childFrame, depth + 1))
				{
					for (int axis = 0; axis < 3; axis++)
					{
						low[axis] = childFrame.offset[axis] < low[axis] ? childFrame.offset[axis] : low[axis];
						high[axis] = childFrame.offset[axis] > high[axis] ? childFrame.offset[axis] : high[axis];
					}
				}
			}

			if (childSections == 0)
			{
				// a section with no children has its box made three times as large (0x41f0d1-0x41f0fc)
				for (int axis = 0; axis < 3; axis++)
				{
					low[axis] *= 3;
					high[axis] *= 3;
				}
			}

			Bone made;

			made.name = name;
			memcpy(made.low, low, sizeof(low));
			memcpy(made.high, high, sizeof(high));
			memcpy(made.place, place, sizeof(place));
			memcpy(made.inverse, absolute, sizeof(absolute));
			TransposeMatrix(made.inverse);
			bones->push_back(made);
			*frame = mine;
			return true;
		}

		// TB-3, FUN_0041eae0 (after Attach 0x41f804 and Shift+V 0x47ad94): each SHPBONDT point record's +4..+0xc = the point's place relative to its bone in the reference pose: the shape's point
		// turned by the object's quaternion (FUN_00430d70, then FUN_00430e80: M times the point, each result cut toward zero), the object's place added (+0x44, FUN_0042f8e0), the bone's reference
		// place taken away (FUN_0042f910), then turned by the bone's reference matrix transposed (FUN_0041e750, FUN_00430f60; FUN_00430e80 again): NearestBone's sum. A bone the reference pose
		// doesn't have (FUN_0041e750 gives 0) keeps the bytes it had: *offsets is read for those. NAMED: a hierarchy with two sections of one name, inteng takes the first in its walk (the parent
		// before its children), ours the first MakeBones made (the children first).
		void BonePointOffsets(std::vector<Bone> const &bones, Matrix3 const objectMatrix, int32_t const objectPlace[3], Image const &image, size_t verticesAt, size_t pointCount,
			std::vector<std::pair<std::string, std::vector<int32_t>>> const &assigned, std::vector<std::vector<std::array<int32_t, 3>>> *offsets)
		{
			offsets->resize(assigned.size());

			for (size_t b = 0; b < assigned.size(); b++)
			{
				(*offsets)[b].resize(assigned[b].second.size(), std::array<int32_t, 3>{ { 0, 0, 0 } });

				Bone const *mine = nullptr;

				for (Bone const &bone : bones)
				{
					if (!mine && bone.name == assigned[b].first)
					{
						mine = &bone;
					}
				}

				for (size_t k = 0; mine && k < assigned[b].second.size(); k++)
				{
					int32_t const index = assigned[b].second[k];

					if (index < 0 || (size_t)index >= pointCount)
					{
						continue;
					}

					int32_t place[3];

					memcpy(place, image.data() + verticesAt + (size_t)index * 12, 12);
					RotateVector(place, objectMatrix);

					for (int axis = 0; axis < 3; axis++)
					{
						place[axis] = place[axis] + objectPlace[axis] - mine->place[axis];
					}

					RotateVector(place, mine->inverse);
					(*offsets)[b][k] = { { place[0], place[1], place[2] } };
				}
			}
		}

		// FUN_0041f2b0: the name of the bone nearest the point (the point turned by the object's turn and moved to the object's place, then into each bone's own frame), or empty. A bone's distance is
		// the length of the per-axis distance the point lies OUTSIDE the bone's box, cut toward zero; the first bone with the smallest wins.
		bool NearestBone(std::vector<Bone> const &bones, Matrix3 const objectMatrix, int32_t const objectPlace[3], int32_t const point[3], std::string *name)
		{
			int32_t world[3] = { point[0], point[1], point[2] };

			RotateVector(world, objectMatrix);
			world[0] += objectPlace[0];
			world[1] += objectPlace[1];
			world[2] += objectPlace[2];

			int32_t best = 0x7fffffff;
			bool found = false;

			for (Bone const &bone : bones)
			{
				int32_t local[3] = { world[0] - bone.place[0], world[1] - bone.place[1], world[2] - bone.place[2] };

				RotateVector(local, bone.inverse);

				double outside[3];

				for (int axis = 0; axis < 3; axis++)
				{
					int32_t farther = local[axis] - bone.high[axis];

					if (farther <= bone.low[axis] - local[axis])
					{
						farther = bone.low[axis] - local[axis];
					}

					outside[axis] = farther < 0 ? 0.0 : (double)farther;
				}

				int32_t const distance = Truncated(sqrt(outside[2] * outside[2] + outside[1] * outside[1] + outside[0] * outside[0]));

				if (distance < best)
				{
					best = distance;
					*name = bone.name;
					found = true;
				}
			}

			return found;
		}

		// The object named so: its chunk's place and turn (OBJHEAD1), and its shape chunk (the REBSHAPE whose SHPHEAD1 holds the object's shape number)
		struct ObjectShape
		{
			int32_t place[3];
			float turn[4];
			Span shape;
		};

		HierarchyEditResult FindObjectShape(Image const &image, Span const &root, std::string const &objectName, ObjectShape *out)
		{
			std::vector<Span> top;

			if (!Children(image, root, &top))
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			int32_t shapeNumber = 0;
			bool haveObject = false;

			for (Span const &child : top)
			{
				if (!NameIs(image, child.at, "RBOBJECT") || haveObject)
				{
					continue;
				}

				Span head;
				bool walkable = false;

				if (!FirstChild(image, child, "OBJHEAD1", &head, &walkable) || head.size < kHeadingBytes + 61)
				{
					continue;
				}

				char const *const first = image.data() + head.at + kHeadingBytes + 60;
				char const *const zero = (char const *)memchr(first, 0, head.at + head.size - (head.at + kHeadingBytes + 60));

				if (zero && std::string(first, (size_t)(zero - first)) == objectName)
				{
					memcpy(out->place, image.data() + head.at + kHeadingBytes + 20, 12);
					memcpy(out->turn, image.data() + head.at + kHeadingBytes + 32, 16);
					shapeNumber = IntAt(image, head.at + kHeadingBytes + 56);
					haveObject = true;
				}
			}

			if (!haveObject)
			{
				return HierarchyEditResult::NoHierarchy;
			}

			for (Span const &child : top)
			{
				if (!NameIs(image, child.at, "REBSHAPE"))
				{
					continue;
				}

				Span head;
				bool walkable = false;

				if (FirstChild(image, child, "SHPHEAD1", &head, &walkable) && head.size >= kHeadingBytes + 68 && IntAt(image, head.at + kHeadingBytes + 20) == shapeNumber)
				{
					out->shape = child;
					return HierarchyEditResult::Done;
				}
			}

			return HierarchyEditResult::NoSequence;
		}

		struct SHPBONHDAt
		{
			Span span;
			int32_t number, subNumber, points;
			std::string hierarchy;
			std::vector<std::pair<std::string, std::vector<int32_t>>> bones;
			std::vector<std::vector<std::array<int32_t, 3>>> offsets;   // each point's +4..+0xc, in the bones' order
		};

		// Every SHPBONHD child of the shape chunk, read
		bool BoneChunks(Image const &image, Span const &shape, std::vector<SHPBONHDAt> *out)
		{
			std::vector<Span> inside;

			out->clear();

			if (!Children(image, shape, &inside))
			{
				return false;
			}

			for (Span const &child : inside)
			{
				if (!NameIs(image, child.at, "SHPBONHD"))
				{
					continue;
				}

				SHPBONHDAt made;
				std::vector<Span> parts;
				bool hasInfo = false;

				made.span = child;
				made.number = made.subNumber = made.points = 0;

				if (!Children(image, child, &parts))
				{
					return false;
				}

				for (Span const &part : parts)
				{
					if (NameIs(image, part.at, "SHPBONIN") && !hasInfo)
					{
						if (part.size < kHeadingBytes + 13)
						{
							return false;
						}

						made.number = IntAt(image, part.at + kHeadingBytes);
						made.subNumber = IntAt(image, part.at + kHeadingBytes + 4);
						made.points = IntAt(image, part.at + kHeadingBytes + 8);

						char const *const first = image.data() + part.at + kHeadingBytes + 12;
						char const *const zero = (char const *)memchr(first, 0, part.at + part.size - (part.at + kHeadingBytes + 12));

						if (!zero)
						{
							return false;
						}

						made.hierarchy.assign(first, (size_t)(zero - first));
						hasInfo = true;
					}
					else if (NameIs(image, part.at, "SHPBONE"))
					{
						std::vector<Span> data;

						if (!Children(image, part, &data))
						{
							return false;
						}

						for (Span const &record : data)
						{
							if (!NameIs(image, record.at, "SHPBONDT") || record.size < kHeadingBytes + 8)
							{
								continue;
							}

							char const *const first = image.data() + record.at + kHeadingBytes;
							char const *const zero = (char const *)memchr(first, 0, record.at + record.size - (record.at + kHeadingBytes));

							if (!zero)
							{
								return false;
							}

							size_t const padded = ((size_t)(zero - first) + 4) & ~(size_t)3;
							size_t at = record.at + kHeadingBytes + padded;

							if (at + 4 > record.at + record.size)
							{
								return false;
							}

							int32_t const count = IntAt(image, at);

							at += 4;

							if (count < 0 || (size_t)count > (record.at + record.size - at) / 20)
							{
								return false;
							}

							std::pair<std::string, std::vector<int32_t>> bone;
							std::vector<std::array<int32_t, 3>> places;

							bone.first.assign(first, (size_t)(zero - first));

							for (int32_t i = 0; i < count; i++)
							{
								size_t const one = at + (size_t)i * 20;
								std::array<int32_t, 3> const offset = { IntAt(image, one + 4), IntAt(image, one + 8), IntAt(image, one + 12) };

								bone.second.push_back(IntAt(image, one));
								places.push_back(offset);
							}

							made.bones.push_back(bone);
							made.offsets.push_back(places);
						}
					}
				}

				if (!hasInfo)
				{
					return false;
				}

				out->push_back(made);
			}

			return true;
		}

		void PutText(Image *chunk, std::string const &text)
		{
			size_t const padded = (text.size() + 4) & ~(size_t)3;
			size_t const at = chunk->size();

			chunk->resize(at + padded, 0);
			memcpy(chunk->data() + at, text.data(), text.size());
		}

		void PutInt(Image *chunk, int32_t value)
		{
			size_t const at = chunk->size();

			chunk->resize(at + 4);
			memcpy(chunk->data() + at, &value, 4);
		}

		void FinishChunk(Image *chunk)
		{
			PutIntAt(chunk, 8, (int32_t)chunk->size());
		}

		Image ChunkStart(char const *name)
		{
			Image chunk(kHeadingBytes, 0);

			memcpy(chunk.data(), name, 8);
			return chunk;
		}

		// SHPBONHD: SHPBONIN, then one SHPBONE (holding one SHPBONDT) for each bone that got points
		// offsets (when not null) holds each assigned point's place relative to its bone, in assigned's order (BonePointOffsets); none, or a missing one: 0, 0, 0
		Image BoneHeader(int32_t number, int32_t subNumber, int32_t pointCount, std::string const &hierarchy, std::vector<std::pair<std::string, std::vector<int32_t>>> const &assigned,
			std::vector<std::vector<std::array<int32_t, 3>>> const *offsets = nullptr)
		{
			Image header = ChunkStart("SHPBONHD");
			Image info = ChunkStart("SHPBONIN");

			PutInt(&info, number);
			PutInt(&info, subNumber);
			PutInt(&info, pointCount);
			PutText(&info, hierarchy);
			FinishChunk(&info);
			header.insert(header.end(), info.begin(), info.end());

			for (size_t b = 0; b < assigned.size(); b++)
			{
				auto const &bone = assigned[b];
				Image data = ChunkStart("SHPBONDT");

				PutText(&data, bone.first);
				PutInt(&data, (int32_t)bone.second.size());

				for (size_t k = 0; k < bone.second.size(); k++)
				{
					bool const has = offsets && b < offsets->size() && k < (*offsets)[b].size();

					PutInt(&data, bone.second[k]);
					PutInt(&data, has ? (*offsets)[b][k][0] : 0);
					PutInt(&data, has ? (*offsets)[b][k][1] : 0);
					PutInt(&data, has ? (*offsets)[b][k][2] : 0);
					PutInt(&data, 0x3f800000);
				}

				PutInt(&data, 0);
				PutInt(&data, 0);
				FinishChunk(&data);

				Image holder = ChunkStart("SHPBONE");

				holder.insert(holder.end(), data.begin(), data.end());
				FinishChunk(&holder);
				header.insert(header.end(), holder.begin(), holder.end());
			}

			FinishChunk(&header);
			return header;
		}
	}

	HierarchyEditResult HierarchyEdit_ReadBoneAttachment(OpenLevelFile *file, std::string const &objectName, std::string const &hierarchy, BoneAttachment *out)
	{
		Image image;
		Span root;

		if (!out)
		{
			return HierarchyEditResult::BadCall;
		}

		*out = BoneAttachment();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		ObjectShape object;
		HierarchyEditResult const found = FindObjectShape(image, root, objectName, &object);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		std::vector<SHPBONHDAt> all;

		if (!BoneChunks(image, object.shape, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (SHPBONHDAt const &each : all)
		{
			if (each.hierarchy == hierarchy)
			{
				out->present = true;
				out->number = each.number;
				out->subNumber = each.subNumber;
				out->points = each.points;
				out->bones = each.bones;
				out->offsets = each.offsets;
				break;
			}
		}

		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_AttachToBones(OpenLevelFile *file, std::string const &objectName, std::string const &hierarchy, int32_t number, int32_t subNumber, bool redetermine, char const **why,
		size_t *attachedPoints)
	{
		Image image;
		Span root;

		Why(why, nullptr);

		if (attachedPoints)
		{
			*attachedPoints = 0;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		ObjectShape object;
		HierarchyEditResult const found = FindObjectShape(image, root, objectName, &object);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		// the shape's points (SHPRAWVT: three whole numbers each)
		Span vertices;
		bool walkable = false;

		if (!FirstChild(image, object.shape, "SHPRAWVT", &vertices, &walkable))
		{
			Why(why, "the object's shape has no points");
			return walkable ? HierarchyEditResult::Refused : HierarchyEditResult::BadLevelBytes;
		}

		size_t const pointCount = (vertices.size - kHeadingBytes) / 12;

		// an attachment to this hierarchy already there: inteng asks before it redetermines (same point count), and then clears the old one
		std::vector<SHPBONHDAt> existing;

		if (!BoneChunks(image, object.shape, &existing))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		Image work = image;
		Span shape = object.shape;
		std::vector<size_t> const aboveShape = { root.at };

		for (SHPBONHDAt const &each : existing)
		{
			if (each.hierarchy != hierarchy)
			{
				continue;
			}

			if (!redetermine && (size_t)each.points == pointCount)
			{
				Why(why, "The shape is already attached to the hierarchy");
				return HierarchyEditResult::Refused;
			}

			Image removed;

			if (!Spliced(work, { root.at, shape.at }, each.span.at, each.span.size, Image(), &removed))
			{
				return HierarchyEditResult::CantBuild;
			}

			work = removed;
			shape.size -= each.span.size;
			root.size -= each.span.size;
			break;
		}

		// the hierarchy's sections, from its root section, in the reference pose
		Span section;
		HierarchyEditResult const rooted = RootSection(work, root, hierarchy, &section);

		if (rooted != HierarchyEditResult::Done)
		{
			return rooted;
		}

		Matrix3 identity = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		int32_t const origin[3] = { 0, 0, 0 };
		std::vector<Bone> bones;
		ReferenceFrame rootFrame;

		if (!MakeBones(work, section, number, subNumber, identity, origin, &bones, &rootFrame, 0) || bones.empty())
		{
			Why(why, "The hierarchy doesn't have the reference sequence");
			return HierarchyEditResult::Refused;
		}

		// each point to its nearest bone
		Matrix3 objectMatrix;

		QuatToMatrix(object.turn, objectMatrix);

		std::vector<std::pair<std::string, std::vector<int32_t>>> assigned;
		size_t connected = 0;
		size_t const verticesAt = vertices.at + kHeadingBytes;

		for (size_t i = 0; i < pointCount; i++)
		{
			int32_t point[3];

			memcpy(point, image.data() + verticesAt + i * 12, 12);

			std::string name;

			if (!NearestBone(bones, objectMatrix, object.place, point, &name))
			{
				continue;
			}

			size_t slot = 0;

			while (slot < assigned.size() && assigned[slot].first != name)
			{
				slot++;
			}

			if (slot == assigned.size())
			{
				assigned.push_back(std::make_pair(name, std::vector<int32_t>()));
			}

			assigned[slot].second.push_back((int32_t)i);
			connected++;
		}

		// TB-3: each point's place relative to its bone (FUN_0041eae0 at 0x41f804)
		std::vector<std::vector<std::array<int32_t, 3>>> offsets;

		BonePointOffsets(bones, objectMatrix, object.place, image, verticesAt, pointCount, assigned, &offsets);

		Image const header = BoneHeader(number, subNumber, (int32_t)pointCount, hierarchy, assigned, &offsets);

		Image out;

		if (!Spliced(work, { root.at, shape.at }, shape.at + shape.size, 0, header, &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		HierarchyEditResult const done = Replace(file, out);

		if (done == HierarchyEditResult::Done && attachedPoints)
		{
			*attachedPoints = connected;
		}

		return done;
	}

	HierarchyEditResult HierarchyEdit_SkinPoints(OpenLevelFile *file, std::string const &objectName, std::vector<int32_t> *points)
	{
		Image image;
		Span root;

		if (!points)
		{
			return HierarchyEditResult::BadCall;
		}

		points->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		ObjectShape object;
		HierarchyEditResult const found = FindObjectShape(image, root, objectName, &object);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		Span vertices;
		bool walkable = false;

		if (!FirstChild(image, object.shape, "SHPRAWVT", &vertices, &walkable))
		{
			return walkable ? HierarchyEditResult::Refused : HierarchyEditResult::BadLevelBytes;
		}

		Matrix3 objectMatrix;

		QuatToMatrix(object.turn, objectMatrix);

		size_t const pointCount = (vertices.size - kHeadingBytes) / 12;

		for (size_t i = 0; i < pointCount; i++)
		{
			int32_t point[3];

			memcpy(point, image.data() + vertices.at + kHeadingBytes + i * 12, 12);
			RotateVector(point, objectMatrix);

			for (int axis = 0; axis < 3; axis++)
			{
				points->push_back(point[axis] + object.place[axis]);
			}
		}

		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_SkinFaces(OpenLevelFile *file, std::string const &objectName, std::vector<int32_t> *faces)
	{
		Image image;
		Span root;

		if (!faces)
		{
			return HierarchyEditResult::BadCall;
		}

		faces->clear();

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		ObjectShape object;
		HierarchyEditResult const found = FindObjectShape(image, root, objectName, &object);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		Span vertices, polygons;
		bool walkable = false;

		if (!FirstChild(image, object.shape, "SHPRAWVT", &vertices, &walkable) || !FirstChild(image, object.shape, "SHPPOLYS", &polygons, &walkable))
		{
			return walkable ? HierarchyEditResult::Refused : HierarchyEditResult::BadLevelBytes;
		}

		// SHPPOLYS (chunk_shape.cpp Shape_Polygon_Chunk::fill_data_block): 36 bytes a polygon: engine type, normal, flags, colour, then five point numbers ended by -1. HOSTILE: a number that is
		// no point of the shape ends the polygon there; a polygon of fewer than three is left out.
		int64_t const pointCount = (int64_t)((vertices.size - kHeadingBytes) / 12);
		size_t const polygonCount = (polygons.size - kHeadingBytes) / 36;

		for (size_t p = 0; p < polygonCount; p++)
		{
			int32_t corner[5];
			int used = 0;

			memcpy(corner, image.data() + polygons.at + kHeadingBytes + p * 36 + 16, sizeof(corner));

			while (used < 4 && corner[used] >= 0 && corner[used] < pointCount)
			{
				used++;
			}

			if (used >= 3)
			{
				faces->push_back(corner[0]);
				faces->push_back(corner[1]);
				faces->push_back(corner[2]);
				faces->push_back(used == 4 ? corner[3] : -1);
			}
		}

		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_MovePointsToBone(OpenLevelFile *file, std::string const &objectName, std::string const &hierarchy, std::string const &bone, std::vector<int32_t> const &points, char const **why, size_t *moved)
	{
		Image image;
		Span root;

		Why(why, nullptr);

		if (moved)
		{
			*moved = 0;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		ObjectShape object;
		HierarchyEditResult const found = FindObjectShape(image, root, objectName, &object);

		if (found != HierarchyEditResult::Done)
		{
			return found;
		}

		std::vector<SHPBONHDAt> existing;

		if (!BoneChunks(image, object.shape, &existing))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		SHPBONHDAt const *mine = nullptr;

		for (SHPBONHDAt const &each : existing)
		{
			if (each.hierarchy == hierarchy)
			{
				mine = &each;
				break;
			}
		}

		if (!mine)
		{
			Why(why, "The shape is not attached to the hierarchy: use Attach shape to bones first");
			return HierarchyEditResult::Refused;
		}

		BoneAttachment current;
		HierarchyEditResult const read = HierarchyEdit_ReadBoneAttachment(file, objectName, hierarchy, &current);

		if (read != HierarchyEditResult::Done || !current.present)
		{
			return read != HierarchyEditResult::Done ? read : HierarchyEditResult::Refused;
		}

		// the bone has to be a section of the hierarchy (its object name)
		std::vector<std::string> sections;
		HierarchyEditResult const named = HierarchyEdit_SectionObjects(file, hierarchy, &sections);

		if (named != HierarchyEditResult::Done)
		{
			return named;
		}

		bool isSection = false;

		for (std::string const &section : sections)
		{
			isSection = isSection || section == bone;
		}

		for (int32_t const point : points)
		{
			if (point < 0 || point >= current.points)
			{
				return HierarchyEditResult::BadCall;
			}
		}

		if (!isSection || points.empty())
		{
			return HierarchyEditResult::BadCall;
		}

		// every point leaves the bone that holds it and goes to the end of the chosen bone's points (a bone of that name is made when there is none)
		size_t slot = 0;

		while (slot < current.bones.size() && current.bones[slot].first != bone)
		{
			slot++;
		}

		if (slot == current.bones.size())
		{
			current.bones.push_back(std::make_pair(bone, std::vector<int32_t>()));
		}

		// each point's offset beside it (TB-3): a point that moves takes its record's offset with it
		current.offsets.resize(current.bones.size());

		for (size_t b = 0; b < current.bones.size(); b++)
		{
			current.offsets[b].resize(current.bones[b].second.size(), std::array<int32_t, 3>{ { 0, 0, 0 } });
		}

		size_t changed = 0;

		for (int32_t const point : points)
		{
			bool already = false;
			std::array<int32_t, 3> carried = { { 0, 0, 0 } };

			for (size_t b = 0; b < current.bones.size(); b++)
			{
				std::vector<int32_t> &list = current.bones[b].second;

				for (size_t i = 0; i < list.size(); i++)
				{
					if (list[i] == point)
					{
						if (b == slot)
						{
							already = true;
						}
						else
						{
							carried = current.offsets[b][i];
							list.erase(list.begin() + (std::ptrdiff_t)i);
							current.offsets[b].erase(current.offsets[b].begin() + (std::ptrdiff_t)i);
						}

						break;
					}
				}
			}

			if (!already)
			{
				current.bones[slot].second.push_back(point);
				current.offsets[slot].push_back(carried);
				changed++;
			}
		}

		// TB-3, FUN_0041eae0 at 0x47ad94: every bone's points worked out again in the reference pose (number, sub from SHPBONIN); a bone the pose doesn't have, or no reference sequence at
		// all, keeps its offsets as they were
		{
			Span section, vertices;
			bool walkable = false;
			Matrix3 identity = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
			int32_t const origin[3] = { 0, 0, 0 };
			std::vector<Bone> bones;
			ReferenceFrame rootFrame;

			if (RootSection(image, root, hierarchy, &section) == HierarchyEditResult::Done && MakeBones(image, section, current.number, current.subNumber, identity, origin, &bones, &rootFrame, 0) &&
				FirstChild(image, object.shape, "SHPRAWVT", &vertices, &walkable) && vertices.size >= kHeadingBytes)
			{
				Matrix3 objectMatrix;

				QuatToMatrix(object.turn, objectMatrix);
				BonePointOffsets(bones, objectMatrix, object.place, image, vertices.at + kHeadingBytes, (vertices.size - kHeadingBytes) / 12, current.bones, &current.offsets);
			}
		}

		Image const header = BoneHeader(current.number, current.subNumber, current.points, hierarchy, current.bones, &current.offsets);
		Image out;

		if (!Spliced(image, { root.at, object.shape.at }, mine->span.at, mine->span.size, header, &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		HierarchyEditResult const done = Replace(file, out);

		if (done == HierarchyEditResult::Done && moved)
		{
			*moved = changed;
		}

		return done;
	}

	namespace
	{
		bool RenumberIn(Image *image, Span const &holder, int32_t number, int32_t subNumber, int32_t newNumber, int32_t newSubNumber, size_t *changed, bool *taken, int depth)
		{
			std::vector<Span> inside;

			if (depth > 64 || !Children(*image, holder, &inside))
			{
				return false;
			}

			for (Span const &child : inside)
			{
				if (NameIs(*image, child.at, "OBASEQHD") && child.size >= kHeadingBytes + 12)
				{
					int32_t const a = IntAt(*image, child.at + kHeadingBytes + 4);
					int32_t const b = IntAt(*image, child.at + kHeadingBytes + 8);

					*taken = *taken || (a == newNumber && b == newSubNumber);

					if (a == number && b == subNumber)
					{
						PutIntAt(image, child.at + kHeadingBytes + 4, newNumber);
						PutIntAt(image, child.at + kHeadingBytes + 8, newSubNumber);
						(*changed)++;
					}
				}
				else if (NameIs(*image, child.at, "OBANALLS"))
				{
					size_t at = child.at + kHeadingBytes;
					size_t const end = child.at + child.size;

					if (end - at < 4)
					{
						return false;
					}

					int32_t const count = IntAt(*image, at);

					at += 4;

					for (int32_t i = 0; i < count; i++)
					{
						if (end - at < 16)
						{
							return false;
						}

						int32_t const frames = IntAt(*image, at);
						int32_t const a = IntAt(*image, at + 4);
						int32_t const b = IntAt(*image, at + 8);

						*taken = *taken || (a == newNumber && b == newSubNumber);

						if (a == number && b == subNumber)
						{
							PutIntAt(image, at + 4, newNumber);
							PutIntAt(image, at + 8, newSubNumber);
							(*changed)++;
						}

						at += 16;

						if (frames < 0 || (size_t)frames > (end - at) / 36)
						{
							return false;
						}

						at += (size_t)frames * 36;
					}
				}
				else if ((NameIs(*image, child.at, "OBJCHIER") || NameIs(*image, child.at, "OBANSEQS") || NameIs(*image, child.at, "OBANSEQC")) &&
					!RenumberIn(image, child, number, subNumber, newNumber, newSubNumber, changed, taken, depth + 1))
				{
					return false;
				}
			}

			return true;
		}
	}

	HierarchyEditResult HierarchyEdit_RenumberSequence(OpenLevelFile *file, std::string const &hierarchy, int32_t number, int32_t subNumber, int32_t newNumber, int32_t newSubNumber, size_t *changed)
	{
		Image image;
		Span root;

		if (!changed)
		{
			return HierarchyEditResult::BadCall;
		}

		*changed = 0;

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		std::vector<HierarchyAt> all;

		if (!Hierarchies(image, root, &all))
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (HierarchyAt const &each : all)
		{
			if (each.name != hierarchy)
			{
				continue;
			}

			Image out = image;
			bool taken = false;

			if (!RenumberIn(&out, each.span, number, subNumber, newNumber, newSubNumber, changed, &taken, 0))
			{
				return HierarchyEditResult::BadLevelBytes;
			}

			if (taken)
			{
				*changed = 0;
				return HierarchyEditResult::Refused;
			}

			return *changed ? Replace(file, out) : HierarchyEditResult::NoSequence;
		}

		return HierarchyEditResult::NoHierarchy;
	}

	HierarchyEditResult HierarchyEdit_ReadDegradation(OpenLevelFile *file, int32_t distances[10], bool *present)
	{
		Image image;
		Span root;

		if (!distances || !present)
		{
			return HierarchyEditResult::BadCall;
		}

		*present = false;

		for (int i = 0; i < 10; i++)
		{
			distances[i] = i * 10000;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		Span chunk;
		bool walkable = false;

		if (!FirstChild(image, root, "HIDEGDIS", &chunk, &walkable))
		{
			return walkable ? HierarchyEditResult::Done : HierarchyEditResult::BadLevelBytes;
		}

		if (chunk.size != kHeadingBytes + 4 + 10 * 4 || IntAt(image, chunk.at + kHeadingBytes) != 10)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		for (int i = 0; i < 10; i++)
		{
			distances[i] = IntAt(image, chunk.at + kHeadingBytes + 4 + (size_t)i * 4);
		}

		*present = true;
		return HierarchyEditResult::Done;
	}

	HierarchyEditResult HierarchyEdit_WriteDegradation(OpenLevelFile *file, int32_t const distances[10])
	{
		Image image;
		Span root;

		if (!distances)
		{
			return HierarchyEditResult::BadCall;
		}

		HierarchyEditResult const opened = Open(file, &image, &root);

		if (opened != HierarchyEditResult::Done)
		{
			return opened;
		}

		Image chunk(kHeadingBytes + 4 + 10 * 4, 0);

		memcpy(chunk.data(), "HIDEGDIS", 8);
		PutIntAt(&chunk, 8, (int32_t)chunk.size());
		PutIntAt(&chunk, kHeadingBytes, 10);

		for (int i = 0; i < 10; i++)
		{
			PutIntAt(&chunk, kHeadingBytes + 4 + (size_t)i * 4, distances[i]);
		}

		Span old;
		bool walkable = false;
		bool const had = FirstChild(image, root, "HIDEGDIS", &old, &walkable);

		if (!had && !walkable)
		{
			return HierarchyEditResult::BadLevelBytes;
		}

		Image out;
		std::vector<size_t> const ancestors = { root.at };

		if (!Spliced(image, ancestors, had ? old.at : image.size(), had ? old.size : 0, chunk, &out))
		{
			return HierarchyEditResult::CantBuild;
		}

		return Replace(file, out);
	}
}
