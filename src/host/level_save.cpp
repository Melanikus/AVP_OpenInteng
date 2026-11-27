// level_save.cpp: Save over the opened file with a backup (level_save.h says the rule and the order). Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "level_save.h"

#include <ctype.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk_object.hpp"   // Object_Chunk, Object_Header_Chunk and the header's flag bits (OBJECT_FLAG_*)
#include "level_file.h"
#include "mishchnk.hpp"       // File_Chunk::list_objects

namespace host
{
	namespace
	{
		bool EndsWithRif(std::string const &path)
		{
			return path.size() > 4 && _stricmp(path.c_str() + path.size() - 4, ".rif") == 0;
		}

		std::string Reason(char const *what, std::string const &path)
		{
			DWORD const code = GetLastError();

			return std::string(what) + ": " + path + (code ? " (Windows error " + std::to_string((unsigned long)code) + ")" : std::string());
		}

		// The target is one the editor writes over: a .rif with room for ".saving" and ".bak" in its path, not a folder, not read-only. *exists: a file is there now.
		LevelSaveOver TargetIsUsable(OpenLevelFile *open, std::string const &target, bool *exists, std::string *why)
		{
			if (!open || !open->Level() || target.empty() || !EndsWithRif(target) || target.size() + 16 >= MAX_PATH)
			{
				*why = "no level, or the file to save over is not a .rif: " + target;
				return LevelSaveOver::BadTarget;
			}

			DWORD const attributes = GetFileAttributesA(target.c_str());

			*exists = attributes != INVALID_FILE_ATTRIBUTES;

			if (*exists && (attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				*why = "the level's path is a folder: " + target;
				return LevelSaveOver::BadTarget;
			}

			if (*exists && (attributes & FILE_ATTRIBUTE_READONLY))
			{
				*why = "the file is read-only: " + target;
				return LevelSaveOver::ReadOnly;
			}

			return LevelSaveOver::Saved;
		}

		// Bytes written to a file that must not be there yet (CREATE_NEW: a file or a link of that name is never written through), flushed to the disk
		bool WriteNewFile(std::string const &path, std::vector<char> const &bytes)
		{
			HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			bool good = true;
			size_t done = 0;

			while (good && done < bytes.size())
			{
				DWORD const want = (DWORD)(bytes.size() - done < (1u << 24) ? bytes.size() - done : (1u << 24));
				DWORD wrote = 0;

				good = WriteFile(file, bytes.data() + done, want, &wrote, nullptr) && wrote == want;
				done += wrote;
			}

			good = FlushFileBuffers(file) && good;
			good = CloseHandle(file) && good;
			return good;
		}

		// The file to save over, checked as SaveAs checks the file it replaces (level_save.h, F-a): Saved, with *asItIs its bytes (none when nothing is there), or the refusal and why.
		// againstOpened: when it is the file the level stands for, it must be as it was then (a save's; the view copy's file is the editor's own, written over at every screen change).
		LevelSaveOver MayWriteOver(OpenLevelFile *open, std::string const &target, bool againstOpened, std::vector<char> *asItIs, std::string *why)
		{
			switch (open->MayWriteOver(target.c_str(), againstOpened, asItIs))
			{
			case LevelSaveResult::Saved:
				return LevelSaveOver::Saved;

			case LevelSaveResult::ChangedOnDisk:
				*why = "the level file was changed on disk by another program since it was opened or last saved, so it is not written over (it is as that program left it): " + target;
				return LevelSaveOver::ChangedOnDisk;

			case LevelSaveResult::NoPath:
				*why = "the file to save over can't be made a whole path, or names a device or a file's stream: " + target;
				return LevelSaveOver::BadTarget;

			default:
				*why = "the file to save over is not a plain file on a disk with one name only (a link, another reparse point, a device, or one of a hard link's names), so it is never "
				       "written over: " + target;
				return LevelSaveOver::NotAPlainFile;
			}
		}

		// Steps 2 and 3 of a write over the target (level_save.h): the temporary file read back through the game's own loader, a last look at the target (still exactly asItWas, the
		// bytes MayWriteOver read), then the temporary file put in place. On a failure the temporary file is removed and the target is as it was.
		LevelSaveOver PutInPlace(std::string const &target, std::string const &temporary, bool exists, bool keepTheOneBefore, std::vector<char> const &asItWas, std::string *backupPath,
		                         std::string *why)
		{
			std::string const backup = target + ".bak";

			// 2. read back through the game's own loader
			{
				OpenLevelFile check;

				if (check.Open(temporary.c_str()) != LevelOpenResult::Opened || !check.Level())
				{
					*why = "the level written did not load back through the game's loader: " + temporary;
					DeleteFileA(temporary.c_str());
					return LevelSaveOver::ReadBackFailed;
				}
			}

			// the last look: the file that was there is still exactly the one checked before the level was written, a plain file with one name (F-a). What can still happen after this
			// is another program swapping the file in the instant before the replace below; ReplaceFileA works by name, so that instant can't be closed here, and a program that can
			// write in the level's folder could change the level itself anyway
			if (exists && !LevelFile_StillThese(target.c_str(), asItWas))
			{
				*why = "the level file changed on disk, or stopped being a plain file with one name, while the save was being made, so it is not written over (it is as it is now): " + target;
				DeleteFileA(temporary.c_str());
				return LevelSaveOver::ChangedOnDisk;
			}

			// 3. into place: the file that was there becomes the .bak in the same step (the sitting's first save), or is replaced with any .bak left alone (a later one)
			if (exists && !keepTheOneBefore)
			{
				if (!ReplaceFileA(target.c_str(), temporary.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
				{
					*why = Reason("the level file could not be replaced (it is as it was)", target);
					DeleteFileA(temporary.c_str());
					return LevelSaveOver::ReplaceFailed;
				}
			}
			else if (exists)
			{
				if (!ReplaceFileA(target.c_str(), temporary.c_str(), backup.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
				{
					// an older .bak in the way: it goes, and the replace is tried once more
					DeleteFileA(backup.c_str());

					if (!ReplaceFileA(target.c_str(), temporary.c_str(), backup.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
					{
						*why = Reason("the level file could not be replaced (it is as it was)", target);
						DeleteFileA(temporary.c_str());
						return LevelSaveOver::ReplaceFailed;
					}
				}

				if (backupPath)
				{
					*backupPath = backup;
				}
			}
			else if (!MoveFileExA(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
			{
				*why = Reason("the level file could not be made", target);
				DeleteFileA(temporary.c_str());
				return LevelSaveOver::ReplaceFailed;
			}

			why->clear();
			return LevelSaveOver::Saved;
		}
	}

	char const *LevelSaveOverWords(LevelSaveOver result)
	{
		switch (result)
		{
		case LevelSaveOver::Saved: return "saved";
		case LevelSaveOver::BadTarget: return "there is no level file to save over";
		case LevelSaveOver::ReadOnly: return "the level file is read-only";
		case LevelSaveOver::WriteFailed: return "the level could not be written";
		case LevelSaveOver::ReadBackFailed: return "the level written did not load back";
		case LevelSaveOver::ReplaceFailed: return "the level file could not be replaced";
		case LevelSaveOver::NotAPlainFile: return "the level file is not a plain file with one name (a link, or a hard link's name)";
		case LevelSaveOver::ChangedOnDisk: return "the level file was changed on disk by another program";
		}

		return "not saved";
	}

	LevelSaveOver LevelSave_Over(OpenLevelFile *open, char const *targetPath, bool keepTheOneBefore, std::string *backupPath, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		if (backupPath)
		{
			backupPath->clear();
		}

		std::string const target = targetPath ? targetPath : "";
		bool exists = false;
		LevelSaveOver const usable = TargetIsUsable(open, target, &exists, why);

		if (usable != LevelSaveOver::Saved)
		{
			return usable;
		}

		// 0. the file to save over itself, as SaveAs would check the file it replaces (level_save.h, F-a)
		std::vector<char> asItWas;
		LevelSaveOver const mayWrite = MayWriteOver(open, target, true, &asItWas, why);

		if (mayWrite != LevelSaveOver::Saved)
		{
			return mayWrite;
		}

		// 1. the temporary file, in the target's own folder (so the last step is a rename on one disk)
		std::string const temporary = target + ".saving";

		DeleteFileA(temporary.c_str());

		// the Windows error said with the refusal is the save's own: the delete above leaves "file not found" (2) when there was no ".saving", and a refusal that sets
		// none (the level not held paired, the loader refusing the image) would carry it (Frost 11-17)
		SetLastError(0);

		LevelSaveResult const written = open->SaveAs(temporary.c_str());

		if (written != LevelSaveResult::Saved)
		{
			*why = Reason(written == LevelSaveResult::NothingOpen ? "the level could not be written (the level held is not paired with its bytes)" :
			              written == LevelSaveResult::GameWouldRefuse ? "the level could not be written (the game's loader refused it)" :
			              "the level could not be written", temporary);
			DeleteFileA(temporary.c_str());
			return LevelSaveOver::WriteFailed;
		}

		LevelSaveOver const placed = PutInPlace(target, temporary, exists, keepTheOneBefore, asItWas, backupPath, why);

		// from now on the level stands for the file saved over, not for the temporary name SaveAs was given (level_file.h, NowStandsFor)
		if (placed == LevelSaveOver::Saved)
		{
			open->NowStandsFor(target.c_str());
		}

		return placed;
	}

	uint32_t LevelSave_ViewFlags(uint32_t flags)
	{
		uint32_t const notAtADifficulty = OBJECT_FLAG_NOTDIFFICULTY1 | OBJECT_FLAG_NOTDIFFICULTY2 | OBJECT_FLAG_NOTDIFFICULTY3;
		uint32_t const inEveryGame = OBJECT_FLAG_PCLOAD | OBJECT_FLAG_AVPGAMEMODEMARINE | OBJECT_FLAG_AVPGAMEMODEALIEN | OBJECT_FLAG_AVPGAMEMODEPREDATOR;

		return (flags & ~notAtADifficulty) | inEveryGame;
	}

	namespace
	{
		size_t const kChunkHeadBytes = 12;   // a chunk starts with its 8 letters and then its whole size, these 12 bytes included, as 4 bytes (low byte first)

		uint32_t WordAt(std::vector<char> const &bytes, size_t at)
		{
			return (uint32_t)(uint8_t)bytes[at] | ((uint32_t)(uint8_t)bytes[at + 1] << 8) | ((uint32_t)(uint8_t)bytes[at + 2] << 16) | ((uint32_t)(uint8_t)bytes[at + 3] << 24);
		}

		void PutWordAt(std::vector<char> &bytes, size_t at, uint32_t word)
		{
			for (int part = 0; part < 4; part++)
			{
				bytes[at + part] = (char)(uint8_t)(word >> (8 * part));
			}
		}

		// The chunk at `at`, inside a parent that ends at `end`: its whole size, or 0 when it can't be one (fewer than 12 bytes left, or a size smaller than its own head or
		// reaching past the parent's end)
		size_t ChunkSizeAt(std::vector<char> const &bytes, size_t at, size_t end)
		{
			if (at > end || end - at < kChunkHeadBytes)
			{
				return 0;
			}

			size_t const size = WordAt(bytes, at + 8);

			return size < kChunkHeadBytes || size > end - at ? 0 : size;
		}
	}

	size_t LevelSave_EveryPlacedObjectIn(std::vector<char> &bytes)
	{
		if (bytes.size() < kChunkHeadBytes || memcmp(bytes.data(), "REBINFF2", 8) != 0)
		{
			return 0;
		}

		size_t const end = ChunkSizeAt(bytes, 0, bytes.size());

		if (end == 0)
		{
			return 0;
		}

		// 1. the walk, writing nothing: where each top-level object's header flags are (the game reads an object's first OBJHEAD1, Object_Chunk::get_header)
		std::vector<size_t> flagsAt;

		for (size_t at = kChunkHeadBytes; at < end;)
		{
			size_t const size = ChunkSizeAt(bytes, at, end);

			if (size == 0)
			{
				return 0;   // the level ends inside a chunk: left as it was
			}

			if (memcmp(&bytes[at], "RBOBJECT", 8) == 0)
			{
				for (size_t inner = at + kChunkHeadBytes; inner < at + size;)
				{
					size_t const innerSize = ChunkSizeAt(bytes, inner, at + size);

					if (innerSize == 0)
					{
						return 0;
					}

					if (memcmp(&bytes[inner], "OBJHEAD1", 8) == 0)
					{
						if (innerSize >= kChunkHeadBytes + 4)
						{
							flagsAt.push_back(inner + kChunkHeadBytes);
						}

						break;
					}

					inner += innerSize;
				}
			}

			at += size;
		}

		// 2. the flags of each PLACED object made the view's (LevelSave_ViewFlags); nothing else is written
		size_t changed = 0;

		for (size_t const where : flagsAt)
		{
			uint32_t const flags = WordAt(bytes, where);
			uint32_t const shown = LevelSave_ViewFlags(flags);

			if ((flags & OBJECT_FLAG_PLACED_OBJECT) && shown != flags)
			{
				PutWordAt(bytes, where, shown);
				changed++;
			}
		}

		return changed;
	}

	bool LevelSave_ViewNeedsItsOwnCopy(OpenLevelFile *open)
	{
		File_Chunk *const level = open ? open->Level() : nullptr;

		if (!level)
		{
			return false;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Header_Chunk *const header = each() ? each()->get_header() : nullptr;
			uint32_t const flags = header ? (uint32_t)header->flags : 0;

			if ((flags & OBJECT_FLAG_PLACED_OBJECT) && LevelSave_ViewFlags(flags) != flags)
			{
				return true;
			}
		}

		return false;
	}

	void LevelSave_ViewsPlacedObjects(OpenLevelFile *open, std::vector<ViewPlacedObject> *out)
	{
		File_Chunk *const level = open ? open->Level() : nullptr;

		if (!out)
		{
			return;
		}

		out->clear();

		if (!level)
		{
			return;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();
			Object_Header_Chunk *const header = object ? object->get_header() : nullptr;

			if (!header || !((uint32_t)header->flags & OBJECT_FLAG_PLACED_OBJECT))
			{
				continue;
			}

			ViewPlacedObject one;
			size_t length = 0;

			static_assert(sizeof(one.id) == sizeof(object->object_data.ID), "an object's ID is 8 bytes");
			memcpy(one.id, &object->object_data.ID, sizeof(one.id));

			// the name up to its first zero, never past its array (the loader's CopyObjectName always ends it with one)
			while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length])
			{
				length++;
			}

			one.name.assign(object->object_data.o_name, length);
			one.place[0] = object->object_data.location.x;
			one.place[1] = object->object_data.location.y;
			one.place[2] = object->object_data.location.z;
			out->push_back(one);
		}
	}

	namespace
	{
		// LevelSave_ViewCopy's and LevelSave_ViewEveryObjectCopy's write: the working bytes built (with every placed object in every game when everyObject, *changed saying how
		// many objects' flags that changed), then the temporary file, read-back and put in place, with no .bak
		LevelSaveOver WriteTheViewsBytes(OpenLevelFile *open, char const *targetPath, bool everyObject, size_t *changed, std::string *why)
		{
			std::string ignored;
			size_t none = 0;

			why = why ? why : &ignored;
			changed = changed ? changed : &none;
			*changed = 0;

			std::string const target = targetPath ? targetPath : "";
			bool exists = false;
			LevelSaveOver const usable = TargetIsUsable(open, target, &exists, why);

			if (usable != LevelSaveOver::Saved)
			{
				return usable;
			}

			// 0. the copy's file is a plain file with one name, or nothing yet (F-a; not compared with the level as opened: a level opened with "Open with" stands for this very file)
			std::vector<char> asItWas;
			LevelSaveOver const mayWrite = MayWriteOver(open, target, false, &asItWas, why);

			if (mayWrite != LevelSaveOver::Saved)
			{
				return mayWrite;
			}

			// 1. the working bytes, built without changing the open level, in a temporary file in the target's own folder
			std::string const temporary = target + ".saving";
			std::vector<char> bytes;

			DeleteFileA(temporary.c_str());

			if (!open->BuildLevel(bytes) || bytes.empty())
			{
				*why = "the level as edited could not be put together (its chunks are not paired with its bytes, or not as they were paired): " + target;
				return LevelSaveOver::WriteFailed;
			}

			// the view's own copy: every placed object in every game (level_save.h); none to change, nothing to write
			if (everyObject)
			{
				*changed = LevelSave_EveryPlacedObjectIn(bytes);

				if (*changed == 0)
				{
					why->clear();
					return LevelSaveOver::Saved;
				}
			}

			// the Windows error said with a refused write is the write's own, as LevelSave_Over's (the delete above leaves "file not found" when there was no ".saving")
			SetLastError(0);

			if (!WriteNewFile(temporary, bytes))
			{
				*why = Reason("the level could not be written", temporary);
				DeleteFileA(temporary.c_str());
				return LevelSaveOver::WriteFailed;
			}

			return PutInPlace(target, temporary, exists, false, asItWas, nullptr, why);
		}
	}

	LevelSaveOver LevelSave_ViewCopy(OpenLevelFile *open, char const *targetPath, std::string *why)
	{
		return WriteTheViewsBytes(open, targetPath, false, nullptr, why);
	}

	LevelSaveOver LevelSave_ViewEveryObjectCopy(OpenLevelFile *open, char const *targetPath, size_t *changed, std::string *why)
	{
		return WriteTheViewsBytes(open, targetPath, true, changed, why);
	}

	namespace
	{
		// the whole path, lower case: the sitting's key for a file
		std::string SittingKey(char const *path)
		{
			char whole[MAX_PATH * 2] = "";
			DWORD const got = path ? GetFullPathNameA(path, (DWORD)sizeof(whole), whole, nullptr) : 0;
			std::string key = got > 0 && got < sizeof(whole) ? whole : (path ? path : "");

			for (char &letter : key)
			{
				letter = letter == '/' ? '\\' : (char)tolower((unsigned char)letter);
			}

			return key;
		}
	}

	bool LevelSaveSitting::SavedBefore(char const *targetPath) const
	{
		return saved.count(SittingKey(targetPath)) > 0;
	}

	void LevelSaveSitting::MarkSaved(char const *targetPath)
	{
		std::string const key = SittingKey(targetPath);

		saved.insert(key);
		kept.insert(key);
	}

	LevelSaveOver LevelSaveSitting::Save(OpenLevelFile *open, char const *targetPath, std::string *backupPath, std::string *why)
	{
		std::string const key = SittingKey(targetPath);
		bool const first = saved.count(key) == 0;
		std::string written;
		LevelSaveOver const result = LevelSave_Over(open, targetPath, first, &written, why);

		if (result == LevelSaveOver::Saved)
		{
			saved.insert(key);

			if (first && !written.empty())
			{
				kept.insert(key);
			}
		}

		if (backupPath)
		{
			*backupPath = result == LevelSaveOver::Saved && kept.count(key) ? std::string(targetPath) + ".bak" : std::string();
		}

		return result;
	}
}
