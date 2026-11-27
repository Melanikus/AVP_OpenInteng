// ffinfo_lines.h: the lines an exported level's packs need in the game's fastfile\ffinfo.txt, and those lines added to a player's own file. Written
// 2026-11-27 by Melanikus (Frost) for AVP_OpenInteng, for Export map's "Install exported map" (AvP-docs director/design-dietrich-sound-import-export-map.md,
// section 3, option A, the owner's choice: "Map ships its own line + Open Inteng installs it"). Dietrich does the file side: the backup, the new file and
// the swap, and the command. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Pure: no Windows, no files.
//
// WHY. Steam AvP opens only the packs fastfile\ffinfo.txt names (ffInit, Rebellion's FFSTDIO.CPP:159-188). Each line is "<folder>;<pack>": a file asked for
// under that folder is looked for in that pack. A level's own sounds and textures, packed by fastfile_pack.h, play only once their line is in that one
// shared file, so Install adds it. Every rule below comes from how Rebellion's reader takes the file (read 2026-11-27 in the Assembla copy,
// branches/reb/3dc/avp/win95/FFSTDIO.CPP), and was checked on the game's own ffinfo.txt: 108 lines, 3,819 bytes, CRLF on every line, the last one too.
//
// A LINE OF OURS (FfInfo_Line):
//   - THE FOLDER, as the game asks for its files: "sound\Custom\MyLevel". It passes the game's rule for a name from a level (LevelFileName_WhyRefused,
//     avpx/3dc/level_file_name.h), uses '\' only (the game asks "SOUND\" + the level's folder + "\" + the name, Objsetup.cpp:121, and compares with
//     _strnicmp, which tells '\' from '/'), and is at most 199 letters.
//   - ITS FIRST LETTER IS SMALL. ffInit takes only a line whose first letter is a letter (FFSTDIO.CPP:177-178), and a pack whose folder starts with a
//     CAPITAL is kept loaded for the whole game, across every level (FFileDesc::ShouldBeKept, :27, and ffclose_almost_all, :226-268): a 32-bit game's
//     memory, held for good. 106 of stock's 108 lines start small; only Graphics\Common and Graphics\common objects start with a capital.
//   - A '\' IS ADDED AFTER THE FOLDER. The folder is matched as the start of the name asked for, no more (CouldInclude, _strnicmp over the folder's
//     length, :129-133), so "sound\Custom\MyLevel" would also take "sound\Custom\MyLevel2\boom.wav", and load MyLevel's pack for it. Stock leans on
//     that (sound\npc\alien also takes sound\npc\alienvoice: 5 such pairs). With the '\', only the level's own folder matches. The folder part runs to
//     the ';' (:54-61), so the '\' is kept. No stock line has one; nothing reads it differently.
//   - THE PACK: letters, digits and _, then ".FFL", at most 40 letters in all, and never a stock pack's name (Snd<N>, Tex<N>, common): the pack is what
//     makes two lines the same to the game (FFileDesc::operator== compares the pack's name only, :144-147), so a stock name would never load.
//   - SO IT FITS REBELLION'S BUFFERS, which nothing checks (the design's N5): the folder part is copied into a 256-byte buffer on the stack up to its
//     ';', and "fastfile\" and the pack into another (:34-81); ffInit reads a line into 512 bytes (:168-183). Ours: at most 200 letters for the folder
//     and its '\', 9 + 40 for the pack, a line of at most 241.
//
// ADDED TO A PLAYER'S FILE (FfInfo_Add). The file is the player's, so it's hostile input (Tao 3), and every byte of it is kept as it is:
//   - REFUSED, nothing added: over 64 KB; any byte but CR, LF and 32 to 126 (ffInit opens it in text mode, :161: Ctrl-Z there ends the file, so a line
//     after one is never read, and a NUL ends a line); a CR not followed by LF (text mode keeps a lone one, and it would end up in a pack's name, :63-72);
//     a line of 250 letters or more (it would overflow the 256-byte buffers before our line is even read, with or without a ';').
//   - Our lines go at the end, each with the file's own line ending (CRLF when it has any, as stock's, else LF; CRLF for an empty file), and the file's
//     last line gets one first when it has none.
//   - A line that's there already, capitals aside, isn't added again, so Install can run twice. A line whose pack is there already under another folder
//     is refused: the game would take the two as one and never load it for the second (operator==, above). A line whose folder is there already with
//     another pack is added: both are tried, in order (ffreadbuf, :317-346). That is the owner's Q3: a level keeping a stock folder (env\Temple) gets
//     "sound\env\Temple\;Custom_<level>_Snd.FFL" after stock's "sound\Env\Temple;Snd25.FFL"; stock's comes first, so only the level's new names come from
//     its own pack.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace model
{
	size_t const kFfInfoMostFolderLetters = 199;      // a line's folder, without the '\' added after it
	size_t const kFfInfoMostPackLetters = 40;         // a pack's name, ".FFL" included
	size_t const kFfInfoMostFileBytes = 64 * 1024;    // a player's whole ffinfo.txt (stock's: 3,819 bytes), before and after our lines
	size_t const kFfInfoMostLineLetters = 249;        // the longest line of a player's file we add to (stock's longest: 55)

	// One line of ours, "<folder>\;<pack>" with no line ending (A LINE OF OURS, above). True with *line; false with *line empty and *why in plain words.
	bool FfInfo_Line(std::string const &folder, std::string const &pack, std::string *line, std::string *why);

	// A player's ffinfo.txt (existing, its bytes as read) with our lines added (ADDED TO A PLAYER'S FILE, above). Each line is one FfInfo_Line made, and is
	// checked again here. True with *out the whole new file and *added how many lines went in (0 when all were there: then *out is existing, byte for
	// byte). False with *out empty, *added 0 and *why in plain words; any name from the file in it is shown safely (its odd letters as '?').
	bool FfInfo_Add(std::vector<uint8_t> const &existing, std::vector<std::string> const &lines, std::vector<uint8_t> *out, size_t *added, std::string *why);
}
