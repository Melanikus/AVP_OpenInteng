// edge_own.cpp: the editor's own versions of the few functions the game keeps in its network files but calls in a game played alone.
// Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng (R-INT-15). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
//
// WHY: the editor leaves out the game's network files (docs/avpx-files.md), and tools/make-edge.js gives every function in them a
// stand-in that stops the editor, because the editor never plays online. A few of those functions aren't about the network at all: the
// game keeps them in pldnet.cpp because they borrow its "ghost" (the way it draws another player), and it calls them in every level,
// played alone or not. A stop there would stop the editor at every level it opens. So each one here is the editor's own version (the
// second answer in docs/host-symbols.md), saying what the game does and what the editor does instead. make-edge.js leaves every name
// defined here out of its stand-ins, and checks that each one is still a name the game calls.
//
// One game file that isn't a network file is left out too, on purpose, and answered here: ConfigFile.cpp, the player's game settings
// file. The last part of this file says why. (Added 2026-11-14 by Melanikus (Hicks).)

#include <windows.h>
#include <stdio.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

#include "ConfigFile.h"
#include "nobody_watching.h"

// STAYING OUT OF THE WAY. One of Hudson's hooks (Hudson is the game's test player; the editor runs him asleep, src/host/edge/asleep_*):
// the game asks it whether its window may become the active one and come to the front when it sizes the window (win_proj.cpp:684,
// ChangeWindowsSize, which the game's InitialiseDirect3D calls at d3_func.cpp:2509). With Hudson asleep the game's own answer is no
// (hudson.cpp:1198-1201), and the window takes the keyboard. That's right when someone has just started the editor. When nobody is at
// the screen (nobody_watching.h: a run that only saves a picture, or proves something), it must take nothing from whoever is using the
// machine, which is this hook's own meaning in the game: so then the answer is yes. Nothing else in what the editor compiles asks it.
bool Hudson_StaysOutOfTheWay()
{
	return NobodyWatching();
}

// ASKING SOMEONE. Another of Hudson's hooks: the game calls it just before it shows a message box (utilities.cpp's avp_MessageBox, which
// the level loader calls on a file it refuses, chunk_load.cpp:590-602), so that a test run, with nobody to answer, can say what it
// wanted to ask instead. With Hudson asleep the game's own answer is to do nothing, and the box appears (hudson.cpp:11442-11446). When
// someone is at the editor's screen, that's right. When nobody is, the question goes to the error output, so the run's own record has it,
// and nobody_watching.cpp then refuses to make the box at all.
void Hudson_StopIfTheGameWantsToAsk(const char *message)
{
	if (NobodyWatching())
	{
		fprintf(stderr, "AVP_OpenInteng: the game wanted to ask someone this, and nobody is at the screen: %s\n", message ? message : "");
	}
}

// THE PLAYER'S IMAGE IN A MIRROR. Three levels have a mirror the player sees himself in: e3demo, e3demosp and derelict. For those, the
// game makes a copy of the player's body when the level starts (game.cpp:288-290, npcsetup.cpp:288, pldnet.cpp:11414-11420) and draws
// it in the mirror each frame (kzsort.cpp:583-588, pldnet.cpp:11518). The editor has no player's body: its camera is only a pair of
// eyes. So no level asks for that copy, and there's none to draw. The mirror room itself is drawn as the game draws it.

// The game answers yes for e3demo, e3demosp and derelict (pldnet.cpp:11414). The editor answers no for every level, so the game never
// makes the copy (CreatePlayersImageInMirror keeps its stand-in that stops: nothing calls it now) and never loads the model for it.
BOOL Current_Level_Requires_Mirror_Image()
{
	return FALSE;
}

// The game draws the copy whenever the mirror is in view (kzsort.cpp:583-588), which view.cpp:870-880 decides from the level's name and
// the room the camera is in, not from the answer above. With no copy made, there's nothing to draw, so this draws nothing.
void RenderPlayersImageInMirror(void)
{
}

// FUNCTIONS WHOSE FIRST STEP IS "IS THIS A NETWORK GAME?". The game calls each of these from code that runs in every game, alone or not,
// and each one's first statement gives its whole answer when there's no network game (AvP.Network is I_No_Network): its line is given
// with each. The editor never has a network game, so each answers exactly that. Where a real one goes on to read what it's handed
// (a lift's dynamics, say), the no-network answer comes first, so nothing is read here either. The types are written as the linker
// writes them (struct strategyblock, struct vectorch, unsigned int for NetID), and the link checks that each matches the game's.

// pldnet.cpp:13865: return AvP.Network == I_No_Network || ... : true with no network game
bool SameAsSinglePlayer(void)
{
	return true;
}

// pldnet.cpp:8660: if (AvP.Network != I_Host || ...) return;
void AddNetMsg_LiftState(struct strategyblock *)
{
}

// pldnet.cpp:8845: if (AvP.Network == I_No_Network || ...) return;
void AddNetMsg_NpcKilled(struct strategyblock *, int, int, int)
{
}

// pldnet.cpp:8637: if (AvP.Network == I_No_Network || ...) return;
void AddNetMsg_NpcSound(int, int, int, int, struct vectorch const *)
{
}

// pldnet.cpp:8728: if (AvP.Network == I_No_Network || ...) return;
void AddNetMsg_NpcThrownObject(struct strategyblock *)
{
}

// networking.cpp:2460: if (AvP.Network != I_Host || ...) return;
void Net_RemoveThisPlayer(unsigned int)
{
}

// pldnet.cpp:13787: if (AvP.Network == I_No_Network || ...) return false;
bool Coop_EveryoneIsDead(void)
{
	return false;
}

// pldnet.cpp:14466: if (... || AvP.Network == I_No_Network) return false;
bool Coop_LevelCompleteLine(char *, size_t)
{
	return false;
}

// pldnet.cpp:14627: if (AvP.Network == I_No_Network || ...) return;
void LevelEvents_Happened(const char *)
{
}

// pldnet.cpp:1371: return AvP.Network != I_No_Network && ... : false with no network game
bool Net_BlocksAreShort()
{
	return false;
}

// pldnet.cpp:14226: if (AvP.Network != I_Host || ...) return;
void Net_CoopHostSays(void)
{
}

// pldnet.cpp:14357: if (AvP.Network != I_Peer || ...) return;
void Net_CoopJoinerSays(void)
{
}

// NOTING A LEVEL'S EVENTS FOR THE NETWORK GAME. When a level is set up, the game notes each object that sets something off when it
// dies (object_setup.cpp:2581 and on), in a list kept in pldnet.cpp (pldnet.cpp:14526, 14538-14554). Only the network game ever reads
// that list: it sends what happened to the other players (:14597-14603), and finds an event when one arrives (:14632, :14667). The
// editor never has a network game, so nothing would ever read what's noted, and noting it changes nothing: these note nothing.
void LevelEvents_Note(const char *, const char *, int)
{
}

void LevelEvents_NoteItem(const char *, const char *, int, int, int, int)
{
}

// KEEPING A NETWORK GAME ANSWERED WHILE A LEVEL LOADS. The game's loading screen calls this at every step of its progress bar, in every
// game (progress_bar.cpp:126), so that the other players don't time out during a long load. With no network game, the game's own
// returns at once, before anything else (networking.cpp:2716-2721: there's no host). The editor never has a network game, so this
// does exactly that: nothing.
void Net_KeepAnsweringWhileLoading()
{
}

// THE PLAYER'S GAME SETTINGS. The game keeps its settings in AliensVsPredator.cfg, in its save folder (Documents\My Games\Aliens versus
// Predator): the screen size, the sound, the keys, the lighting. ConfigFile.cpp reads that file when the game starts (winmain.cpp:292)
// and writes the settings it holds back into it, whole, from several places: when the game's Direct3D is let go
// (d3_func.cpp:3003, in ReleaseDirect3D), when the video mode changes (d3_func.cpp:631), and from the options pages.
//
// The editor must never write that file: it belongs to the player's game. And it did. The editor never read the file, so the game's
// settings in the editor held only the few the editor's own code asked for, each at its first value; and the editor's own close called
// ReleaseDirect3D, which wrote those few over the player's whole file. On 2026-11-14 that left Melanikus's file with five settings: his
// screen size, his sound and the rest were gone, and his next game started at 800 by 600.
//
// So the editor leaves ConfigFile.cpp out (docs/avpx-files.md), and these are its answers, the game's own rules for each call kept in
// memory and nothing else: a setting the editor asks for, and doesn't have, starts at the first value it's asked with, as the game's own
// does when its file doesn't have it; a setting can be changed once it has been asked for, as the game's can. Nothing here opens, reads
// or writes a file. Every setting starts afresh each time the editor starts.

// The settings held, under their headings, as the game holds them (ConfigFile.cpp's AvPConfig): heading, then name, then value as text
static std::map<std::string, std::map<std::string, std::string>> settingsInMemory;

// ConfigFile.cpp:49, Config_Load, isn't here: only the game's own start reads the file (winmain.cpp:292), and the editor leaves that out.
// So the editor starts with no settings, and nothing in it asks for the file to be read.

// ConfigFile.cpp:130. The game writes its file here. The editor writes nothing, ever, and that's all this does.
bool Config_Save()
{
	return true;
}

// ConfigFile.cpp:186-200. A whole number: the one held, or, if there's none, the first value asked for, which is then held.
int Config_GetInt(const std::string &heading, const std::string &variable, int defaultValue)
{
	std::map<std::string, std::string> &underHeading = settingsInMemory[heading];
	const std::map<std::string, std::string>::const_iterator held = underHeading.find(variable);
	if (held == underHeading.end())
	{
		std::ostringstream asText;
		asText << defaultValue;
		underHeading.insert(std::make_pair(variable, asText.str()));
		return defaultValue;
	}

	// The game's own reading of a number, as utilities.cpp:112-123 does it: a text that isn't a number reads as 0
	std::istringstream text(held->second);
	int value = 0;
	text >> value;
	return value;
}

// ConfigFile.cpp:202-208. Changes a whole number that's held. One that's never been asked for isn't held, and nothing changes.
void Config_SetInt(const std::string &heading, const std::string &variable, int newValue)
{
	const std::map<std::string, std::map<std::string, std::string>>::iterator underHeading = settingsInMemory.find(heading);
	if (underHeading == settingsInMemory.end() || underHeading->second.find(variable) == underHeading->second.end())
	{
		return;
	}
	std::ostringstream asText;
	asText << newValue;
	underHeading->second[variable] = asText.str();
}

// ConfigFile.cpp:210-216. Changes a text that's held. One that's never been asked for isn't held, and nothing changes.
void Config_SetString(const std::string &heading, const std::string &variable, const std::string &newValue)
{
	const std::map<std::string, std::map<std::string, std::string>>::iterator underHeading = settingsInMemory.find(heading);
	if (underHeading == settingsInMemory.end() || underHeading->second.find(variable) == underHeading->second.end())
	{
		return;
	}
	underHeading->second[variable] = newValue;
}

// ConfigFile.cpp:218-238. A text: the one held, without its quote marks, or, if there's none, the first value asked for, which is then held.
std::string Config_GetString(const std::string &heading, const std::string &variable, const std::string &defaultString)
{
	std::map<std::string, std::string> &underHeading = settingsInMemory[heading];
	const std::map<std::string, std::string>::const_iterator held = underHeading.find(variable);
	if (held == underHeading.end())
	{
		underHeading.insert(std::make_pair(variable, defaultString));
		return defaultString;
	}
	std::string text = held->second;
	text.erase(std::remove(text.begin(), text.end(), '"'), text.end());
	return text;
}

// ConfigFile.cpp:240-282. Yes or no: the one held, or, if there's none, the first value asked for, which is then held. A held text that
// is neither "true" nor "false" is set to the value asked with, which is given back.
bool Config_GetBool(const std::string &heading, const std::string &variable, bool defaultValue)
{
	std::map<std::string, std::string> &underHeading = settingsInMemory[heading];
	const std::map<std::string, std::string>::iterator held = underHeading.find(variable);
	if (held == underHeading.end())
	{
		underHeading.insert(std::make_pair(variable, defaultValue ? "true" : "false"));
		return defaultValue;
	}
	std::string text = held->second;
	text.erase(std::remove(text.begin(), text.end(), '"'), text.end());
	if (text == "true")
	{
		return true;
	}
	if (text == "false")
	{
		return false;
	}
	held->second = defaultValue ? "true" : "false";
	return defaultValue;
}

// ConfigFile.cpp:284-297. Changes a yes or no that's held. One that's never been asked for isn't held, and nothing changes.
void Config_SetBool(const std::string &heading, const std::string &variable, bool newValue)
{
	const std::map<std::string, std::map<std::string, std::string>>::iterator underHeading = settingsInMemory.find(heading);
	if (underHeading == settingsInMemory.end() || underHeading->second.find(variable) == underHeading->second.end())
	{
		return;
	}
	underHeading->second[variable] = newValue ? "true" : "false";
}
