// read-shot-context-test.js: the test of tools/read-shot-context.js (GitHub #78). Each refusal has a check of its own, on a picture made wrong in one way (the picture is hostile, Tao 3), as
// tests/host_png_meta_test.cpp checks png_meta.cpp. Written 2026-11-17 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Use, from the repository's root: node tools/read-shot-context-test.js. It prints "ok" or "FAILED" for each check and gives back 1 if any failed. It writes one file in the system's temporary
// folder and deletes it.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const tool = require('./read-shot-context.js');

let failed = 0;

function check(good, what) {
	console.log(`  ${good ? 'ok' : 'FAILED'}  ${what}`);
	failed += good ? 0 : 1;
}

// the PNG standard's CRC, bit by bit (the tool uses a table: two ways to one answer)
function bitCrc(bytes) {
	let crc = 0xffffffff;

	for (const b of bytes) {
		crc ^= b;

		for (let k = 0; k < 8; k++) {
			crc = (crc & 1) ? ((crc >>> 1) ^ 0xedb88320) : (crc >>> 1);
		}
	}

	return (crc ^ 0xffffffff) >>> 0;
}

function chunk(type, data) {
	const body = Buffer.concat([Buffer.from(type, 'latin1'), data]);
	const head = Buffer.alloc(4);
	const tail = Buffer.alloc(4);

	head.writeUInt32BE(data.length);
	tail.writeUInt32BE(bitCrc(body));
	return Buffer.concat([head, body, tail]);
}

function ourData(text, flag = 0, method = 0, language = '') {
	return Buffer.concat([Buffer.from('AVP_OpenInteng', 'latin1'), Buffer.from([0, flag, method]), Buffer.from(language, 'latin1'), Buffer.from([0, 0]), Buffer.isBuffer(text) ? text : Buffer.from(text, 'utf8')]);
}

function beforeEnd(png, extra) {
	return Buffer.concat([png.subarray(0, png.length - 12), extra, png.subarray(png.length - 12)]);
}

function refusal(bytes) {
	try {
		tool.readContext(bytes);
		return '';
	} catch (e) {
		return e instanceof tool.Refusal ? e.message : `THREW ${e}`;
	}
}

const png = Buffer.from([
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
	0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89,
	0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4,
	0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82]);

check(tool.crc32(png.subarray(12, 29)) === 0x1f15c489 && bitCrc(png.subarray(37, 51)) === 0x0d0a2db4, "the sample picture's CRCs, by the tool's table and by this test's bits");
check(/no iTXt chunk/.test(refusal(png)), `a picture with no context: "${refusal(png)}"`);

const context = { format: 1, level: 'Genshd1', camera: [-27851, -14055, 85500], note: 'café "quoted"' };
const good = beforeEnd(png, chunk('iTXt', ourData(JSON.stringify(context))));
let read = null;

try {
	read = tool.readContext(good);
} catch (e) {
	read = null;
}

check(read !== null && JSON.stringify(read) === JSON.stringify(context), 'the context is read back as written');

const cases = [
	['not a PNG', Buffer.from('hello'), /PNG signature/],
	['a corrupt CRC', (() => { const b = Buffer.from(good); b[png.length - 12 + 8 + 20] ^= 1; return b; })(), /wrong CRC/],
	['cut short in the context chunk', good.subarray(0, good.length - 20), /past the end|cut short/],
	['cut just before IEND', good.subarray(0, good.length - 12), /no IEND/],
	['a second context chunk', beforeEnd(good, chunk('iTXt', ourData('{"format": 1}'))), /second context chunk/],
	['an over-long text', beforeEnd(png, chunk('iTXt', ourData(`{"format": 1, "x": "${'a'.repeat(256 * 1024)}"}`))), /over the 262144 allowed/],
	['compressed', beforeEnd(png, chunk('iTXt', ourData('{"format": 1}', 1, 0))), /compressed/],
	['a language tag', beforeEnd(png, chunk('iTXt', ourData('{"format": 1}', 0, 0, 'en'))), /language tag/],
	['not UTF-8', beforeEnd(png, chunk('iTXt', ourData(Buffer.from([0x7b, 0x22, 0x61, 0x22, 0x3a, 0x22, 0xff, 0x22, 0x7d])))), /not UTF-8/],
	['a zero byte', beforeEnd(png, chunk('iTXt', ourData(Buffer.from('{"format": 1, "a": "x\u0000y"}', 'utf8')))), /zero byte/],
	['not JSON', beforeEnd(png, chunk('iTXt', ourData('hello'))), /not well-formed JSON/],
	['a JSON array', beforeEnd(png, chunk('iTXt', ourData('[1, 2]'))), /not one JSON object/],
	['nested 65 deep', beforeEnd(png, chunk('iTXt', ourData(`{"format": 1, "a": ${'['.repeat(64)}${']'.repeat(64)}}`))), /nested more than 64/],
	['another format', beforeEnd(png, chunk('iTXt', ourData('{"format": 2}'))), /knows format 1/],
	['a byte after IEND', Buffer.concat([good, Buffer.from([0])]), /follow the IEND/],
	['a chunk before IHDR', Buffer.concat([png.subarray(0, 8), chunk('tEXt', Buffer.from('a\u0000b', 'latin1')), png.subarray(8)]), /not IHDR of 13/],
	['a length past 2^31', (() => { const b = Buffer.from(good); b[png.length - 12] = 0xff; return b; })(), /past the PNG standard|past the end/],
	['a type that is not letters', (() => { const b = Buffer.from(good); b[png.length - 12 + 4] = 0x31; return b; })(), /not four letters/],
];

for (const [what, bytes, says] of cases) {
	const words = refusal(bytes);

	check(says.test(words), `${what}: refused ("${words}")`);
}

let cuts = 0;
let refused = 0;

for (let n = 0; n < good.length; n++) {
	cuts++;
	refused += refusal(good.subarray(0, n)) !== '' ? 1 : 0;
}

check(refused === cuts, `cut short at every one of its ${cuts} lengths: refused each time (${refused})`);
check(refusal(beforeEnd(png, chunk('iTXt', ourData(`{"format": 1, "a": ${'['.repeat(63)}${']'.repeat(63)}}`)))) === '', 'nested 64 deep: taken');

// the program itself, on a file: the JSON laid out and 0; a refusal and 1
// It's run from the picture's folder, with the picture's plain name (the tool takes nothing outside the folder it's run from).
const name = `read-shot-context-test-${process.pid}.png`;
const file = path.join(os.tmpdir(), name);
const fromItsFolder = { encoding: 'utf8', cwd: os.tmpdir() };

fs.writeFileSync(file, good);

let printed = '';
let code = 0;

try {
	printed = execFileSync(process.execPath, [path.join(__dirname, 'read-shot-context.js'), name], fromItsFolder);
} catch (e) {
	code = e.status;
}

check(code === 0 && JSON.stringify(JSON.parse(printed)) === JSON.stringify(context), 'the program prints the context and gives back 0');

fs.writeFileSync(file, png);

try {
	printed = execFileSync(process.execPath, [path.join(__dirname, 'read-shot-context.js'), name], fromItsFolder);
	code = 0;
} catch (e) {
	code = e.status;
	printed = String(e.stdout);
}

check(code === 1 && /^refused: /.test(printed), `and a refusal with 1: "${printed.trim()}"`);

// a path that leads out of the folder it's run from is refused before anything is read
try {
	execFileSync(process.execPath, [path.join(__dirname, 'read-shot-context.js'), path.join('..', name)], { ...fromItsFolder, stdio: 'pipe' });
	code = 0;
} catch (e) {
	code = e.status;
}

check(code === 2, `a picture outside the folder it's run from is refused (exit ${code})`);
fs.unlinkSync(file);

console.log(failed ? `${failed} checks FAILED` : 'All checks passed');
process.exit(failed ? 1 : 0);
