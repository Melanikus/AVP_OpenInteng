// read-shot-context.js: prints the context a screenshot of AVP_OpenInteng carries inside its PNG (GitHub #78; src/io/png_meta.h writes it). Written 2026-11-17 by Melanikus (Dietrich) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Use: node tools/read-shot-context.js <picture.png>. Prints the context's JSON, laid out, and gives back 0; a refusal is one line saying why, and gives back 1 (2 for a wrong use).
// The picture is HOSTILE input (it can come from anyone's report), read as png_meta.cpp reads it: a plain file of at most 64 MB; the PNG signature; every chunk's length inside the file and
// below 2^31, its type four letters, its CRC right; IHDR first (13 bytes), IEND last (empty) with nothing after; exactly one iTXt with the keyword "AVP_OpenInteng"; flags 0 and 0, no language
// tag, no translated keyword; text at most 256 KB, UTF-8 with no zero byte, one JSON object; and this reader's own: its "format" is 1.
'use strict';

const fs = require('fs');

const KEYWORD = 'AVP_OpenInteng';
const MOST_TEXT = 256 * 1024;
const MOST_FILE = 64 * 1024 * 1024;
const MOST_DEPTH = 64;
const SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

const CRC_TABLE = (() => {
	const table = new Uint32Array(256);

	for (let n = 0; n < 256; n++) {
		let c = n;

		for (let k = 0; k < 8; k++) {
			c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
		}

		table[n] = c >>> 0;
	}

	return table;
})();

function crc32(bytes) {
	let crc = 0xffffffff;

	for (let i = 0; i < bytes.length; i++) {
		crc = CRC_TABLE[(crc ^ bytes[i]) & 0xff] ^ (crc >>> 8);
	}

	return (crc ^ 0xffffffff) >>> 0;
}

class Refusal extends Error {}

// The deepest nesting of a parsed JSON value (the object itself is 1)
function depthOf(value, depth) {
	if (depth > MOST_DEPTH) {
		return depth;
	}

	let deepest = depth;

	if (value !== null && typeof value === 'object') {
		for (const key of Object.keys(value)) {
			deepest = Math.max(deepest, depthOf(value[key], depth + 1));

			if (deepest > MOST_DEPTH) {
				break;
			}
		}
	}

	return deepest;
}

// The context of a PNG's bytes, as a parsed object; a Refusal with plain words for anything else
function readContext(bytes) {
	if (!Buffer.isBuffer(bytes) || bytes.length < 8 || !bytes.subarray(0, 8).equals(SIGNATURE)) {
		throw new Refusal('the bytes do not start with the PNG signature');
	}

	let at = 8;
	let count = 0;
	let ours = null;
	let ended = false;

	while (at < bytes.length) {
		if (bytes.length - at < 12) {
			throw new Refusal(`the chunk at byte ${at} is cut short: ${bytes.length - at} bytes left, a chunk takes at least 12`);
		}

		const length = bytes.readUInt32BE(at);
		const type = bytes.subarray(at + 4, at + 8).toString('latin1');

		if (!/^[A-Za-z]{4}$/.test(type)) {
			throw new Refusal(`the chunk at byte ${at} has a type that is not four letters`);
		}

		if (length > 0x7fffffff) {
			throw new Refusal(`the ${type} chunk at byte ${at} says it is ${length} bytes, past the PNG standard's 2^31 - 1`);
		}

		if (length > bytes.length - at - 12) {
			throw new Refusal(`the ${type} chunk at byte ${at} says it is ${length} bytes, past the end of the file`);
		}

		if (bytes.readUInt32BE(at + 8 + length) !== crc32(bytes.subarray(at + 4, at + 8 + length))) {
			throw new Refusal(`the ${type} chunk at byte ${at} has a wrong CRC`);
		}

		if (count === 0 && (type !== 'IHDR' || length !== 13)) {
			throw new Refusal(`the first chunk is ${type} of ${length} bytes, not IHDR of 13`);
		}

		const data = bytes.subarray(at + 8, at + 8 + length);

		if (type === 'iTXt' && data.length > KEYWORD.length && data.subarray(0, KEYWORD.length).toString('latin1') === KEYWORD && data[KEYWORD.length] === 0) {
			if (ours) {
				throw new Refusal(`a second context chunk at byte ${at} (the first at ${ours.at})`);
			}

			ours = { at, data };
		}

		count++;
		at += 12 + length;

		if (type === 'IEND') {
			if (length !== 0) {
				throw new Refusal('the IEND chunk is not empty');
			}

			if (at !== bytes.length) {
				throw new Refusal(`${bytes.length - at} bytes follow the IEND chunk`);
			}

			ended = true;
			break;
		}
	}

	if (!ended) {
		throw new Refusal('the file ends with no IEND chunk');
	}

	if (!ours) {
		throw new Refusal('the PNG holds no iTXt chunk with the keyword AVP_OpenInteng');
	}

	const after = KEYWORD.length + 1;

	if (ours.data.length - after < 4) {
		throw new Refusal('the context chunk is too short for its flags');
	}

	if (ours.data[after] !== 0 || ours.data[after + 1] !== 0) {
		throw new Refusal('the context chunk is compressed: ours never is');
	}

	if (ours.data[after + 2] !== 0 || ours.data[after + 3] !== 0) {
		throw new Refusal('the context chunk has a language tag or a translated keyword: ours has neither');
	}

	const text = ours.data.subarray(after + 4);

	if (text.length > MOST_TEXT) {
		throw new Refusal(`the text is ${text.length} bytes, over the ${MOST_TEXT} allowed`);
	}

	if (text.includes(0)) {
		throw new Refusal('the text holds a zero byte');
	}

	let words;

	try {
		words = new TextDecoder('utf-8', { fatal: true }).decode(text);
	} catch (e) {
		throw new Refusal('the text is not UTF-8');
	}

	let context;

	try {
		context = JSON.parse(words);
	} catch (e) {
		throw new Refusal('the text is not well-formed JSON');
	}

	if (context === null || typeof context !== 'object' || Array.isArray(context)) {
		throw new Refusal('the text is not one JSON object');
	}

	if (depthOf(context, 1) > MOST_DEPTH) {
		throw new Refusal(`the JSON is nested more than ${MOST_DEPTH} deep`);
	}

	if (context.format !== 1) {
		throw new Refusal(`the context's format is ${JSON.stringify(context.format)}; this reader knows format 1`);
	}

	return context;
}

// A plain file read whole, at most 64 MB
function readFileBytes(file) {
	let facts;

	try {
		facts = fs.lstatSync(file);
	} catch (e) {
		throw new Refusal(`the file cannot be read: ${file}`);
	}

	if (!facts.isFile() || facts.isSymbolicLink()) {
		throw new Refusal(`not a plain file: ${file}`);
	}

	if (facts.size > MOST_FILE) {
		throw new Refusal(`the file is over ${MOST_FILE} bytes: ${file}`);
	}

	return fs.readFileSync(file);
}

if (require.main === module) {
	if (process.argv.length !== 3) {
		console.log('use: node <tree>/tools/read-shot-context.js <picture.png>, run from the folder that holds the picture');
		process.exit(2);
	}

	// The picture is named from the folder this is run from, and held to it (tools/tree-path.js): each part a plain name, nothing that
	// leads out of it (Snyk Code, Path Traversal, the editor's 11-17 scan). So cd to the picture's folder first.
	const picture = require('./tree-path.js').insideTree(process.argv[2], 'the picture');

	try {
		console.log(JSON.stringify(readContext(readFileBytes(picture)), null, 2));
	} catch (e) {
		if (!(e instanceof Refusal)) {
			throw e;
		}

		console.log(`refused: ${e.message}`);
		process.exit(1);
	}
}

module.exports = { readContext, readFileBytes, crc32, Refusal };
