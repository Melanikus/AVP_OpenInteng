// make-edge.js: writes the editor's "edge", from the linker's own list of what's missing. Written 2026-11-14 by Melanikus (Hicks) for
// AVP_OpenInteng (R-INT-15).
//
// WHY: the editor compiles most of the game (option A), but not the game's start, its network game, its network layer, its online pages
// or Hudson, the game's test player. The rest of the game still calls into those, so the linker names each such function and each such
// piece of data as missing. The edge is what the editor puts in their place, and this script writes all of it but one hand-written file:
//   - each network function: a stand-in that STOPS THE EDITOR, loudly, if it's ever reached (EdgeStop, src/host/edge_stop.cpp). The
//     editor never plays online, so none of them is ever a silent no-op (step 5);
//   - each piece of data: the game's own definition, copied word for word with its first value (step 6);
//   - each Hudson hook: "Hudson asleep", the answer its real definition gives with Hudson off, as he always is in the editor (step 7);
//   - the film decoders' and the music libraries' calls: by hand, in src/host/edge_media.cpp, which step 8 checks name for name.
//
// HOW, so a stranger can redo it at every pin move:
//   1. Build win32/VS2026/AVP_OpenInteng.vcxproj with /p:NoEdge=true, which leaves the whole edge out, and keep the build's log.
//   2. node tools/make-edge.js <that log> <avpx folder> src/host/edge
//   3. git diff src/host/edge, review every change, and build again without NoEdge: it must link.
//
// What it reads:
//   - the linker's "unresolved external symbol" lines, which give each name's exact signature as text;
//   - AvPx's own source, to find which file DEFINES each name, and how;
//   - AvPx's headers, to find which one DECLARES each name. Each file it writes includes its source file's own headers, so a stand-in
//     whose signature doesn't match the game's declaration fails to compile or to link. It can't slip through.
// It refuses to write anything if a name it should cover can't be placed or read: no guessing.

'use strict';
const fs = require('fs');
const path = require('path');

const [logPath, avpxDir, outPath] = process.argv.slice(2);
if (!logPath || !avpxDir || !outPath) {
	console.error('usage: node make-edge.js <link log of a NoEdge build> <avpx folder> <src/host/edge folder>');
	process.exit(2);
}

// The game's network files: the ones the editor doesn't compile, whose functions get loud stand-ins.
const networkFiles = [
	'3dc/avp/win95/pldnet.cpp', '3dc/networking.cpp', '3dc/net_close.cpp', '3dc/relay_list.cpp', '3dc/relay_list_text.cpp',
	'3dc/relay_auto.cpp', '3dc/net_host.cpp', '3dc/net_crypto_cng.cpp', '3dc/net_identity_store.cpp', '3dc/net_handshake_a.cpp',
	'3dc/servers_cfg.cpp', '3dc/servers_cfg_file.cpp', '3dc/servers_trust.cpp', '3dc/net_server_key.cpp', '3dc/net_pq_native.cpp',
	'3dc/pq_key_exchange.cpp', '3dc/pq_signature_check.cpp', '3dc/avp/win95/frontend/online_screens_page.cpp',
	'3dc/avp/win95/frontend/online_page.cpp', '3dc/avp/win95/frontend/server_yes_page.cpp', '3dc/avp/win95/winmain.cpp'];

// 1. The missing names, from the log. A C++ name's line gives its signature in quotes, then its decorated name in brackets.
const missing = new Map();
for (const line of fs.readFileSync(logPath, 'latin1').split(/\r?\n/)) {
	const m = line.match(/LNK20(01|19):[^"]*"([^"]+)" \((\?[^)\s]+)\)/);
	if (m) missing.set(m[3], m[2]);
}
if (missing.size === 0) {
	console.error('make-edge.js: no missing names in ' + logPath);
	process.exit(1);
}

// 2. Which network file defines each one. A definition is the name followed by "(" on a line that doesn't end in ";".
const nameOf = signature => {
	const m = signature.match(/([A-Za-z_][A-Za-z0-9_]*)\s*\(/);
	return m ? m[1] : null;
};
const definedIn = new Map();
for (const rel of networkFiles) {
	const text = fs.readFileSync(path.join(avpxDir, rel), 'latin1');
	for (const line of text.split(/\r?\n/)) {
		const m = line.match(/^[A-Za-z_][\w\s\*&:<>,]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*$/);
		if (m && !definedIn.has(m[1])) definedIn.set(m[1], rel);
	}
}

// The edge's two hand-written files, beside the generated folder: edge_media.cpp (the film and music libraries' calls) and edge_own.cpp
// (the editor's own versions of the few functions the game keeps in its network files but calls in a game played alone). Each name
// either one defines is answered there, by a person, so it's never generated; step 8 checks both files name for name.
const handFiles = ['edge_media.cpp', 'edge_own.cpp'].map(f => path.join(outPath, '..', f));
const handNames = new Map();
for (const file of handFiles) {
	for (const line of fs.readFileSync(file, 'latin1').split(/\r?\n/)) {
		const m = codeOf(line, true).match(/^[A-Za-z_][\w\s\*&:<>,]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*$/);
		if (m) handNames.set(m[1], path.basename(file));
	}
}

// 3. Keep the functions defined in a network file, but not those answered by hand. A decorated name with "@@3" or "@@2" right after
//    its name is data, not a function: data never gets a stand-in that stops: it gets the game's own definition, with its first value
//    (step 6).
const functions = [];
for (const [decorated, signature] of missing) {
	if (/^\?[A-Za-z_][A-Za-z0-9_]*@@[23]/.test(decorated)) continue;
	const name = nameOf(signature);
	if (name && definedIn.has(name) && !handNames.has(name)) functions.push({ decorated, signature, name, file: definedIn.get(name) });
}
functions.sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0);

// A log from a build that already has the stand-ins names none of the network functions, and would leave old files in place as if they
// were current. That's always the wrong log, so it's refused.
if (functions.length === 0) {
	console.error('make-edge.js: this log names no network function. It must come from a build WITHOUT src/host/edge (step 1 above)');
	process.exit(1);
}

// 4. The headers that declare them, found among AvPx's own headers. Every function has to be declared in one, or nothing is written.
const headers = [];
const walk = dir => {
	for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
		const full = path.join(dir, entry.name);
		if (entry.isDirectory()) { if (!/^(tests|dependencies|vcpkg|\.git)$/.test(entry.name)) walk(full); }
		else if (/\.(h|hpp)$/i.test(entry.name)) headers.push(full);
	}
};
walk(path.join(avpxDir, '3dc'));
const headerText = headers.map(h => [h, fs.readFileSync(h, 'latin1')]);
const declaredIn = new Set();
const undeclared = [];
for (const f of functions) {
	const re = new RegExp('\\b' + f.name + '\\s*\\(');
	const hit = headerText.find(([, text]) => re.test(text));
	if (hit) declaredIn.add(path.basename(hit[0])); else undeclared.push(f.name);
}
// A function no header declares is declared with "extern" inside the files that call it, as the game's older code often does. Its
// decorated name in the log comes from that caller's own declaration, so a stand-in written from the same text still has to match it
// exactly to link. They're listed at the top of the file, so a reviewer can see which ones rest on the linker alone.
if (undeclared.length) {
	console.log('make-edge.js: ' + undeclared.length + ' declared only inside their callers: ' + undeclared.join(', '));
}

// A game file's own #include lines, in its own order, for the files below. An include that sits inside an #if keeps its #if, #elif,
// #else and #endif lines: hudson.cpp, for one, includes build_commit.h only when that file exists (hudson.cpp:228), and without its #if
// the include fails wherever the file isn't there. Only the #if blocks that hold an include are copied. A branch is written out the
// moment an include turns up in it, with every #if/#elif/#else line before it in that block, so the include stays under its own branch.
// net_version_check.h is a compile-time check of the network messages' sizes, which pldnet.cpp includes only after its own table of
// them (pldnet.cpp:1205, then :1275). It means nothing here, and can't compile without that table, so it's left out.
function ownIncludes(rel) {
	const open = []; // the file's #if blocks not yet closed: the lines seen so far, and whether they're written yet
	let out = '';
	for (const line of fs.readFileSync(path.join(avpxDir, rel), 'latin1').split(/\r?\n/)) {
		const directive = line.match(/^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b/);
		if (directive) {
			const word = directive[1];
			if (word.startsWith('if')) { open.push({ lines: [line.trim()], written: false }); continue; }
			if (open.length === 0) continue;
			const block = open[open.length - 1];
			if (block.written) out += line.trim() + '\n';
			else block.lines.push(line.trim());
			if (word === 'endif') open.pop();
			continue;
		}
		const m = line.match(/^\s*#\s*include\s+("[^"]+"|<[^>]+>)/);
		if (!m || /net_version_check\.h/.test(m[1])) continue;
		for (const block of open) {
			if (!block.written) { out += block.lines.join('\n') + '\n'; block.written = true; }
		}
		out += '#include ' + m[1] + '\n';
	}
	if (open.length) {
		console.error('make-edge.js: ' + rel + ' ends with ' + open.length + ' #if block(s) still open; its includes are not copied');
		process.exit(1);
	}
	return out;
}

// 5. One stand-in file per network file, so each one's stand-ins compile with exactly that file's own #include lines, in its own
//    order: the game's headers need their prerequisites in the order the real file gives them, and two files' headers don't always
//    mix. The signature is the linker's own, with "__cdecl" kept. Each stand-in stops the editor with its own name.
//    outPath is a folder; each file in it is named after its network file.
//    Nothing is written until step 8 has checked the whole list: until then each file waits in "pending".
const pending = [];
let written = 0;
for (const rel of networkFiles) {
	const mine = functions.filter(f => f.file === rel);
	if (mine.length === 0) continue;

	let out = '';
	out += '// GENERATED by tools/make-edge.js from the linker\'s own list: do not edit by hand. Regenerate and diff it at every pin move.\n';
	out += '//\n';
	out += '// Stand-ins for the functions of the game\'s ' + rel + ' that the rest of the game calls, which the editor never runs: it\n';
	out += '// never plays online (R-INT-15). Each one stops the editor with its own name if it is ever reached, and does nothing else. The\n';
	out += '// includes are that file\'s own, in its own order.\n';
	const here = undeclared.filter(n => mine.some(f => f.name === n));
	if (here.length) {
		out += '//\n// Declared in no AvPx header, only with "extern" inside the files that call them, so the linker alone checks that these match:\n';
		for (let i = 0; i < here.length; i += 6) out += '//   ' + here.slice(i, i + 6).join(', ') + '\n';
	}
	out += '\n';
	out += ownIncludes(rel);
	out += '#include "edge_stop.h"\n\n';
	for (const f of mine) {
		// The linker writes each type with its "struct", "enum", "class" or "union" in front. A type the game declares as a typedef of
		// a struct with no name of its own can't be written that way, so the words are left out: the file's own includes declare
		// every type, and the decorated name the linker checks is the same either way.
		const signature = f.signature.replace(/\(void\)$/, '()').replace(/\b(struct|enum|class|union) (?=[A-Za-z_])/g, '');
		out += signature + '\n{\n\tEdgeStop("' + f.name + '");\n}\n\n';
	}
	const file = path.join(outPath, 'edge_' + path.basename(rel, '.cpp') + '.cpp');
	pending.push([file, out]);
	console.log('make-edge.js: ' + mine.length + ' stand-ins for ' + file);
	written += mine.length;
}
if (written !== functions.length) {
	console.error('make-edge.js: wrote ' + written + ' of ' + functions.length + ' stand-ins');
	process.exit(1);
}

// 6. The game's data the rest of the game reads, defined in a file the editor doesn't compile: copied WORD FOR WORD from its real
//    definition, first value included, so the editor starts from exactly the state the game starts from. A definition is the line that
//    names it (not "extern") up to its closing ";" outside any braces. Hudson's data comes from Hudson's own files the same way: its first
//    values are what "Hudson asleep" means. One file per source file, with that file's own includes (net_version_check.h left out, as
//    above).
const dataFiles = networkFiles.concat(fs.readdirSync(path.join(avpxDir, '3dc/avp'))
	.filter(f => /^hudson.*\.cpp$/.test(f)).map(f => '3dc/avp/' + f));
const dataNames = [];
for (const [decorated, signature] of missing) {
	if (!/^\?[A-Za-z_][A-Za-z0-9_]*@@[23]/.test(decorated)) continue;
	const m = signature.match(/([A-Za-z_][A-Za-z0-9_]*)\s*$/);
	if (m) dataNames.push(m[1]);
}
const dataBySource = new Map();
const dataNotFound = [];
for (const name of dataNames) {
	let found = null;
	for (const rel of dataFiles) {
		const lines = fs.readFileSync(path.join(avpxDir, rel), 'latin1').split(/\r?\n/);
		const start = lines.findIndex(l => !/^\s*(\/\/|extern\b)/.test(l) &&
			new RegExp('^[A-Za-z_][\\w\\s\\*&:<>,]*\\b' + name + '\\b\\s*(\\[[^\\]]*\\])?\\s*(=|;)').test(l));
		if (start < 0) continue;
		let depth = 0, end = start;
		for (; end < lines.length; end++) {
			for (const ch of lines[end].replace(/\/\/.*$/, '')) { if (ch === '{') depth++; else if (ch === '}') depth--; }
			if (depth <= 0 && /;\s*(\/\/.*)?$/.test(lines[end])) break;
		}
		found = { rel, text: lines.slice(start, end + 1).join('\n') };
		break;
	}
	if (!found) { dataNotFound.push(name); continue; }
	if (!dataBySource.has(found.rel)) dataBySource.set(found.rel, []);
	dataBySource.get(found.rel).push({ name, text: found.text });
}
if (dataNotFound.length) {
	console.error('make-edge.js: no definition found for the data ' + dataNotFound.join(', ') + '; the data files are not written');
	process.exit(1);
}
for (const [rel, items] of dataBySource) {
	let out = '';
	out += '// GENERATED by tools/make-edge.js: do not edit by hand. Regenerate and diff it at every pin move.\n//\n';
	out += '// The data of the game\'s ' + rel + ' that the rest of the game reads, copied word for word from its real definitions, first\n';
	out += '// values included, so the editor starts from the same state as the game. The editor doesn\'t compile ' + rel + ' itself.\n\n';
	out += ownIncludes(rel);
	out += '\n';
	for (const item of items.sort((a, b) => a.name < b.name ? -1 : 1)) out += '// ' + rel + '\n' + item.text + '\n\n';
	const file = path.join(outPath, 'data_' + path.basename(rel, '.cpp') + '.cpp');
	pending.push([file, out]);
	console.log('make-edge.js: ' + items.length + ' data for ' + file);
}

// A line of C++ with its "//" comment cut off. With blank set, what's inside each string or character literal becomes spaces too, so the
// shapes below and the braces counted are the code's own, never a "{" or a "?" inside some text. The length stays the same either way,
// so a place found in the blanked line is the same place in the real one.
function codeOf(line, blank) {
	let out = '';
	let quote = null;
	for (let i = 0; i < line.length; i++) {
		const ch = line[i];
		if (quote) {
			if (ch === '\\') { out += blank ? '  ' : ch + (line[i + 1] || ''); i++; continue; }
			if (ch === quote) { quote = null; out += ch; continue; }
			out += blank ? ' ' : ch;
			continue;
		}
		if (ch === '"' || ch === '\'') { quote = ch; out += ch; continue; }
		if (ch === '/' && line[i + 1] === '/') break;
		out += ch;
	}
	return out;
}

// The place of the "}" that closes the "{" at open, in blanked text, or -1.
function closingBrace(blanked, open) {
	let depth = 0;
	for (let i = open; i < blanked.length; i++) {
		if (blanked[i] === '{') depth++;
		else if (blanked[i] === '}' && --depth === 0) return i;
	}
	return -1;
}

// 7. Hudson asleep (R-INT-15: "Hudson's hooks do exactly what the game does with Hudson asleep, each with a test"). The editor's
//    HudsonRunIsOn is the game's own first value, false (step 6). So every Hudson hook the rest of the game calls gets the answer its
//    REAL definition gives when HudsonRunIsOn is false, copied from that definition. Only four shapes are read. Anything else stops
//    the generator by name, so a hook whose "off" answer isn't plain to see is never guessed:
//      a. it starts (after any "static" lines) with  if (!HudsonRunIsOn || ...) { ... }  : the answer is what that block does. The
//         block may only empty a list it was handed and then return a value with no call in it;
//      b. its whole body is  return HudsonRunIsOn ? A : B;  or  return (HudsonRunIsOn && ...) ? A : B;  : the answer is "return B;";
//      c. its whole body is  return HudsonRunIsOn && ...;  with no "||" and no "?", in a bool hook: the answer is "return false;";
//      d. a void hook whose whole body (after any "static" lines) is  if (HudsonRunIsOn && ...) { ... }  with no "||" in that
//         condition: the answer is to do nothing.
//    "||" is the one operator that could hide a second way in, which is why (c) and (d) refuse it and (a) can't be fooled by it: in (a)
//    a false HudsonRunIsOn makes the whole condition true, whatever follows. Each answer keeps the real definition's own first lines,
//    under that file's own includes, with the real line it came from copied above it for the reader. tests/edge_asleep_test.cpp checks
//    every answer against the real function by hand, apart from this script.
const hudsonFiles = fs.readdirSync(path.join(avpxDir, '3dc/avp')).filter(f => /^hudson.*\.cpp$/.test(f)).map(f => '3dc/avp/' + f);
const hudsonSource = new Map(hudsonFiles.map(rel => [rel, fs.readFileSync(path.join(avpxDir, rel), 'latin1').split(/\r?\n/)]));

// The definition of name in one of Hudson's files: its first lines up to a line holding only "{", then its body. A first line that
// turns out to end in ";" before any "{" is a declaration, and the search goes on past it.
function hudsonDefinition(name) {
	const starts = new RegExp('^[A-Za-z_][\\w\\s\\*&:<>,]*?\\b' + name + '\\s*\\(');
	for (const [rel, lines] of hudsonSource) {
		for (let start = 0; start < lines.length; start++) {
			if (!starts.test(lines[start])) continue;
			let open = start;
			while (open < lines.length && lines[open].trim() !== '{' && !/;\s*$/.test(codeOf(lines[open], true))) open++;
			if (open >= lines.length || lines[open].trim() !== '{') continue;
			const blanked = lines.slice(open).map(l => codeOf(l, true)).join('\n');
			const close = closingBrace(blanked, blanked.indexOf('{'));
			if (close < 0) return null;
			const lineCount = blanked.slice(0, close).split('\n').length;
			return { rel, head: lines.slice(start, open).join('\n'), headLine: start + 1, body: lines.slice(open + 1, open + lineCount - 1),
				bodyLine: open + 2 };
		}
	}
	return null;
}

// The "off" answer of one definition, as the lines to write, or a reason it can't be read.
function asleepAnswer(def) {
	const real = def.body.map(l => codeOf(l, false)).join('\n');
	const blanked = def.body.map(l => codeOf(l, true)).join('\n');
	let at = 0;
	const skipSpace = () => { while (at < blanked.length && /\s/.test(blanked[at])) at++; };
	skipSpace();
	while (blanked.startsWith('static', at) && /\s/.test(blanked[at + 6] || '')) {
		const semi = blanked.indexOf(';', at);
		if (semi < 0) return { why: 'a "static" line with no ";"' };
		at = semi + 1;
		skipSpace();
	}
	const rest = blanked.slice(at).trimEnd();
	const restReal = real.slice(at, at + rest.length);
	const deciding = def.body.findIndex(l => codeOf(l, true).trim() && !/^\s*static\s/.test(codeOf(l, true)));
	const evidence = { line: def.bodyLine + deciding, text: def.body[deciding].trim() };
	const isVoid = /^void\s/.test(def.head);
	const isBool = /^bool\s/.test(def.head);

	// A "/* */" comment isn't cut away like a "//" one. It needs no rule of its own: a hook with one where the shapes below look, at
	// its start, in its condition or in its "off" block, matches none of them and is refused (tests/make_edge_test.js shows one).

	// b and c: the whole body is one return
	const ret = rest.match(/^return\s+([\s\S]*);$/);
	if (ret && rest.indexOf(';') === rest.length - 1) {
		const exprStart = rest.indexOf(ret[1]);
		const expr = ret[1];
		const exprReal = restReal.slice(exprStart, exprStart + expr.length);
		const asks = expr.split('?').length - 1;
		const colon = expr.indexOf(':');
		if (asks === 1 && expr.split(':').length === 2 && colon > expr.indexOf('?') &&
			(/^HudsonRunIsOn\s*\?/.test(expr) || /^\(\s*HudsonRunIsOn\s*&&[^()?:|]*\)\s*\?/.test(expr))) {
			return { lines: ['return ' + exprReal.slice(colon + 1).trim() + ';'], evidence };
		}
		// No "," either: "return HudsonRunIsOn && a, b;" gives b, whatever HudsonRunIsOn is
		if (/^HudsonRunIsOn\s*&&/.test(expr) && !/\|\||\?|,/.test(expr)) {
			return isBool ? { lines: ['return false;'], evidence } : { why: '"return HudsonRunIsOn && ..." in a hook that isn\'t bool' };
		}
		return { why: 'a return this script doesn\'t read' };
	}

	// a and d: an "if" first. Neither condition may hold a ",": "if (!HudsonRunIsOn || a, b)" asks only about b.
	const ifOff = rest.match(/^if\s*\(\s*!\s*HudsonRunIsOn\s*(\|\|[^{}?,]*)?\)\s*\{/);
	const ifOn = rest.match(/^if\s*\(\s*HudsonRunIsOn\s*&&([^{}?|,]*)\)\s*\{/);
	const ifMatch = ifOff || ifOn;
	if (!ifMatch) return { why: 'it doesn\'t start with HudsonRunIsOn' };
	const open = ifMatch[0].length - 1;
	const close = closingBrace(rest, open);
	if (close < 0) return { why: 'its first block never closes' };
	if (ifOff) {
		const inner = rest.slice(open + 1, close);
		if (!/^\s*(?:[A-Za-z_]\w*\.clear\(\)\s*;\s*)*return\b[^;()]*;\s*$/.test(inner)) {
			return { why: 'its "off" block does more than empty a list and return a plain value' };
		}
		// Cut into statements where the code has a ";", found in the blanked text, so a ";" inside some text stays inside it
		const lines = [];
		let from = open + 1;
		for (let i = open + 1; i < close; i++) {
			if (rest[i] !== ';') continue;
			lines.push(restReal.slice(from, i + 1).trim());
			from = i + 1;
		}
		return { lines, evidence };
	}
	if (!isVoid) return { why: 'an "if (HudsonRunIsOn && ...)" hook that isn\'t void' };
	if (rest.slice(close + 1).trim()) return { why: 'more follows its "if (HudsonRunIsOn && ...)" block' };
	return { lines: [], evidence };
}

const asleepBySource = new Map();
const asleepRefused = [];
const asleepNames = new Set();
for (const [decorated, signature] of missing) {
	if (/^\?[A-Za-z_][A-Za-z0-9_]*@@[23]/.test(decorated)) continue;
	const name = nameOf(signature);
	// A hook the editor answers by hand (edge_own.cpp, which says why) isn't written asleep
	if (!name || definedIn.has(name) || handNames.has(name)) continue;
	const def = hudsonDefinition(name);
	if (!def) continue;
	const answer = asleepAnswer(def);
	if (answer.why) { asleepRefused.push(name + ' (' + def.rel + ':' + def.headLine + ': ' + answer.why + ')'); continue; }
	if (!asleepBySource.has(def.rel)) asleepBySource.set(def.rel, []);
	asleepBySource.get(def.rel).push({ name, def, answer });
	asleepNames.add(name);
}
if (asleepRefused.length) {
	console.error('make-edge.js: ' + asleepRefused.length + ' Hudson hooks whose "off" answer this script can\'t read, so nothing is written:');
	for (const r of asleepRefused) console.error('  ' + r);
	process.exit(1);
}
for (const [rel, items] of asleepBySource) {
	let out = '';
	out += '// GENERATED by tools/make-edge.js: do not edit by hand. Regenerate and diff it at every pin move.\n//\n';
	out += '// Hudson asleep: the hooks of the game\'s ' + rel + ' that the rest of the game calls, each answering exactly what its real\n';
	out += '// definition answers when HudsonRunIsOn is false, as it always is in the editor (R-INT-15). Above each one is the real line its\n';
	out += '// answer comes from. tests/edge_asleep_test.cpp checks every answer. The editor doesn\'t compile ' + rel + ' itself.\n\n';
	out += ownIncludes(rel);
	out += '\n';
	for (const { def, answer } of items.sort((a, b) => a.name < b.name ? -1 : 1)) {
		out += '// ' + rel + ':' + answer.evidence.line + ': ' + answer.evidence.text + '\n';
		out += def.head + '\n{\n' + answer.lines.map(l => '\t' + l + '\n').join('') + '}\n\n';
	}
	const file = path.join(outPath, 'asleep_' + path.basename(rel, '.cpp') + '.cpp');
	pending.push([file, out]);
	console.log('make-edge.js: ' + items.length + ' Hudson hooks asleep for ' + file);
}

// 8. The whole list, each name answered exactly once. Every missing name is now a network stand-in (step 5), the game's data (step 6),
//    or a Hudson hook asleep (step 7). The rest must be answered by hand: in src/host/edge_media.cpp, the film decoders' and the music
//    libraries' calls, because each one's answer needs a person to read the real library; in src/host/edge_own.cpp, the few functions
//    the game keeps in its network files but calls in a game played alone. Each file says why for each name. Together they must define
//    exactly the names left: one missing fails this, and so does one there that the game no longer calls, so the edge can't keep a
//    stand-in nothing needs. The C names (vorbisfile's and pthreads') come in the log without quotes, as the
//    linker's own name with the "_" Win32 puts in front of every C name.
//    A name that starts with "__imp_" is a call into a DLL, which the game's headers mark as imported (OpenAL's are). It means the link
//    is missing that DLL's library, and the answer is to link it (R-INT-15 links OpenAL), never a stand-in. So it's refused, with that
//    reason.
const covered = new Set([...functions.map(f => f.name), ...dataNames, ...asleepNames]);
const left = new Set();
const imported = new Set();
for (const [decorated, signature] of missing) {
	if (decorated.startsWith('__imp_')) { imported.add(decorated); continue; }
	const name = nameOf(signature) || (signature.match(/([A-Za-z_][A-Za-z0-9_]*)\s*$/) || [])[1];
	if (name && !covered.has(name)) left.add(name);
}
for (const line of fs.readFileSync(logPath, 'latin1').split(/\r?\n/)) {
	if (!/LNK20(01|19):/.test(line)) continue;
	// A C++ call into a DLL comes with quotes, like any C++ name, but its bracketed name starts with "__imp_" (step 1 skips it)
	const cppImport = line.match(/"[^"]+" \((__imp_[^)\s]+)\)/);
	if (cppImport) { imported.add(cppImport[1]); continue; }
	if (line.includes('"')) continue;
	const m = line.match(/(?:symbol|resolvido)\s+(_[A-Za-z_][A-Za-z0-9_]*)\b/);
	if (!m) continue;
	if (m[1].startsWith('__imp_')) imported.add(m[1]);
	else left.add(m[1].slice(1));
}
if (imported.size) {
	console.error('make-edge.js: ' + imported.size + ' calls into a DLL whose library the link doesn\'t list (link the library; these are never stood in): ' +
		[...imported].sort().slice(0, 8).join(', ') + (imported.size > 8 ? ', ...' : ''));
	process.exit(1);
}
// The names left must be exactly the hand-written files' names (handNames, read before step 3)
const notStoodIn = [...left].filter(n => !handNames.has(n)).sort();
const notNeeded = [...handNames.keys()].filter(n => !left.has(n)).sort();
if (notStoodIn.length || notNeeded.length) {
	if (notStoodIn.length) console.error('make-edge.js: missing, with no answer anywhere (add it to edge_media.cpp or edge_own.cpp, or to a step above): ' + notStoodIn.join(', '));
	if (notNeeded.length) console.error('make-edge.js: written by hand, but the game no longer calls them (take them out): ' +
		notNeeded.map(n => n + ' (' + handNames.get(n) + ')').join(', '));
	process.exit(1);
}
const byHand = file => [...handNames.values()].filter(f => f === file).length;
console.log('make-edge.js: every missing name answered once: ' + functions.length + ' network, ' + dataNames.length + ' data, ' +
	asleepNames.size + ' Hudson asleep, ' + byHand('edge_media.cpp') + ' film and music in edge_media.cpp, ' + byHand('edge_own.cpp') +
	' the editor\'s own in edge_own.cpp');

// 9. Only now, with the whole list answered, the files are written. The project takes the folder whole, so a file this script wrote
//    last time and doesn't write now must go: every .cpp in it that starts with this script's own "GENERATED" line is deleted first.
//    A file without that line is left alone, and it's a mistake the build will show, because nothing else belongs in that folder.
fs.mkdirSync(outPath, { recursive: true });
for (const entry of fs.readdirSync(outPath)) {
	const file = path.join(outPath, entry);
	if (/\.cpp$/.test(entry) && fs.readFileSync(file, 'latin1').startsWith('// GENERATED by tools/make-edge.js')) fs.unlinkSync(file);
}
for (const [file, text] of pending) fs.writeFileSync(file, text);
console.log('make-edge.js: ' + pending.length + ' files written to ' + outPath);
