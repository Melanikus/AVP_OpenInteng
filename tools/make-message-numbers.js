// make-message-numbers.js: writes src/model/message_numbers_table.cpp, the kind of every message number the game plays (1 to 450), DERIVED from
// the game's own data, never typed in. Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng, Video mode (alpha.5; Bishop 2's split, Dietrich
// leading). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: a Message strategy's number N is both a film, FMVs\message<N>.smk, and a text line, the game's string N - 1 + TEXTSTRING_LEVELMSG_001
// (object_setup.cpp:1852; a mission objective's two numbers the same way, :3506 and :3512), and the game plays only 1 to 450 (strategy_chunk.cpp
// MostLevelMessages). A mapper's film must take a number no stock level uses. Each number is one of:
//   S  used by a stock level: a Message (strategy kind 73, its number at the payload's +20) or a mission objective (kind 70, +20 and +24) in one
//      of the stock levels names it, so its film plays there too;
//   F  a stock film no level uses: FMVs\message<N>.smk is Rebellion's, and no stock level names N;
//   C  free and clean: no stock level names it, no stock film, and its text line is Rebellion's placeholder or empty;
//   O  free, but its text line is an old Rebellion line, which the console prints when the film plays.
// Only the numbers and their kinds are written: no Rebellion text goes into our source (Bishop's rule).
//
// THE FOLDER MUST BE THE STOCK GAME: Rebellion's own files, each checked against Steam's own depot manifests (AvP-docs tools/bishop/msgnumbers
// verify-stock.js; 2026-11-22: AvP-builds-excluded\AvP-play-release, all 117 files equal). Never a mapper's own game folder: his edited levels
// are not what a player who gets his map has (2026-11-22: the owner's own Derelict names 1 and dropped 103, so the 11-21 lists, read there, said
// 103 was free; on stock it is Derelict's briefing film). A number a STOCK level uses is never offered as free.
//
// Use, FROM THE STOCK GAME'S FOLDER (the one holding language.txt, FMVs\ and avp_rifs\ with the stock levels), naming this file in the tree:
//   node <tree>\tools\make-message-numbers.js           writes <tree>\src\model\message_numbers_table.cpp
//   node <tree>\tools\make-message-numbers.js --check   writes nothing: says whether the table is what the data makes, byte for byte,
//                                                       and gives back 1 when it isn't
// NO PATH IS TYPED: a path on the command line is outside input (Tao 3), and until 2026-11-22 the tool took the game's folder that way, so Snyk
// Code's path rule (javascript/PT) found the typed path reaching three file calls (Bishop 2 and Drake, bishop-int's gate). So the tool opens only
// its own fixed names (language.txt, FMVs, avp_rifs) in the folder it is run from, the names that folder lists, and its own tree's files. The
// only word it takes is --check.
// THE --check CAN ONLY RUN WHERE THE GAME'S FILES ARE (the owner's PC): Rebellion's files never go on a public machine, so GitHub can't run it.
// GitHub's checks test the table's own consistency instead (tests/model_message_numbers_test.cpp). Run the --check by hand after any change to
// the table, the game's data or this tool, and say so in the hand-over.
//
// THE GAME'S FILES ARE HOSTILE (Tao 3): each is read whole within a size limit, every unpack is bounded and checked to give exactly the size
// its header says, every chunk's size must lie inside its parent, and a level that can't be read stops the whole run (no level is skipped,
// or its numbers would wrongly show as free). The numbers the game's own code fixes (the first level message's string, how many there are)
// are read from AvPx's sources here (avpx/3dc), never typed in; the decoder is the game's own (3dc/win95/huffman.cpp: MakeHuffmanDecodeTable
// :433, HuffmanDecode :491), ported line for line by Vasquez (2026-11-21, AvP-docs tools/vasquez lang-decode.js and stock-structure-survey.js).
'use strict';

const fs = require('fs');
const path = require('path');

const repository = path.join(__dirname, '..');
const tablePath = path.join(repository, 'src', 'model', 'message_numbers_table.cpp');
const avpx = path.join(repository, 'avpx');
const kLanguageMost = 1024 * 1024;        // language.txt is about 80 KB
const kLevelMost = 64 * 1024 * 1024;      // the biggest stock level is about 2.6 MB packed
const kUnpackedMost = 256 * 1024 * 1024;
const kLevelsMost = 200;                  // the stock game has 63

function refuse(why) {
	console.log('make-message-numbers: REFUSED: ' + why);
	process.exit(1);
}

// A file read whole, through ONE open handle (Vasquez's review, 2026-11-22: no gap between the check and the read): opened once, its kind and
// size taken from the open handle (fstat), and read through it, one byte past its size so a file that grows under the read is refused too. A link
// is refused first by its name as well (lstat), only as an extra: what decides is the handle's.
function readBounded(file, most, what) {
	let link;
	try { link = fs.lstatSync(file); } catch (e) { refuse(`no ${what} at ${file}`); }
	if (link.isSymbolicLink()) refuse(`${what} is a link, not a plain file: ${file}`);
	let fd;
	try { fd = fs.openSync(file, 'r'); } catch (e) { refuse(`${what} can't be opened: ${file}`); }
	try {
		const stat = fs.fstatSync(fd);
		if (!stat.isFile()) refuse(`${what} is not a plain file: ${file}`);
		if (stat.size > most) refuse(`${what} is ${stat.size} bytes, more than ${most}: ${file}`);
		const bytes = Buffer.alloc(stat.size + 1);
		let got = 0;
		for (;;) {
			const n = fs.readSync(fd, bytes, got, bytes.length - got, got);
			if (n === 0) break;
			got += n;
			if (got > stat.size) refuse(`${what} grew while it was read: ${file}`);
		}
		if (got !== stat.size) refuse(`${what} gave ${got} bytes where it said ${stat.size}: ${file}`);
		return bytes.subarray(0, got);
	} finally {
		fs.closeSync(fd);
	}
}

// A folder's names, or a plain refusal naming the folder when it is not there or not a folder (Vasquez's review: a missing FMVs\ or avp_rifs\ threw raw)
function folderNames(folder, what) {
	let stat;
	try { stat = fs.lstatSync(folder); } catch (e) { refuse(`no ${what} folder at ${folder}`); }
	if (!stat.isDirectory()) refuse(`${what} is not a folder: ${folder}`);
	try { return fs.readdirSync(folder); } catch (e) { refuse(`the ${what} folder can't be read: ${folder}`); }
}

// ---- AvPx's own numbers, from its sources
// TEXTSTRING_LEVELMSG_001's value: its place in the first enum TEXTSTRING_ID of langenum.h (the one the game builds: "#if (1)"), counting from
// TEXTSTRING_BLANK=0. How many level messages there are: the TEXTSTRING_LEVELMSG_<nnn> in that enum, which must be 001 to the last with no gap, and
// must be AvPx's MostLevelMessages (strategy_chunk.cpp) and the editor's kMostMessageNumber (src/model/smk_header.h).
function avpxNumbers() {
	const langenum = fs.readFileSync(path.join(avpx, '3dc', 'avp', 'langenum.h'), 'latin1').replace(/\r/g, '');
	const start = langenum.indexOf('enum TEXTSTRING_ID\n{');
	const end = langenum.indexOf('};', start);
	if (start < 0 || end < 0 || langenum.indexOf('#if (1)') < 0 || langenum.indexOf('#if (1)') > start) refuse('langenum.h has no first enum TEXTSTRING_ID under #if (1)');
	const names = langenum.slice(start + 'enum TEXTSTRING_ID\n{'.length, end).replace(/\/\/[^\n]*/g, '').replace(/\/\*[\s\S]*?\*\//g, '')
		.split(',').map((s) => s.trim()).filter((s) => s.length > 0);
	if (names[0] !== 'TEXTSTRING_BLANK=0') refuse('the enum does not start with TEXTSTRING_BLANK=0');
	if (names.some((n, i) => i > 0 && n.includes('='))) refuse('a later name of the enum sets its own value: its places would not be its values');
	const first = names.indexOf('TEXTSTRING_LEVELMSG_001');
	if (first < 0) refuse('the enum has no TEXTSTRING_LEVELMSG_001');
	let count = 0;
	while (names[first + count] === 'TEXTSTRING_LEVELMSG_' + String(count + 1).padStart(3, '0')) count++;
	if (names.some((n, i) => /^TEXTSTRING_LEVELMSG_/.test(n) && (i < first || i >= first + count))) refuse('the level messages are not one run 001 to the last');
	const strategy = fs.readFileSync(path.join(avpx, '3dc', 'avp', 'win95', 'strategy_chunk.cpp'), 'latin1');
	const most = /static int const MostLevelMessages = ([0-9]+);/.exec(strategy);
	if (!most || +most[1] !== count) refuse(`AvPx's MostLevelMessages (${most ? most[1] : 'not found'}) is not the ${count} level messages of langenum.h`);
	const header = fs.readFileSync(path.join(repository, 'src', 'model', 'smk_header.h'), 'latin1');
	const ours = /int32_t const kMostMessageNumber = ([0-9]+);/.exec(header);
	if (!ours || +ours[1] !== count) refuse(`the editor's kMostMessageNumber (${ours ? ours[1] : 'not found'}) is not the game's ${count}`);
	return { firstLevelMessage: first, most: count };
}

// The chunk names whose chunks hold other chunks: every RIF_IMPLEMENT_DYNCREATE of a class that comes from Chunk_With_Children (avpx/3dc)
function containerIds() {
	const files = [];
	(function walk(dir) {
		for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
			const p = path.join(dir, e.name);
			if (e.isDirectory()) walk(p); else if (/\.(cpp|hpp|h)$/i.test(e.name)) files.push(p);
		}
	})(path.join(avpx, '3dc'));
	const text = files.map((f) => fs.readFileSync(f, 'latin1')).join('\n');
	const base = {};
	for (const m of text.matchAll(/class\s+(\w+)\s*:\s*public\s+(\w+)/g)) base[m[1]] = m[2];
	const isContainer = (cls, depth = 0) => cls === 'Chunk_With_Children' || (depth < 20 && base[cls] && isContainer(base[cls], depth + 1));
	const ids = new Set();
	for (const m of text.matchAll(/RIF_IMPLEMENT_DYNCREATE\w*\s*\(\s*"(\w{8})"\s*,\s*(\w+)/g)) if (isContainer(m[2])) ids.add(m[1]);
	ids.add('REBINFF2');
	if (!ids.has('AVPSTRAT') && ids.size < 10) refuse('too few container chunks were found in avpx/3dc');
	return ids;
}

// ---- The game's Huffman package ("REBCRIF1", huffman.hpp: Identifier[8], CompressedDataSize, UncompressedDataSize, CodelengthCount[11],
// ByteAssignment[256], then the data), unpacked the game's way. Gives null when the package isn't one.
const MAX_DEPTH = 11, HEADER = 8 + 4 + 4 + 4 * MAX_DEPTH + 256;
function unpack(file) {
	if (file.length < HEADER) return null;
	const size = file.readInt32LE(12);
	const counts = []; for (let n = 0; n < MAX_DEPTH; n++) counts.push(file.readInt32LE(16 + 4 * n));
	const list = file.subarray(16 + 4 * MAX_DEPTH, HEADER);
	const dataBytes = file.length - HEADER;
	if (size < 0 || size > kUnpackedMost || size > dataBytes * 8) return null;
	let space = 0; for (let n = 0; n < MAX_DEPTH; n++) { if (counts[n] < 0) return null; space += counts[n] * 2 ** (MAX_DEPTH - 1 - n); }
	if (space < 2 ** MAX_DEPTH) return null;
	const table = new Int32Array(1 << MAX_DEPTH);
	(function makeTable() {
		let lenbits = 0, repcount = 1 << MAX_DEPTH, repspace = 1, thisdepth = 0, depthbit = 4, p = 255, o = 0, d = 0;
		for (;;) {
			do { lenbits++; depthbit <<= 1; repspace <<= 1; repcount >>= 1; } while (!(thisdepth = counts[d++]));
			do {
				let temp = p < 0 ? 0xff : (lenbits | (list[p--] << 8));
				let out = o >> 2, count = repcount;
				do { table[out] = temp; out += repspace; } while (--count);
				temp = depthbit;
				do { temp >>= 1; if (temp & 3) return; o ^= temp; } while (!(o & temp));
			} while (--thisdepth);
		}
	})();
	const words = Math.floor(dataBytes / 4);
	const out = Buffer.alloc(size);
	let dest = 0, available = 0, reserve = 0, wid = 0, bits = 0, resbits = 0, source = 0;
	do {
		available += wid;
		let fill = 31 - available;
		bits = (bits << fill) >>> 0;
		if (fill > reserve) {
			fill -= reserve;
			available += reserve;
			if (reserve) bits = ((bits >>> reserve) | (resbits << (32 - reserve))) >>> 0;
			resbits = source >= words ? 0 : file.readUInt32LE(HEADER + 4 * source++);
			reserve = 32;
		}
		bits = ((bits >>> fill) | (resbits << (32 - fill))) >>> 0;
		resbits = resbits >>> fill;
		reserve -= fill;
		available = 31;
		let entry = table[(bits >>> 1) & ((1 << MAX_DEPTH) - 1)];
		for (;;) {
			wid = entry & 0xff;
			available -= wid;
			if (!(available >= 0 && dest !== size)) break;
			bits = bits >>> wid;
			out[dest++] = (entry >> 8) & 0xff;
			entry = table[(bits >>> 1) & ((1 << MAX_DEPTH) - 1)];
		}
	} while (available > -32 && dest !== size);
	return dest === size ? out : null;
}

function unpackedOrPlain(raw, what) {
	const tag = raw.toString('latin1', 0, 8);
	if (tag === 'REBINFF2') return raw;
	if (tag !== 'REBCRIF1') refuse(`${what} is neither a packed (REBCRIF1) nor a plain (REBINFF2) Rebellion file`);
	const out = unpack(raw);
	if (!out) refuse(`${what} does not unpack to the size its header says`);
	return out;
}

// ---- The game's text: string i (from 1) is the text between the next pair of quote marks (language.cpp 91-111); string 0 is the empty one
function languageStrings(gameFolder) {
	const text = unpackedOrPlain(readBounded(path.join(gameFolder, 'language.txt'), kLanguageMost, 'language.txt'), 'language.txt').toString('latin1');
	if (!text.startsWith('REBINFF2')) refuse('language.txt does not start REBINFF2 once unpacked');
	const strings = [''];
	let at = 0;
	for (;;) {
		const a = text.indexOf('"', at);
		if (a < 0) break;
		const b = text.indexOf('"', a + 1);
		if (b < 0) break;
		strings.push(text.slice(a + 1, b));
		at = b + 1;
	}
	return strings;
}

// ---- The numbers the stock levels name: every AVPSTRAT chunk of kind 73 (its number at the payload's +20) or 70 (+20 and +24)
function numbersTheLevelsUse(gameFolder, containers) {
	const folder = path.join(gameFolder, 'avp_rifs');
	const levels = folderNames(folder, 'levels (avp_rifs)').filter((n) => /\.rif$/i.test(n)).sort();
	if (levels.length === 0 || levels.length > kLevelsMost) refuse(`${folder} has ${levels.length} levels`);
	const used = new Set();
	for (const name of levels) {
		const d = unpackedOrPlain(readBounded(path.join(folder, name), kLevelMost, name), name);
		(function walk(start, end, depth) {
			if (depth > 64) refuse(`${name}: chunks nested deeper than 64`);
			let at = start;
			while (at + 12 <= end) {
				const id = d.toString('latin1', at, at + 8);
				const size = d.readUInt32LE(at + 8);
				if (size < 12 || at + size > end) refuse(`${name}: a chunk at ${at} is ${size} bytes, which does not fit inside its parent`);
				if (containers.has(id)) walk(at + 12, at + size, depth + 1);
				else if (id === 'AVPSTRAT' && size - 12 >= 28) {
					const kind = d.readInt32LE(at + 12 + 8);
					if (kind === 73) used.add(d.readInt32LE(at + 12 + 20));
					if (kind === 70 && size - 12 >= 32) { used.add(d.readInt32LE(at + 12 + 20)); used.add(d.readInt32LE(at + 12 + 24)); }
				}
				at += size;
			}
		})(0, d.length, 0);
	}
	return { used, levels: levels.length };
}

function filmNumbers(gameFolder) {
	const films = new Set();
	for (const name of folderNames(path.join(gameFolder, 'FMVs'), 'films (FMVs)')) {
		const m = /^message([1-9][0-9]{0,3})\.smk$/i.exec(name);
		if (m) films.add(+m[1]);
	}
	return films;
}

// Rebellion's placeholder lines all start with these words; a free number whose line is one, or empty, prints nothing of Rebellion's story
const kPlaceholderStart = 'DUMMY LEVEL-SPECIFIC MSG';

function kindsFromData(gameFolder) {
	const { firstLevelMessage, most } = avpxNumbers();
	const strings = languageStrings(gameFolder);
	if (strings.length <= firstLevelMessage + most - 1) refuse(`language.txt has ${strings.length - 1} strings, fewer than the ${most} level messages need`);
	const { used, levels } = numbersTheLevelsUse(gameFolder, containerIds());
	const films = filmNumbers(gameFolder);
	let kinds = '';
	for (let n = 1; n <= most; n++) {
		const line = strings[firstLevelMessage + n - 1];
		if (used.has(n)) kinds += 'S';
		else if (films.has(n)) kinds += 'F';
		else if (line === '' || line.startsWith(kPlaceholderStart)) kinds += 'C';
		else kinds += 'O';
	}
	const above = [...used].filter((n) => n > most);
	return { kinds, most, levels, above };
}

// ---- The table, as C++: one string of kinds, 50 a line, each line's first and last number beside it
function tableText(kinds, most, levels) {
	const lines = [];
	for (let at = 0; at < kinds.length; at += 50) {
		const piece = kinds.slice(at, at + 50);
		lines.push(`	"${piece}"   // ${at + 1}-${at + piece.length}`);
	}
	const count = (k) => [...kinds].filter((c) => c === k).length;
	return [
		'// message_numbers_table.cpp: GENERATED by tools/make-message-numbers.js from the game\'s own data (language.txt, FMVs\\ and the ' + levels + ' stock',
		'// levels). Do not edit by hand: change the tool and run it again (its --check says whether this is what the data makes). AVP_OpenInteng\'s own',
		'// code: see License.txt, "AVP_OpenInteng\'s own code". Numbers and kinds only: no Rebellion text is in here.',
		'//',
		'// The kind of every message number 1 to ' + most + ', in order: S used by a stock level, F a stock film no level uses, C free and clean, O free',
		'// with an old Rebellion line (src/model/message_numbers.h). In all: ' + count('C') + ' C, ' + count('O') + ' O, ' + count('F') + ' F, ' + count('S') + ' S.',
		'#include "message_numbers.h"',
		'',
		'namespace model',
		'{',
		'	char const kMessageNumberKinds[] =',
		...lines.map((l, i) => i === lines.length - 1 ? l.replace(/"   \/\//, '";   //') : l),
		'}',
		''
	].join('\n');
}

// The one word taken is --check; anything else, a folder typed by habit included, is the usage (no word typed is ever used as a path)
const args = process.argv.slice(2);
if (args.length > 1 || (args.length === 1 && args[0] !== '--check')) {
	console.log('usage: run from the stock game\'s folder (language.txt, FMVs\\, avp_rifs\\): node <tree>\\tools\\make-message-numbers.js [--check]');
	process.exit(2);
}
const check = args.length === 1;
const { kinds, most, levels, above } = kindsFromData(process.cwd());
if (above.length) console.log(`make-message-numbers: note: stock levels name numbers past ${most}, which the game refuses: ${above.join(', ')}`);
const text = tableText(kinds, most, levels);
const counts = ['C', 'O', 'F', 'S'].map((k) => `${k} ${[...kinds].filter((c) => c === k).length}`).join(', ');
if (check) {
	const now = fs.existsSync(tablePath) ? fs.readFileSync(tablePath, 'utf8').replace(/\r/g, '') : '';
	if (now !== text) { console.log(`make-message-numbers: src/model/message_numbers_table.cpp is NOT what the data makes (${counts}, from ${levels} levels)`); process.exit(1); }
	console.log(`make-message-numbers: src/model/message_numbers_table.cpp is what the data makes (${counts}, from ${levels} levels)`);
} else {
	fs.writeFileSync(tablePath, text);
	console.log(`make-message-numbers: wrote src/model/message_numbers_table.cpp (${counts}, from ${levels} levels)`);
}
