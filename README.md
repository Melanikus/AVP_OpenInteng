# AVP_OpenInteng

The first public alpha of AVP_OpenInteng: a remake of **inteng**, Rebellion's own level editor for **Aliens versus Predator**, now
open source. Like inteng, it works on a level once its geometry is built: place objects, NPCs and generators, give them their
behaviour, map textures onto the polygons, light the level, set its sounds and acoustics, lay out the AI's paths and waypoints, link
its modules, and set the level-wide settings. It works exactly like inteng, minus bugs and... you can move the camera with your mouse.

**Aliens versus Predator was created by Rebellion, who own it.** The source code to Aliens vs Predator is copyright (c) 1999-2000
Rebellion, and this editor compiles part of it. It may not be used, in full or in part, for commercial purposes. A huge thank you to
Rebellion for the most awesome game. AVP_OpenInteng's own code is MIT (it belongs to everyone who loves this game); the game's code and
inteng's quoted words stay Rebellion's. See `License.txt`.

## Status

**Pre-release.** Its home is https://github.com/Melanikus/AVP_OpenInteng, a public repository. The first release is `v1.0.0-alpha.0`
(2026-11-18).

## How it's built

- **The game's own code, never a copy.** The editor compiles AvPx's own level loader, chunk library and renderer, from the `avpx/`
  submodule. That's the AvPx port of the game, pinned to one commit. So the editor reads a level exactly as the game does, and every
  fix to the game's reader reaches the editor when the pin moves.
  - Nothing inside `avpx/` is ever edited here.
  - A pin moves only to an AvPx commit that passed all of AvPx's checks.
- **Everything else is ours:** the editor's window and loop, its UI (our own, no outside UI library), its modes and its save path.
- **No precompiled code** is in this repository. The libraries it uses are built from pinned sources.

## Its settings (AVP_OpenInteng.ini)

The zip has an `AVP_OpenInteng.ini` beside the editor. Before the first start, add two lines under its `[Editor]`: `DataFolder=` (your
own copy of the game's data, the folder that holds `avp_rifs`) and `Level=` (the level to open when none is given, without `.rif`).

- `StartWidth=` and `StartHeight=` are the size the window starts at, inside its frame: 1280 x 720 in the zip. They're kept to at least
  640 x 480 and at most your monitor.
- `Windowed=true` keeps the window framed, as the command line's `-w` does. With `false`, a size as big as the monitor covers it whole,
  as classic inteng's full screen does.
- What wins: inteng's own 640 x 480, then `StartWidth` and `StartHeight`, then the command line's `-r<width>x<height>`, then a classic
  `IntengOptions.ini` (beside the editor, or in the Tools folder `IntengTools=` names), whose `Screen Width` and `Screen Height` win, as in
  inteng. The editor never writes to `IntengOptions.ini`.

## Opening a level file with the editor ("Open with")

The editor opens a level named on its command line: `AVP_OpenInteng.exe <file.rif>`. With none, it opens the one named by `Level=`
in `AVP_OpenInteng.ini`. To open `.rif` files from Windows' own menus, set it up once by hand. **The editor never writes to the
registry**: Windows keeps the choice itself.

1. Right-click a `.rif` file and choose **Open with**, then **Choose another app**.
2. Choose **More apps**, then **Look for another app on this PC**, and pick `AVP_OpenInteng.exe`.
3. To have Windows open every `.rif` file this way, tick **Always use this app to open .rif files** before you choose it.

What happens to the file:

- It's copied into the editor's own data folder (`DataFolder=` in `AVP_OpenInteng.ini`), into its `avp_rifs`, under its own name,
  and opened from there, as the game opens its levels. The file you picked is only read.
- Only a regular `.rif` file of at most 16 MB is taken: not a folder, a link or a device.
- Its name, without `.rif`, has to be one a game file could have: plain letters and digits, and `_ - . + & @` and spaces. Otherwise
  the editor asks you to rename it.
- If a different level is already in the editor's `avp_rifs` under that name, it's never replaced: the editor asks you to rename
  yours. The same file, opened again, is simply opened.

## Your own videos (mp4-to-smk)

AvP plays its in-game videos (the messages on a level's monitors) as Smacker `.smk` files. `mp4-to-smk.exe`, in the zip beside the
editor, turns your own video into one in AvP's own format:

```
mp4-to-smk.exe <input video> <output.smk> [--fit | --crop] [--overwrite] [--level N | --level source]
```

- **In:** `.mp4`, `.m4v`, `.avi` or `.wmv`, read by Windows' own Media Foundation. At most 256 MB, 180 seconds and 4096 pixels a side.
- **Out, always:** 128 x 96 at 15 frames a second, sound at 22,050 Hz, 8-bit, mono: the format of AvP's own message videos.
- **A video that isn't 4:3:** it asks whether to resize it to fit (the whole picture, with black bars) or crop it to fill (the edges
  cut). `--fit` or `--crop` answers first.
- **Sound:** made as loud as AvP's own videos (`--level`, 1 to 128; the default is their median) and never clipped. `--level source`
  keeps your video's own level.
- **An existing output** is refused unless you add `--overwrite`.

**Putting it in the game:**

1. Convert it: `mp4-to-smk.exe myvideo.mp4 message900.smk` (the name is `message` and the Message strategy's number).
2. Copy the `.smk` into the `FMVs` folder of your game's folder, beside AvP's own `message*.smk` files.
3. In the level, the Message strategy with that number plays it when it fires. The editor's Message view lists the videos it finds.

**How a level plays it.** A Message strategy, when it fires, plays `FMVs\message<n>.smk` from the game's folder, where `<n>` is
the Message's own number. A level's monitor is a 128 x 128 picture with `FMVs` in its name, and it shows those message videos.

- For your own level, give its Message a number no stock level uses, and name your file after it. Replacing a stock
  `messageNNN.smk` changes that stock level too.
- A monitor shows nothing when a video named after its own picture exists in `FMVs\`, and no video plays while a cheat is on.

**The encoder** is `libsmackerenc.dll`, built from Barry Duncan's libsmackerdec (LGPL 2.1; its licence and source are in the zip's
`licences` folder). The tool loads only the DLL shipped with it. To use your own build of it, as the LGPL allows, pass its SHA-256
with `--encoder-sha256 <hex>`.

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
