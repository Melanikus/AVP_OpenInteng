// See hierarchy_collections.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "hierarchy_collections.h"

#include <string.h>

namespace model
{
	namespace
	{
		int32_t Int(uint8_t const *at)
		{
			int32_t value;

			memcpy(&value, at, sizeof(value));
			return value;
		}

		void PutInt(std::vector<uint8_t> *out, int32_t value)
		{
			uint8_t bytes[4];

			memcpy(bytes, &value, sizeof(value));
			out->insert(out->end(), bytes, bytes + 4);
		}

		char Fold(char c)
		{
			return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
		}

		// the name's room: its length + 1 rounded up to a multiple of 4, (length + 4) & ~3 (hierarchy_chunk.cpp:524, 553)
		size_t NameRoom(size_t length)
		{
			return (length + 4) & ~(size_t)3;
		}

		// a name a command takes: not empty, no other collection (not the one `except`) has it
		CollectionResult NameIsUsable(std::vector<ShapeSetCollection> const &list, std::string const &name, int32_t except)
		{
			if (name.empty() || name.size() > kMostCollectionNameBytes || name.find('\0') != std::string::npos)
			{
				return CollectionResult::BadName;
			}

			for (ShapeSetCollection const &other : list)
			{
				if (other.number != except && CollectionNamesEqual(other.name, name))
				{
					return CollectionResult::NameInUse;
				}
			}

			return CollectionResult::Ok;
		}
	}

	char const *CollectionWords(CollectionResult result)
	{
		switch (result)
		{
		case CollectionResult::Ok: return "done";
		case CollectionResult::BadCall: return "nothing to read or change";
		case CollectionResult::TooShort: return "the shape set collection ends too soon";
		case CollectionResult::BadNumber: return "a shape set collection number is outside 0 to 4095";
		case CollectionResult::BadName: return "a shape set collection name is empty, too long or has no end";
		case CollectionResult::BadCount: return "a shape set collection says more shape sets than its bytes hold";
		case CollectionResult::NameInUse: return "that name is already used";
		case CollectionResult::NoSuchCollection: return "there is no such shape set collection";
		case CollectionResult::WrongSize: return "the shape set collection would not be the size of its bytes";
		case CollectionResult::NoSuchShapeSet: return "there is no such alternate shape set";
		case CollectionResult::AlreadyInCollection: return "the collection already has that shape set";
		case CollectionResult::EmptyCollection: return "This shape collection contains no alternate shape sets";
		case CollectionResult::NotInCollection: return "the collection does not have that shape set";
		}

		return "unknown answer";
	}

	bool CollectionNamesEqual(std::string const &a, std::string const &b)
	{
		if (a.size() != b.size())
		{
			return false;
		}

		for (size_t i = 0; i < a.size(); i++)
		{
			if (Fold(a[i]) != Fold(b[i]))
			{
				return false;
			}
		}

		return true;
	}

	CollectionResult ReadCollection(uint8_t const *payload, size_t size, ShapeSetCollection *out)
	{
		if (!out || (!payload && size))
		{
			return CollectionResult::BadCall;
		}

		if (size < 4)
		{
			return CollectionResult::TooShort;
		}

		ShapeSetCollection read = {};

		read.number = Int(payload);

		if (read.number < 0 || read.number > kMostCollectionNumber)
		{
			return CollectionResult::BadNumber;
		}

		size_t at = 4;
		size_t length = 0;

		while (at + length < size && payload[at + length] != 0)
		{
			length++;
		}

		if (at + length >= size || length > kMostCollectionNameBytes)
		{
			return CollectionResult::BadName;
		}

		read.name.assign((char const *)payload + at, length);
		at += NameRoom(length);

		if (at > size || size - at < 4)
		{
			return CollectionResult::TooShort;
		}

		int32_t const count = Int(payload + at);

		at += 4;

		if (count < 0 || (size_t)count > kMostCollectionSets || (size - at) / 4 < (size_t)count)
		{
			return CollectionResult::BadCount;
		}

		for (int32_t i = 0; i < count; i++)
		{
			read.shapeSets.push_back(Int(payload + at));
			at += 4;
		}

		if (size - at < 8)
		{
			return CollectionResult::TooShort;
		}

		read.voiceIndex = Int(payload + at);
		read.flags = Int(payload + at + 4);
		*out = read;
		return CollectionResult::Ok;
	}

	CollectionResult CollectionBytes(ShapeSetCollection const &collection, std::vector<uint8_t> *payload)
	{
		if (!payload)
		{
			return CollectionResult::BadCall;
		}

		if (collection.number < 0 || collection.number > kMostCollectionNumber)
		{
			return CollectionResult::BadNumber;
		}

		if (collection.name.size() > kMostCollectionNameBytes || collection.name.find('\0') != std::string::npos)
		{
			return CollectionResult::BadName;
		}

		if (collection.shapeSets.size() > kMostCollectionSets)
		{
			return CollectionResult::BadCount;
		}

		std::vector<uint8_t> bytes;

		PutInt(&bytes, collection.number);
		bytes.insert(bytes.end(), collection.name.begin(), collection.name.end());
		bytes.resize(4 + NameRoom(collection.name.size()), 0);   // the end byte and the padding
		PutInt(&bytes, (int32_t)collection.shapeSets.size());

		for (int32_t set : collection.shapeSets)
		{
			PutInt(&bytes, set);
		}

		PutInt(&bytes, collection.voiceIndex);
		PutInt(&bytes, collection.flags);
		payload->swap(bytes);
		return CollectionResult::Ok;
	}

	CollectionResult CreateCollection(std::vector<ShapeSetCollection> const &list, std::string const &name, ShapeSetCollection *created)
	{
		if (!created)
		{
			return CollectionResult::BadCall;
		}

		CollectionResult const usable = NameIsUsable(list, name, -1);

		if (usable != CollectionResult::Ok)
		{
			return usable;
		}

		// the smallest number from 2 up that is not used (the loop of 0x48b051: on a match the number goes up by one and the list is looked through again)
		int32_t number = 2;
		bool again = true;

		while (again)
		{
			again = false;

			for (ShapeSetCollection const &other : list)
			{
				if (other.number == number)
				{
					number++;
					again = true;
					break;
				}
			}
		}

		if (number > kMostCollectionNumber)
		{
			return CollectionResult::BadNumber;
		}

		ShapeSetCollection made = {};

		made.number = number;
		made.name = name;
		made.voiceIndex = 0;
		made.flags = 0;
		*created = made;
		return CollectionResult::Ok;
	}

	CollectionResult RenameCollection(std::vector<ShapeSetCollection> *list, int32_t number, std::string const &newName)
	{
		if (!list)
		{
			return CollectionResult::BadCall;
		}

		for (ShapeSetCollection &c : *list)
		{
			if (c.number == number)
			{
				CollectionResult const usable = NameIsUsable(*list, newName, number);

				if (usable != CollectionResult::Ok)
				{
					return usable;
				}

				c.name = newName;
				return CollectionResult::Ok;
			}
		}

		return CollectionResult::NoSuchCollection;
	}

	CollectionResult DeleteCollection(std::vector<ShapeSetCollection> *list, int32_t number)
	{
		if (!list)
		{
			return CollectionResult::BadCall;
		}

		for (size_t i = 0; i < list->size(); i++)
		{
			if ((*list)[i].number == number)
			{
				list->erase(list->begin() + (ptrdiff_t)i);
				return CollectionResult::Ok;
			}
		}

		return CollectionResult::NoSuchCollection;
	}

	CollectionResult SetCollectionVoiceIndex(std::vector<ShapeSetCollection> *list, int32_t number, int32_t voiceIndex)
	{
		if (!list)
		{
			return CollectionResult::BadCall;
		}

		for (ShapeSetCollection &c : *list)
		{
			if (c.number == number)
			{
				c.voiceIndex = voiceIndex;
				return CollectionResult::Ok;
			}
		}

		return CollectionResult::NoSuchCollection;
	}

	namespace
	{
		bool Has(ShapeSetCollection const &collection, int32_t number)
		{
			for (int32_t const each : collection.shapeSets)
			{
				if (each == number)
				{
					return true;
				}
			}

			return false;
		}
	}

	CollectionResult OfferedShapeSets(std::vector<AlternateSetInfo> const &alternates, ShapeSetCollection const &collection, std::vector<AlternateSetInfo> *offered)
	{
		if (!offered)
		{
			return CollectionResult::BadCall;
		}

		offered->clear();

		for (AlternateSetInfo const &alternate : alternates)
		{
			if (!Has(collection, alternate.number))
			{
				offered->push_back(alternate);
			}
		}

		return CollectionResult::Ok;
	}

	CollectionResult AddShapeSet(std::vector<ShapeSetCollection> *list, int32_t collectionNumber, int32_t setNumber, std::vector<AlternateSetInfo> const &alternates)
	{
		if (!list)
		{
			return CollectionResult::BadCall;
		}

		for (ShapeSetCollection &c : *list)
		{
			if (c.number != collectionNumber)
			{
				continue;
			}

			bool exists = false;

			for (AlternateSetInfo const &alternate : alternates)
			{
				exists = exists || alternate.number == setNumber;
			}

			if (!exists)
			{
				return CollectionResult::NoSuchShapeSet;
			}

			if (Has(c, setNumber))
			{
				return CollectionResult::AlreadyInCollection;
			}

			if (c.shapeSets.size() >= kMostCollectionSets)
			{
				return CollectionResult::BadCount;
			}

			c.shapeSets.push_back(setNumber);
			return CollectionResult::Ok;
		}

		return CollectionResult::NoSuchCollection;
	}

	CollectionResult RemoveShapeSet(std::vector<ShapeSetCollection> *list, int32_t collectionNumber, int32_t setNumber)
	{
		if (!list)
		{
			return CollectionResult::BadCall;
		}

		for (ShapeSetCollection &c : *list)
		{
			if (c.number != collectionNumber)
			{
				continue;
			}

			if (c.shapeSets.empty())
			{
				return CollectionResult::EmptyCollection;
			}

			for (size_t i = 0; i < c.shapeSets.size(); i++)
			{
				if (c.shapeSets[i] == setNumber)
				{
					c.shapeSets.erase(c.shapeSets.begin() + (ptrdiff_t)i);
					return CollectionResult::Ok;
				}
			}

			return CollectionResult::NotInCollection;
		}

		return CollectionResult::NoSuchCollection;
	}
}
