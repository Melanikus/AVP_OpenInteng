// check-binaries.js: checks on the programs a release ships, so that the bytes scanned and shipped are provably only our source and one pinned Microsoft compiler.
// Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng (Bishop's order after alpha.3, whose mp4-to-smk.exe and libsmackerenc.dll were flagged by one scanner each and
// could not be shown to be rebuildable: "the same commit built here and on CI gives byte-identical binaries, or the release refuses"; Tao 1, Tao 2 rebuildable).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". It only reads the files: nothing is run, and every offset is checked first (Tao 3).
//
//   node tools/check-binaries.js same [--allow-clock] <folder A> <folder B> <file> [<file> ...]
//     Each file must have the same SHA-256 in both folders: two builds of one commit, in two folders or on two machines. Where two differ, it says where.
//     --allow-clock is for a program linked WITHOUT /Brepro (alpha.3's editor, mp4-to-smk, fmt and OpenAL): such a link writes the time and a random GUID into the
//     file, so two builds can never be the same bytes. The file then counts as the same only if EVERY differing byte is one of those fields: the header's time
//     stamp and checksum, the debug directory's time stamps, the .pdb record's GUID and age, the export and resource directories' time stamps, and the compiler's
//     __DATE__ ("Nov 22 2026") and __TIME__ ("00:53:40") texts. Each such field is named. Any other differing byte fails the file: it is not ours until shown to be.
//     Exit 0 all the same, 1 not.
//
//   node tools/check-binaries.js toolset <pin file> <file> [<file> ...]
//     Each file must have been linked by the pinned toolset. A Windows program records which tools made it: the PE header's linker version, and the "Rich" header
//     Microsoft's linker writes, one entry per tool and tool build that made the objects in it. The pin file (tools/toolset-pin.json) names the linker version and
//     every tool build allowed: the pinned compiler's own, the C runtime objects that ship with it, and the fixed builds of the Windows SDK's libraries. Any other
//     build in a file (another compiler, another runtime, another SDK) refuses it. Exit 0 all pinned, 1 not, 2 a file or the pin that can't be read.
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// The parts of a PE file this needs. Every offset is checked against the file's size before it is used (the file is hostile input).
function readPe(file) {
	const b = fs.readFileSync(file);
	const fail = why => ({ file, error: why });
	const u16 = o => (o >= 0 && o + 2 <= b.length ? b.readUInt16LE(o) : -1);
	const u32 = o => (o >= 0 && o + 4 <= b.length ? b.readUInt32LE(o) : -1);

	if (b.length < 0x40 || b.toString('latin1', 0, 2) !== 'MZ') return fail('not a Windows program (no MZ)');
	const pe = u32(0x3c);
	if (pe < 0x40 || pe + 24 > b.length || b.toString('latin1', pe, pe + 4) !== 'PE\0\0') return fail('no PE header');
	const coff = pe + 4, sections = u16(coff + 2), optSize = u16(coff + 16), opt = coff + 20;
	if (opt + optSize > b.length || sections < 0 || sections > 96) return fail('a header that does not fit the file');
	const magic = u16(opt);
	if (magic !== 0x10b && magic !== 0x20b) return fail('an unknown optional header');
	const dirs = opt + (magic === 0x20b ? 112 : 96);
	const dirCount = u32(opt + (magic === 0x20b ? 108 : 92));

	const secs = [];
	for (let s = 0; s < sections; s++) {
		const h = opt + optSize + s * 40;
		if (h + 40 > b.length) return fail('a section table past the end');
		secs.push({ name: b.toString('latin1', h, h + 8).replace(/\0+$/, ''), va: u32(h + 12), vsize: u32(h + 8), raw: u32(h + 20), rawSize: u32(h + 16) });
	}
	const toOffset = rva => {
		for (const s of secs) if (rva >= s.va && rva < s.va + Math.max(s.vsize, s.rawSize)) return s.raw + (rva - s.va);
		return -1;
	};
	const dir = n => (n < dirCount && dirs + n * 8 + 8 <= b.length ? { rva: u32(dirs + n * 8), size: u32(dirs + n * 8 + 4) } : { rva: 0, size: 0 });

	// the fields a link without /Brepro fills from the clock or at random: [offset, length, words]
	const clock = [[coff + 4, 4, 'the time stamp in the header'], [opt + 64, 4, 'the checksum in the header']];
	const exp = dir(0), res = dir(2), dbg = dir(6);
	if (exp.rva > 0 && toOffset(exp.rva) >= 0) clock.push([toOffset(exp.rva) + 4, 4, 'the export directory\'s time stamp']);
	if (res.rva > 0 && toOffset(res.rva) >= 0) clock.push([toOffset(res.rva) + 4, 4, 'the resource directory\'s time stamp']);
	let codeview = null;
	const d0 = dbg.rva > 0 ? toOffset(dbg.rva) : -1;
	for (let o = d0; d0 >= 0 && o + 28 <= d0 + dbg.size && o + 28 <= b.length; o += 28) {
		clock.push([o + 4, 4, 'a debug directory entry\'s time stamp']);
		if (u32(o + 12) !== 2) continue;   // IMAGE_DEBUG_TYPE_CODEVIEW
		const at = u32(o + 24);
		if (at > 0 && at + 24 <= b.length && b.toString('latin1', at, at + 4) === 'RSDS') {
			const z = b.indexOf(0, at + 24);
			codeview = { at, end: z > 0 ? z : at + 24 };
			clock.push([at + 4, 20, 'the .pdb record\'s GUID and age']);
		}
	}

	// the Rich header: before the PE header, "Rich" and a key; XOR-decoded back to its start, "DanS"
	const rich = [];
	const end = b.indexOf('Rich', 0x40, 'latin1');
	if (end > 0 && end + 8 <= pe) {
		const key = u32(end + 4);
		let start = -1;
		for (let o = end - 4; o >= 0x40; o -= 4) if (((u32(o) ^ key) >>> 0) === 0x536e6144) { start = o; break; }
		for (let o = start + 16; start > 0 && o + 8 <= end; o += 8) {
			const id = (u32(o) ^ key) >>> 0;
			rich.push({ product: id >>> 16, build: id & 0xffff, count: (u32(o + 4) ^ key) >>> 0 });
		}
	}

	return { file, bytes: b, linker: b[opt + 2] + '.' + b[opt + 3], clock, rich, secs, codeview };
}

function sha256(buffer) {
	return crypto.createHash('sha256').update(buffer).digest('hex');
}

// __DATE__ is "Mmm dd yyyy", __TIME__ "hh:mm:ss": the text around offset i in a, if it is one, as [start, length]
function compilerTimeText(bytes, i) {
	const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
	for (let s = Math.max(0, i - 10); s <= i; s++) {
		const date = bytes.toString('latin1', s, s + 11), time = bytes.toString('latin1', s, s + 8);
		if (s + 11 <= bytes.length && months.includes(date.slice(0, 3)) && /^[A-Z][a-z]{2} [ 0-3][0-9] [12][0-9]{3}$/.test(date)) return [s, 11];
		if (s + 8 <= bytes.length && /^[0-2][0-9]:[0-5][0-9]:[0-5][0-9]$/.test(time)) return [s, 8];
	}
	return null;
}

// Where two builds of one file differ. allowClock: the clock fields (and the compiler's date and time texts) are let through, each named; anything else is "other bytes".
function compare(a, b, allowClock) {
	if (a.error || b.error) return { same: false, words: [a.error || b.error] };
	if (a.bytes.length !== b.bytes.length) return { same: false, words: [`the sizes differ (${a.bytes.length} and ${b.bytes.length} bytes)`] };
	const allowed = [], other = [];
	for (let i = 0; i < a.bytes.length; i++) {
		if (a.bytes[i] === b.bytes[i]) continue;
		const field = a.clock.find(([at, n]) => i >= at && i < at + n);
		if (field) { if (!allowed.includes(field[2])) allowed.push(field[2]); continue; }
		const text = compilerTimeText(a.bytes, i);
		if (text && compilerTimeText(b.bytes, i)) {
			const words = `the compiler's date or time text at offset ${text[0]} ("${a.bytes.toString('latin1', text[0], text[0] + text[1])}")`;
			if (!allowed.includes(words)) allowed.push(words);
			i = text[0] + text[1] - 1;
			continue;
		}
		other.push(i);
	}
	const words = [];
	if (other.length) {
		const sec = a.secs.find(s => other[0] >= s.raw && other[0] < s.raw + s.rawSize);
		words.push(`${other.length} other bytes differ, the first at offset ${other[0]} (${sec ? sec.name : 'the headers'})`);
	}
	if (allowed.length) words.push((allowClock ? 'let through: ' : 'differ: ') + allowed.join('; '));
	return { same: other.length === 0 && (allowClock || allowed.length === 0), words };
}

function same(allowClock, folderA, folderB, files) {
	let bad = 0;
	for (const name of files) {
		const fa = path.join(folderA, name), fb = path.join(folderB, name);
		if (!fs.existsSync(fa) || !fs.existsSync(fb)) { console.log(`  FAILED  ${name}: not in both folders`); bad++; continue; }
		const ha = sha256(fs.readFileSync(fa)), hb = sha256(fs.readFileSync(fb));
		if (ha === hb) { console.log(`  ok  ${name}: byte for byte the same, SHA-256 ${ha}`); continue; }
		const r = compare(readPe(fa), readPe(fb), allowClock);
		console.log(`  ${r.same ? 'ok' : 'FAILED'}  ${name}: ${ha.slice(0, 16)} and ${hb.slice(0, 16)}: ${r.words.join('; ')}`);
		bad += r.same ? 0 : 1;
	}
	console.log(bad ? `check-binaries same: ${bad} of ${files.length} DIFFER` : `check-binaries same: all ${files.length} the same${allowClock ? ' (the clock fields let through)' : ''}`);
	return bad ? 1 : 0;
}

function toolset(pinFile, files) {
	let pin;
	try {
		pin = JSON.parse(fs.readFileSync(pinFile, 'utf8'));
	} catch (e) {
		console.log(`check-binaries toolset: the pin file ${pinFile} can't be read: ${e.message}`);
		return 2;
	}
	if (typeof pin.linker !== 'string' || !Array.isArray(pin.richBuilds) || !pin.richBuilds.every(n => Number.isInteger(n) && n >= 0 && n <= 0xffff)) {
		console.log('check-binaries toolset: the pin file needs "linker" (such as "14.51") and "richBuilds" (whole numbers)');
		return 2;
	}
	const allowed = new Set(pin.richBuilds);
	let bad = 0, unreadable = 0;
	for (const file of files) {
		const pe = readPe(file);
		if (pe.error) { console.log(`  FAILED  ${file}: ${pe.error}`); unreadable++; continue; }
		const others = [...new Set(pe.rich.filter(e => !allowed.has(e.build)).map(e => e.build))];
		const problems = [];
		if (pe.linker !== pin.linker) problems.push(`linked by ${pe.linker}, not ${pin.linker}`);
		if (pe.rich.length === 0) problems.push('no Rich header (not linked by Microsoft\'s linker)');
		if (others.length) problems.push(`tool builds not pinned: ${others.join(', ')}`);
		console.log(problems.length ? `  FAILED  ${file}: ${problems.join('; ')}` : `  ok  ${file}: linker ${pe.linker}, every tool build pinned`);
		bad += problems.length ? 1 : 0;
	}
	const pinWords = `${pin.msvc || 'the pin'} (linker ${pin.linker})`;
	console.log(bad + unreadable ? `check-binaries toolset: ${bad + unreadable} of ${files.length} NOT built by ${pinWords}` : `check-binaries toolset: all ${files.length} built by ${pinWords}`);
	return unreadable ? 2 : bad ? 1 : 0;
}

const args = process.argv.slice(2);
if (args[0] === 'same') {
	const allowClock = args[1] === '--allow-clock';
	const rest = args.slice(allowClock ? 2 : 1);
	if (rest.length >= 3) process.exit(same(allowClock, rest[0], rest[1], rest.slice(2)));
}
if (args[0] === 'toolset' && args.length >= 3) process.exit(toolset(args[1], args.slice(2)));
console.log('usage: node tools/check-binaries.js same [--allow-clock] <folder A> <folder B> <file>...  |  node tools/check-binaries.js toolset <pin file> <file>...');
process.exit(2);
