# Coding standard

**This repository follows AvPx's own standard,** `avpx/CODING-STANDARD.md`, as of the pinned commit. When the pin moves, the standard
moves with it. It's not copied here, so the two can never drift apart.

## The editor's own additions

1. **Text from a level is hostile** (names, notes, anything read from a file):
   - it's held in bounded buffers, with its length checked;
   - it's shown as plain text, never used as a format string (no `printf(name)`, always `printf("%s", name)` or a plain-text call).
   - **Every text widget has a test with hostile text:** too long, no terminating zero, control characters, `%n` and `%s`.
2. **The editor writes only the file the user picked,** plus its one-time `.bak`, through a temporary file, a flush and a replace.
   **No name from inside a level ever becomes a path.** One more write, for "Open with": a level opened from outside the editor's data
   folder is copied, as it is, into the editor's own `data\avp_rifs`, under its own base name, and only when that name passes the game's
   rule for level file names and no different file has it already (src/host/open_with.cpp; approved by Bishop, 2026-11-14). Nothing
   else is ever made from the path it came from.
3. **Every check the editor makes has a test that names its reason,** and a take-out row that the test catches.
4. **Nothing in `avpx/` is edited.** See `docs/host-symbols.md` for what the editor does instead.
5. **Plain-language comments,** as AvPx's standard says: a stranger reading it decades from now should be able to follow it.
