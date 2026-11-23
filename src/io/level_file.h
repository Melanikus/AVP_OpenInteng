// Opening a level to edit, and saving it back: the phase 2 plan's F7. Written 2026-11-14 by Melanikus for AVP_OpenInteng.
//
// WHY IT'S DONE THIS WAY. Both were measured on the 86 level files the game comes with, before this was written:
// - A chunk nobody changed is written back as the very bytes it was read from. Only a changed chunk is written by the game's chunk
//   library, and always into memory this file fills with zeros first, so every byte the library doesn't set is a zero. (The library's
//   own writer is on an AvPx card. Rule stated 2026-11-14 by Melanikus (Drake), F7 N8.)
// - Every level the game comes with is packed (Huffman-coded). Packing a level again with the game's own packer gives back the same
//   file for only 2 of the 86, because it can choose its codes differently. So a save with nothing changed writes the file's own
//   bytes, and a changed level is packed again, which the game reads just the same.
//
// WHAT A SAVE DOES, in order (CODING-STANDARD.md, rule 2):
//   1. builds the whole level in memory, as above;
//   2. writes it to a file of its own beside the one the user picked, named after it with ".saving" on the end, and makes the disk
//      keep it (a flush);
//   3. loads that file back through the game's own loader, and if the game would refuse it, deletes it and stops;
//   4. the first time a file is saved over, copies the file as it was to the same name with ".bak" on the end. That happens once: a
//      .bak that's already there is never replaced, so it always holds the file from before the editor first touched it;
//   5. puts the new file in place of the old in one step, which the disk keeps even if the power goes straight after.
// If any step fails, the file the user picked is left as it was. No name from inside a level ever becomes part of a path: every path
// here is the one the user picked, with ".saving" or ".bak" added.
//
// WHAT COUNTS AS CHANGED. A mode that changes a chunk's numbers says so with MarkChanged. Only a chunk that holds numbers can be marked:
// a chunk that holds other chunks has nothing of its own but them, so the one inside it that changed is marked instead. No chunk may be
// added to the level or removed from it: the level is built for a save only while it's still the tree that was paired, chunk for chunk,
// and otherwise the save is refused (CantBuild). The modes that need to add or remove a chunk bring that. (Enforced 2026-11-14 by
// Melanikus (Drake), F7 L13.)

#ifndef AVP_OPENINTENG_LEVEL_FILE_H
#define AVP_OPENINTENG_LEVEL_FILE_H

#include <stddef.h>
#include <map>
#include <string>
#include <set>
#include <vector>

class Chunk;
class Chunk_With_Children;
class File_Chunk;

// How opening a level went
enum class LevelOpenResult
{
	Opened,
	NoPath,               // no path was given, or it names a device or a file's stream, not a plain file (F7 N7)
	CantRead,             // the file couldn't be opened, or read whole, or is bigger than any level the game reads
	LoaderRefused,        // the game's own loader refuses the file, so the editor does too
	ChangedWhileOpening,  // the file the game's loader read isn't the one this file read a moment before: something changed it
	DoesntPair,           // the loaded chunks don't match the file's own, one for one, so they couldn't be written back as they were
	TooManyChunks,        // the level has more chunks than the editor takes (far more than any real level has)
	NothingOpen,          // ReplaceLevel: no level is open, or the one open couldn't be paired
};

// How a save went. Only Saved means the file the user picked has changed.
enum class LevelSaveResult
{
	Saved,
	NothingOpen,          // no level is open, or the last one couldn't be paired with its bytes again
	NoPath,               // no path was given, or it can't be made whole, or it names a device or a file's stream (F7 N7)
	ReadOnly,             // the file picked is marked read-only
	ChangedOnDisk,        // the file picked is the one the level stands for (it was opened from it, or last saved to it), and it isn't
	                      // as it was then: something else changed it, so it isn't replaced (added 2026-11-14, F7 N3)
	CantBuild,            // the level couldn't be put together, or packed, or is too big for the game
	SavingFileInTheWay,   // a file with the ".saving" name is there already: it isn't this save's, so it's never deleted here
	OwnSavingFileInTheWay, // the ".saving" in the way is one an earlier save of this OpenLevelFile made and couldn't delete (LeftBehind),
	                       // the very same file still: the editor's own, safe to delete by hand, but never deleted here
	CantWrite,            // the new file couldn't be written whole and flushed, or what the disk gave back wasn't what was written
	GameWouldRefuse,      // the game's own loader refuses the new file, so nothing was replaced
	CantKeepBackup,       // the one-time ".bak" couldn't be made, so nothing was replaced
	CantReplace,          // the new file couldn't be put in place of the old one, which is still there as it was; or the file there
	                      // isn't a plain file on a disk with one name, which is never replaced (F7 N7)
};

// A file the editor made itself and then couldn't delete again, and which file it is on its disk, so that a file put there since under
// the same name is never taken for it. (Added 2026-11-14 by Melanikus (Drake), F7 L10.)
struct LeftFile
{
	std::string name;
	bool identityKnown;           // whether the numbers below could be read
	unsigned char identity[24];   // Windows' FILE_ID_INFO for it: the disk's serial number, then the file's own 16-byte number there
};

// One level file, opened to be edited. It holds the game's own loaded level, the file's bytes as they were read, and where in them
// each loaded chunk came from.
class OpenLevelFile
{
public:
	OpenLevelFile();
	~OpenLevelFile();

	// Opens a level file. A level already open is closed first, whatever happens.
	LevelOpenResult Open(const char * path);

	// The loaded level, as the game's chunk library holds it, or nothing if no level is open
	File_Chunk * Level() const { return level; }

	// Says that a mode has changed this chunk's numbers, so the next save writes it from what the library holds. Gives back false, and
	// marks nothing, for a chunk that isn't one of this level's or that holds other chunks.
	bool MarkChanged(Chunk * chunk);

	// The bytes a chunk that holds only numbers holds NOW, after its 12-byte heading: the bytes the editor last put there with SetPayload,
	// or, if it never has, the very bytes the chunk was read from. The chunk's own class, which the library keeps, is never changed by an
	// edit, so it is these bytes, and never the class, that say what the chunk holds. Gives back false, with *payload left alone, for no
	// level open, no chunk, a chunk that isn't one of this level's, or one that holds other chunks. (Added 2026-11-14, F7 edits.)
	bool PayloadOf(Chunk * chunk, std::vector<unsigned char> * payload) const;

	// Replaces what a chunk that holds only numbers holds with these bytes, which have to be exactly as many as it has now: an edit of a
	// field never changes the chunk's size, and a chunk that grows or shrinks is a different kind of change. The chunk is marked changed, and
	// the next save writes these bytes for it, after its heading as it was, and every other byte as it was read. The bytes are not judged
	// here: the field model checks each value before it's put in, and the game's loader checks the whole file before a save replaces
	// anything (SaveAs, GameWouldRefuse). From now on, for as long as this level is open, MarkChanged refuses this chunk: the library's class
	// for it still says what the file held before the edit, so writing it the library's way would undo the edit. Gives back false, changing
	// nothing, for the chunks PayloadOf refuses and for a payload of another size. (Added 2026-11-14, F7 edits.)
	bool SetPayload(Chunk * chunk, std::vector<unsigned char> const & payload);

	bool HasChanges() const { return !changed.empty() || restructured; }

	// Replaces the level the editor holds with another whole level, for an edit that adds or takes away a chunk (S5: create and delete an
	// object), which MarkChanged and SetPayload can't do. image is a whole unpacked level (REBINFF2 and its chunks, the form BuildLevel gives),
	// with the edit already made in its bytes. The game's own loader reads it first, from a private copy, and the new level is paired with its
	// bytes, all while the level the editor holds is untouched: only when every step has gone through is the new level swapped in. Then every
	// Chunk * handed out before is gone (the old level is deleted), the edits made so far are in the new bytes, the next save writes the new
	// level, and Generation() is one more. Gives back Opened when it's done; otherwise nothing has changed, and the result says what refused
	// it (LoaderRefused: the game wouldn't take that level). (Added 2026-11-15 by Melanikus (Drake), S5.)
	LevelOpenResult ReplaceLevel(std::vector<char> const & image);

	// Which level, and which swap of it, this is, so a view of the level (and every cache made for it) knows to build itself again. Each ReplaceLevel makes it one more.
	// Each Open (and each Close) moves it to a number the run never gave before, in this file object or in any other (kGenerationsAnOpen apart, from one count for the
	// whole program), so a cache made for one level is never taken for the next one opened (Vasquez 11-20: it went back to 0, and the Waypoint paint's game module
	// numbers were a first level's on a second one)
	unsigned Generation() const { return generation; }

	// How many edits the level has taken since it was opened: every chunk marked changed (MarkChanged, SetPayload: the same size) and every level swapped in (ReplaceLevel). The view of the
	// level (editor_main.cpp ShowTheWorkingBytes, the view gap) loads the working bytes again when it differs from the count it last loaded at; Generation() alone misses a same-size edit.
	// (Added 2026-11-17 by Melanikus (Dietrich): Drake's find, a Rescale was not shown after a screen change.)
	unsigned Changes() const { return changes; }

	// Saves the level to the file the user picked: the one it was opened from, or another. The steps are at the top of this file.
	LevelSaveResult SaveAs(const char * path);

	// SaveAs's own checks of the file it replaces, asked of path itself (Vasquez's find F-a, 2026-11-22). The save over the opened file (src/host/level_save.h) has SaveAs
	// write the level to "<path>.saving", then puts that file in place of path, so SaveAs's checks only ever looked at the temporary name and never at the file replaced.
	// Gives back Saved when nothing is there yet, or when path is a plain file on a disk with one name only (not a link or any other reparse point, a folder, a device,
	// or one of a hard link's names: F7 N7), read whole without following a link of that name, and, with againstOpened, when it is the file the level stands for (it was
	// opened from it, or last saved to it) it is as it was then, byte for byte (F7 N3). Then *asItIs holds its bytes (none when nothing is there), for the save's last
	// look before the replace (LevelFile_StillThese). Otherwise NoPath (it can't be made a whole path, or names a device or a stream), CantReplace (not a plain file
	// with one name, or it can't be read as one) or ChangedOnDisk, with *asItIs empty. The view gap's copy asks without againstOpened: a level opened with "Open with"
	// stands for its copy in avp_rifs\OpenedByEditor, which is the very file the view copy writes over at every screen change.
	LevelSaveResult MayWriteOver(const char * path, bool againstOpened, std::vector<char> * asItIs) const;

	// From now on the level stands for path: the save over the opened file put what SaveAs wrote in place of path itself, not of the temporary name SaveAs was given, so
	// the next save's MayWriteOver compares path with the bytes saved. Its numbers on the disk are read again from path, without following a link of that name.
	// (Vasquez's F-a, 2026-11-22.)
	void NowStandsFor(const char * path);

	// The level as the next save would write it, unpacked (a REBINFF2 block). For the save itself, and for tests.
	bool BuildLevel(std::vector<char> & out);

	// The files this OpenLevelFile's opens and saves made and then couldn't delete again: a ".saving", a ".bak" a failed save had begun,
	// or a private copy for the game's loader. Each is the editor's own, so the editor can say so, and they're safe to delete by hand.
	// (Added 2026-11-14 by Melanikus (Drake), F7 L10.)
	std::vector<LeftFile> const & LeftBehind() const { return leftBehind; }

	OpenLevelFile(OpenLevelFile const &) = delete;
	OpenLevelFile & operator=(OpenLevelFile const &) = delete;

private:
	// The two below without their guard: Open and SaveAs call these, and refuse if the memory they need can't be had
	LevelOpenResult OpenUnguarded(const char * path);
	LevelSaveResult SaveAsUnguarded(const char * path);

	void Close();
	bool PairLevel();
	bool Pair(Chunk_With_Children * holder, size_t from, size_t to);
	void NoteChange(Chunk * chunk);
	bool Write(size_t from, size_t to, std::vector<char> & out);
	bool TreeIsAsPaired(Chunk_With_Children * holder, size_t from, size_t to, size_t * bytesNow);

	File_Chunk * level;
	std::vector<char> fileBytes;    // the file exactly as it was on disk
	std::vector<char> levelBytes;   // the level unpacked: the same bytes as fileBytes for a file that isn't packed
	bool packed;
	bool paired;
	size_t chunksSeen;               // the chunks counted so far by PairLevel, and whether there were more than the editor takes
	bool tooManyChunks;
	std::string savingMade;          // the ".saving" file this save made, while it's there and this save's own; otherwise empty

	// The file the level stands for: the one it was opened from, or last saved to. Its whole path, and which file it is on its disk
	// (FILE_ID_INFO, when that could be read), so a save over it can first ask whether it's still as it was. (Added 2026-11-14 by
	// Melanikus (Drake), F7 N3.)
	std::wstring standsForPath;
	bool standsForKnown;
	unsigned char standsForIdentity[24];
	std::vector<LeftFile> leftBehind; // LeftBehind's files; kept across Close and Open, as the files stay on the disk

	// A loaded chunk, and its size in levelBytes as it was checked when it was paired
	struct ChunkPlace
	{
		Chunk * chunk;
		size_t bytes;
	};

	std::map<size_t, ChunkPlace> chunkAt;                 // the loaded chunk whose bytes start at each place in levelBytes
	std::map<Chunk *, Chunk_With_Children *> holderOf;     // the chunk each loaded chunk is inside: the level itself for the top ones
	std::set<Chunk *> changed;                             // marked by MarkChanged, or by SetPayload
	std::map<Chunk *, std::vector<unsigned char> > edited; // the bytes SetPayload put in a chunk, for the save to write (only chunks in changed)
	std::set<Chunk *> classIsStale;                        // every chunk SetPayload has edited, since the level was opened: MarkChanged refuses them
	std::map<Chunk *, size_t> placeOf;                     // where each paired chunk starts in levelBytes (the key of chunkAt, the other way round)
	std::set<Chunk *> holdsAChange;                        // every chunk with a changed one somewhere inside it
	std::set<Chunk *> holders;                             // every paired chunk that holds others, found as it was paired, so no
	                                                       // chunk handed in is ever asked itself (F7, 2026-11-14)

	bool restructured;                                     // ReplaceLevel has made the level different from the file's: a save writes levelBytes
	unsigned generation;                                   // ReplaceLevel's count
	unsigned changes;                                      // Changes(): NoteChange's count and ReplaceLevel's
	LevelOpenResult ReplaceLevelUnguarded(std::vector<char> const & image);
};

// The save's last look at the file it is about to replace, right before the replace (src/host/level_save.cpp): true only when path is there, a plain file on a disk with
// one name only, read without following a link of that name, holding exactly these bytes (the ones MayWriteOver gave). (Added 2026-11-22 by Melanikus (Vasquez), F-a.)
bool LevelFile_StillThese(const char * path, std::vector<char> const & bytes);

#endif
