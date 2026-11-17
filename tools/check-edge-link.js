// check-edge-link.js: proves the editor's program took code from no library but the ones it's allowed. Written 2026-11-14 by Melanikus
// (Hicks) for AVP_OpenInteng (R-INT-15).
//
// WHY: the editor compiles most of the game but none of its network code (the edge, tools/make-edge.js). R-INT-15 asks for a gate
// that fails if the network's libraries are linked anyway: ENet (the game's network layer) and mlkem and mldsa (its post-quantum key
// exchange and signatures). This one goes further. Every library the program took any code from must be on the list below, so a
// library nobody reviewed can't come in quietly either. Adding one is a reviewed change to this file.
//
// HOW: the linker's map file (/MAP, set in win32/VS2026/AVP_OpenInteng.vcxproj) lists every function and piece of data in the program,
// each with the object it came from, written "library:object" when that object came out of a library. This reads those library names.
// A search for the words alone wouldn't do: the game's own names contain them ("DamageNetworkGhost" holds "enet").
//
// Usage, from the tree's root: node tools/check-edge-link.js <the program's .map> <the program's .exe>. Both must be inside the tree
// (tree-path.js): win32/VS2026/Win32/Release/AVP_OpenInteng.map and .exe.
// It answers 0 when every library is allowed, and 1 with the reasons when not.

'use strict';
const fs = require('fs');
const { insideTree } = require('./tree-path.js');

const [mapGiven, exeGiven] = process.argv.slice(2);
if (!mapGiven || !exeGiven) {
	console.error('usage: node tools/check-edge-link.js <program .map> <program .exe>, from the tree\'s root');
	process.exit(2);
}

// Both are held inside the tree's root, the folder this is run from (tree-path.js says why)
const mapPath = insideTree(mapGiven, 'The program\'s .map');
const exePath = insideTree(exeGiven, 'The program\'s .exe');

// Never, whatever else changes (R-INT-15): the editor never plays online. ws2_32 and wsock32 are Windows' own network sockets, which
// nothing in the editor has a reason to use either.
const kNever = /^(enet|mlkem|mldsa|ws2_32|wsock32)/i;

// Allowed, each with what it's for. The names are as the map writes them, with no ".lib".
const kAllowed = new Map([
	['MSVCRT', 'the C runtime\'s start-up code (/MD)'],
	['ucrt', 'the C runtime'],
	['vcruntime', 'the C++ runtime\'s base'],
	['msvcprt', 'the C++ standard library'],
	['kernel32', 'Windows itself'],
	['user32', 'windows, the keyboard and message boxes'],
	['advapi32', 'Windows itself (the C runtime takes it in)'],
	['shell32', 'folders such as the user\'s own (the game\'s save folder)'],
	['winmm', 'the game\'s timer'],
	['xinput', 'game pads, which the game\'s input code reads'],
	['d3d9', 'Direct3D 9, the game\'s renderer'],
	['d3dx9', 'Direct3D 9\'s helpers, the game\'s shaders and textures'],
	['OpenAL32', 'sound effects (R-INT-10, R-INT-15): built from the pinned AvPx source and scanned there'],
]);

// The map and the program must come from the same link: a map left over from an older link would prove nothing about this program
const mapTime = fs.statSync(mapPath).mtimeMs;
const exeTime = fs.statSync(exePath).mtimeMs;
if (Math.abs(mapTime - exeTime) > 60 * 1000) {
	console.error('check-edge-link.js: the map and the program are more than a minute apart, so they aren\'t from the same link: link again');
	process.exit(1);
}

// A line of the map's symbol table: "0001:00083280  ?Name@@...  00484280 f  library:object.obj" (the "f" and an "i" are optional)
const libraries = new Map();
let symbols = 0;
for (const line of fs.readFileSync(mapPath, 'latin1').split(/\r?\n/)) {
	const m = line.match(/^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+\S+\s+[0-9a-f]{8}(?:\s+[fi])*\s+(\S+)\s*$/i);
	if (!m) continue;
	symbols++;
	const from = m[1];
	const colon = from.indexOf(':');
	if (colon <= 0) continue; // one of the program's own objects
	const library = from.slice(0, colon);
	libraries.set(library, (libraries.get(library) || 0) + 1);
}

// A map with no symbols at all means its layout changed, or it's empty: that proves nothing, so it fails
if (symbols === 0) {
	console.error('check-edge-link.js: no symbols found in ' + mapPath + ': the map\'s layout isn\'t one this script reads, so nothing is proven');
	process.exit(1);
}

const never = [...libraries.keys()].filter(l => kNever.test(l)).sort();
const unknown = [...libraries.keys()].filter(l => !kNever.test(l) && !kAllowed.has(l)).sort();
for (const l of never) console.error('check-edge-link.js: NEVER ALLOWED: ' + l + ' (' + libraries.get(l) + ' symbols). The editor links no network code (R-INT-15).');
for (const l of unknown) console.error('check-edge-link.js: not on the list: ' + l + ' (' + libraries.get(l) + ' symbols). Add it to kAllowed, with its reason, in a reviewed change.');
if (never.length || unknown.length) process.exit(1);

console.log('check-edge-link.js: ' + symbols + ' symbols; every library is allowed: ' +
	[...libraries.entries()].sort().map(([l, n]) => l + ' ' + n).join(', '));
