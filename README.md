# AVP_OpenInteng

A new map editor for **Aliens versus Predator**, for everything in a level except its geometry and textures (those are Blender's,
through BlendRIF). It rebuilds what Rebellion's own 2000 editor, "inteng", let a mapper do: place objects, NPCs and generators, give
them their behaviour, light the level, set its acoustics, lay out the AI's paths and waypoints, and set the level-wide settings.

**Aliens versus Predator was created by Rebellion, who own it.** The source code to Aliens vs Predator is copyright (c) 1999-2000
Rebellion, and this editor compiles part of it. It may not be used, in full or in part, for commercial purposes. A huge thank you to
Rebellion for the most awesome game. AVP_OpenInteng's own code is MIT (it belongs to everyone who loves this game); the game's code and
inteng's quoted words stay Rebellion's. See `License.txt`.

## Status

**Pre-release.** Its home is https://github.com/Melanikus/AVP_OpenInteng (private for now). Nothing is pushed there until the first
milestone. The first release will be `v1.0.0-beta.0`.

## How it's built

- **The game's own code, never a copy.** The editor compiles AvPx's own level loader, chunk library and renderer, from the `avpx/`
  submodule. That's the AvPx port of the game, pinned to one commit. So the editor reads a level exactly as the game does, and every
  fix to the game's reader reaches the editor when the pin moves.
  - Nothing inside `avpx/` is ever edited here.
  - A pin moves only to an AvPx commit that passed all of AvPx's checks.
- **Everything else is ours:** the editor's window and loop, its UI (our own, no outside UI library), its modes and its save path.
- **No precompiled code** is in this repository. The libraries it uses are built from pinned sources.

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
