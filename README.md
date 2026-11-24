# AVP_OpenInteng

The first open community beta of AVP_OpenInteng: a remake of **inteng**, Rebellion's own level editor for **Aliens versus Predator**, now
open source. Like inteng, it works on a level once its geometry is built: place objects, NPCs and generators, give them their
behaviour, map textures onto the polygons, light the level, set its sounds and acoustics, lay out the AI's paths and waypoints, link
its modules, and set the level-wide settings. It works exactly like inteng, minus bugs and... you can move the camera with your mouse.

**It runs on a plain Steam AvP, no AvPx needed:** the editor brings its own shaders (since alpha.2).

**Aliens versus Predator was created by Rebellion, who own it.** The source code to Aliens vs Predator is copyright (c) 1999-2000
Rebellion, and this editor compiles part of it. It may not be used, in full or in part, for commercial purposes. A huge thank you to
Rebellion for the most awesome game. AVP_OpenInteng's own code is MIT (it belongs to everyone who loves this game); the game's code and
inteng's quoted words stay Rebellion's. See `License.txt`.

## Status

**Pre-release.** Its home is https://github.com/Melanikus/AVP_OpenInteng, a public repository. The first release is `v1.0.0-alpha.0`
(2026-11-18).

## Setting up

**Open Inteng is one folder. It needs no installer, no registry keys, and no avptools**: only its folder, inside your game's folder,
named `Tools`. (Old guides for Rebellion's tools set registry keys by hand. None of that is needed here.)

1. **Find your game's folder**, the one that holds `avp_rifs`:
   - Steam: `C:\Program Files (x86)\Steam\steamapps\common\Aliens versus Predator Classic`. In Steam: right-click the game, then
     **Manage** > **Browse local files**.
   - The original Fox install: `C:\Program Files (x86)\Fox\Aliens versus Predator` (`C:\Program Files\Fox\...` on 32-bit Windows).
2. **Make a folder named `Tools` there**, if there isn't one.
3. **Unzip Open Inteng into that `Tools` folder**, so the editor is at `<your game's folder>\Tools\AVP_OpenInteng.exe`.
4. **Open a level** with it: see "Opening a level file with the editor" below.

The folders, when it's set up:

```
Aliens versus Predator Classic\      your game's folder: the one DataFolder= names, if you ever set it
    avp_rifs\                        the game's levels (.rif): the editor opens and saves these
        OpenedByEditor\              levels opened with "Open with" are copied here first (the editor's own)
    avp_huds\                        the game's HUD files and models
    FMVs\                            the game's message videos: Video mode saves yours here
    Tools\                           Open Inteng's folder
        AVP_OpenInteng.exe
        AVP_OpenInteng.ini
        shape_rifs\                  object shapes for Quick-create (avptools' own folder works here), if you have them
```

**Somewhere else?** If the editor isn't in the game's `Tools` folder, tell it where the game is: open `AVP_OpenInteng.ini` and put
`DataFolder=<your game's folder>` under `[Editor]` (see "Its settings" below). Without that, it stops and says so.

## How it's built

- **The game's own code, never a copy.** The editor compiles AvPx's own level loader, chunk library and renderer, from the `avpx/`
  submodule. That's the AvPx port of the game, pinned to one commit. So the editor reads a level exactly as the game does, and every
  fix to the game's reader reaches the editor when the pin moves.
  - Nothing inside `avpx/` is ever edited here.
  - A pin moves only to an AvPx commit that passed all of AvPx's checks.
- **Everything else is ours:** the editor's window and loop, its UI (our own, no outside UI library), its modes and its save path.
- **No precompiled code** is in this repository. The libraries it uses are built from pinned sources.

## Its settings (AVP_OpenInteng.ini)

The zip has an `AVP_OpenInteng.ini` beside the editor. Put the editor in your game's own `Tools` folder (`<the game>\Tools`, as inteng
was) and there is nothing to set: it reads the game's files from the folder above it, the one that holds `avp_rifs`. A plain Steam
install works as it is.

- `DataFolder=` under `[Editor]` names that folder yourself: your game's folder or a copy of it, the one that holds `avp_rifs`. Add it
  when the editor is anywhere else. A `DataFolder=` line always wins: one that can't be used stops the editor, saying why.
- `Level=` is read only by hidden test runs (`-picture`), as the level they open. With you at the screen it is never read: see
  "Opening a level file" below.

- `StartWidth=` and `StartHeight=` are the size the window starts at, inside its frame: 1280 x 720 in the zip. They're kept to at least
  640 x 480 and at most your monitor.
- `Windowed=true` keeps the window framed, as the command line's `-w` does. With `false`, a size as big as the monitor covers it whole,
  as classic inteng's full screen does.
- `AutoPlot=1` shows the Waypoint screen's Alien auto waypoint menu, which plots the alien waypoints for you: on in the zip. `0` hides it.
- What wins: inteng's own 640 x 480, then `StartWidth` and `StartHeight`, then the command line's `-r<width>x<height>`, then a classic
  `IntengOptions.ini` (beside the editor, or in the Tools folder `IntengTools=` names), whose `Screen Width` and `Screen Height` win, as in
  inteng. The editor never writes to `IntengOptions.ini`.

## Opening a level file with the editor ("Open with")

The editor opens a level named on its command line: `AVP_OpenInteng.exe <file.rif>`. Started with none, it does what inteng does: it
says "Please load a rif file by double clicking on it", and Ok ends it. To open `.rif` files from Windows' own menus, set it up once by
hand. **The editor never writes to the registry**: Windows keeps the choice itself.

1. Right-click a `.rif` file and choose **Open with**, then **Choose another app**.
2. Choose **More apps**, then **Look for another app on this PC**, and pick `AVP_OpenInteng.exe`.
3. To have Windows open every `.rif` file this way, tick **Always use this app to open .rif files** before you choose it.

What happens to the file:

- It's copied into the editor's data folder (the game's folder above `Tools`, or `DataFolder=`), into its `avp_rifs\OpenedByEditor`, under its
  own name, and opened from there. Only the editor puts files in that folder. The file you picked is only read.
- Only a regular `.rif` file of at most 16 MB is taken: not a folder, a link or a device.
- Its name, without `.rif`, has to be one a game file could have: plain letters and digits, and `_ - . + & @` and spaces. Otherwise
  the editor asks you to rename it.
- A file opened before under the same name is replaced by the new copy, since that folder is the editor's own. Opening that copy
  itself simply opens it.

## Your own videos (mp4-to-smk)

AvP plays its in-game videos (the messages on a level's monitors) as Smacker `.smk` files. The editor makes them itself: Mode > Video.
`mp4-to-smk.exe` is no longer in the zip. It can still be built from this project's source (`win32/VS2026/mp4-to-smk.vcxproj`), and
turns your own video into one in AvP's own format:

```
mp4-to-smk.exe <input video> <output.smk> [--fit | --crop] [--overwrite] [--level N | --level source] [--max-seconds N] [--max-input-mb N]
```

- **In:** `.mp4`, `.m4v`, `.mov`, `.mkv`, `.webm`, `.avi`, `.wmv`, `.mpg`, `.mpeg`, `.vob`, `.ts`, `.m2ts` or `.3gp`, read by Windows' own Media Foundation. Whether a
  video decodes depends on the codecs your Windows has: one it can't decode is named in words, with the Microsoft extension that adds it
  (for example "HEVC picture: Microsoft's HEVC Video Extension adds it"). By default at most 256 MB and 180 seconds, and 4096 pixels a side;
  `--max-input-mb` (up to 2048) and `--max-seconds` (up to 600) take more. A video too big for the program's memory is refused in words.
- **Out, always:** 128 x 96 at 15 frames a second, sound at 22,050 Hz, 8-bit, mono: the format of AvP's own message videos.
- **A video that isn't 4:3:** it asks whether to resize it to fit (the whole picture, with black bars) or crop it to fill (the edges
  cut). `--fit` or `--crop` answers first.
- **Sound:** made as loud as AvP's own videos (`--level`, 1 to 128; the default is their median) and never clipped. `--level source`
  keeps your video's own level.
- **An existing output** is refused unless you add `--overwrite`.

**Putting it in the game:**

1. Convert it: `mp4-to-smk.exe myvideo.mp4 message417.smk` (the name is `message` and the Message strategy's number, 1 to 450: the
   editor refuses a higher one).
2. Copy the `.smk` into the `FMVs` folder of your game's folder, beside AvP's own `message*.smk` files.
3. In the level, the Message strategy with that number plays it when it fires. The editor's Message view lists the videos it finds.

**How a level plays it.** A Message strategy, when it fires, plays `FMVs\message<n>.smk` from the game's folder, where `<n>` is
the Message's own number. A level's monitor is a 128 x 128 picture with `FMVs` in its name, and it shows those message videos.

- For your own level, give its Message a number no stock level uses, and name your file after it. Replacing a stock
  `messageNNN.smk` changes that stock level too. The release notes (https://github.com/Melanikus/AVP_OpenInteng/releases) list the free
  numbers: 417 to 450 and the other numbers no stock level uses.
- A monitor shows nothing when a video named after its own picture exists in `FMVs\`, and no video plays while a cheat is on.

**The encoder** is libsmackerenc, made from Barry Duncan's libsmackerdec (LGPL 2.1), and it is linked into the editor itself (its Video
mode makes films with it), and into `mp4-to-smk.exe` when you build that: there is no DLL beside either, and none is loaded. The editor's FMV preview has Barry's decoder, libsmackerdec (LGPL 2.1), linked into it the same
way. Each library's licence is in the zip's `licences` folder, and `licences/README.txt` links its whole source, at the very commit the
release was built from (the sources are not in the zip, so each release scans fewer files on VirusTotal). To use your own
build of either, as the LGPL allows, build the program again from this project's source with yours in its place
(`win32/VS2026/libsmackerenc_static.vcxproj` and `libsmackerdec_static.vcxproj` build them).

## Animated objects from Blender: the track import

An animation baked in Blender becomes a set of **track objects** that move together in a level, one for each rigid part of the model
(an arm's segments, a mech's limbs). The game's own track code plays them, so nothing new runs in the game. It was proven on
2026-11-19 with a three-part arm in Colony (`Genshd1`), played in Steam AvP.

**What you need:**

- `track-import.exe`, a command-line tool built from this repository's `tools/track-import` with its `.args`, as the verifiers are.
  It isn't in the release zip yet.
- Blender, to bake the animation and write it as `anim.json` (below). The proof used a small Blender script that builds the arm and
  its IK, then bakes it.

**Steps:**

1. **Bake.** In Blender, bake each part's origin and turn at a fixed rate, in the level's units (millimetres, y down). The proof used
   15 keys a second. Make the last key the same pose as the first, so the animation loops. Write it as `anim.json`: the format is at
   the top of `tools/track-import/track-import.cpp`.
2. **Find a spot.** `track-import probe <level.rif> <x> <y> <z>` gives the floor below that point and the ceiling above it, so the
   model fits.
3. **Make the parts.** `track-import parts <level.rif> <out.rif> <source> <scale> <name> [...]` makes each part a NEW placed object,
   around a copy of an existing object's shape, rescaled. The level's own objects stay as they were.
4. **Import.** `track-import import <out.rif> <anim.json> <final.rif> [linear|smooth] [--pool-free-mib <MB>]` turns each part into a
   Track object (strategy 72). Its track has one section for each pair of keys, loops, and plays from the level's start, so all parts
   keep in step.
   - `linear` (the default) turns each part about its root joint.
   - `smooth` lets the game smooth the motion through the keys. The game then ignores the part's offset, so each part turns about
     its own origin.
   - `--pool-free-mib` is the level's free memory, as the editor's status line shows it when the level opens: on Rebellion's Colony,
     **Level memory: 6 of 40 blocks taken, 34.63 MB free**, so `--pool-free-mib 34.63`. The tracks are fitted into it the way the
     game packs them, and an import that wouldn't fit is refused, naming the first track that doesn't. Without it, the import keeps
     to 4 MB.
5. **Check.** `track-import check <final.rif> <anim.json>` plays every part together with the game's own track code, 100 loops at
   60 frames a second and again at an uneven frame rate. It reports:
   - the gap at each joint two parts share;
   - how far each part drifts from the bake;
   - the loop's seam;
   - how near any part comes to the player's start.
6. **Look in the editor.** Open `final.rif`, go to Object Placement and pick a part. **Object > Edit Track** opens it in Track
   Editing, and **Play Track** moves it. The editor plays one track at a time, as inteng does.
   `track-import posecopy <final.rif> <anim.json> <seconds> <copy.rif>` makes a still copy at any moment, for pictures.
7. **In the game.** Put the level in the game's `avp_rifs` folder, keeping a copy of the original, and play it. The parts move from
   the level's start, in step.

**Limits:**

- Each part is a rigid object. There's no bending skin, so a model is as many objects as it has parts.
- The shapes are copies of objects already in the level, rescaled. Blender's own models don't come in yet.
- The parts loop from the level's start. Nothing starts or stops them yet, but a switch could, as with any track object.
- A track's sections are one piece of the level's memory, so a track fits in one of its 1 MB blocks, and all the import's tracks must
  fit the level's free memory (`--pool-free-mib`, above). The tool refuses more, because the game's loader would trust the count.
- `anim.json` is read as hostile input: its size, depth, strings and counts all have bounds, and every number is checked before use.

## Where to read next

- `docs/TAKING-OVER.md`: how to pick this up with nobody to ask. What each check proves, and what to do when a tool is gone.
- `CODING-STANDARD.md`: the rules every change follows.
- `docs/avpx-files.md`: every AvPx file the editor compiles, and why.
- `License.txt`: Rebellion's terms, the notices of the code that came after, and ours (MIT).

## Rules this project keeps

1. **Security first.** Nothing merges without passing every scan. A finding is fixed, never hidden.
2. **Built to be picked up decades from now.** Plain-language comments, everything pinned, rebuildable on a clean machine.
3. **Input is hostile.** A level file from anyone is checked before it's trusted. The editor writes only the file you pick, plus one
   backup of it, and a copy of a level file you open with it in its own data folder ("Open with", above). It never uses a name from
   inside a level as a path.
