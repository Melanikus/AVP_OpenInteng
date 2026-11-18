// from-shot-test.js: the test of the shot reader (src/host/shot_read.cpp: the editor's -shot and the harness's from-shot), through the harness's --shot-to-scenario mode, which reads a shot with
// that same code. Each field the shot gives is held to its bounds (the picture is hostile, Tao 3): a picture made wrong in one way each, and one made right. Written 2026-11-18 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Use, from the repository's root, with all three inside it (tree-path.js): node tools/from-shot-test.js <the built drive_scenario.exe> <a folder holding a level> <that level's file name> (the level gives the shot's sha256; the
// check of a level's sha256 against the shot's is the editor's, acceptance-from-shot-2). It prints "ok" or "FAILED" for each check and gives back 1 if any failed. It writes its pictures into
// a new folder in the system's temporary folder and deletes it.
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');
const tool = require('./read-shot-context.js');
const { insideTree } = require('./tree-path.js');

let failed = 0;

function check(good, what) {
	console.log(`  ${good ? 'ok' : 'FAILED'}  ${what}`);
	failed += good ? 0 : 1;
}

if (process.argv.length < 5) {
	console.log('usage: node tools/from-shot-test.js <drive_scenario.exe> <a folder holding a level> <that level\'s file name>');
	process.exit(2);
}

// The three come from the command line, so they are held to the folder this is run from, the tree's root, as every tool's paths are (tree-path.js
// says how). The harness must be a drive_scenario.exe that is there as a plain file.
const harness = insideTree(process.argv[2], 'The harness');
const levelPath = insideTree(path.join(process.argv[3], process.argv[4]), 'The level');
const mapName = path.basename(levelPath);
const harnessFile = fs.statSync(harness, { throwIfNoEntry: false });

if (path.basename(harness).toLowerCase() !== 'drive_scenario.exe' || !harnessFile || !harnessFile.isFile()) {
	console.log(`The harness must be a drive_scenario.exe that is there: ${JSON.stringify(String(process.argv[2]))} isn't`);
	process.exit(2);
}

const sha = crypto.createHash('sha256').update(fs.readFileSync(levelPath)).digest('hex');

function chunk(type, data) {
	const body = Buffer.concat([Buffer.from(type, 'latin1'), data]);
	const head = Buffer.alloc(4);
	const tail = Buffer.alloc(4);

	head.writeUInt32BE(data.length);
	tail.writeUInt32BE(tool.crc32(body));
	return Buffer.concat([head, body, tail]);
}

// a 1 x 1 picture (read-shot-context-test.js's) with the context put before its IEND
const png = Buffer.from([
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
	0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89,
	0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4,
	0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82]);

function shot(jsonText) {
	const data = Buffer.concat([Buffer.from('AVP_OpenInteng', 'latin1'), Buffer.from([0, 0, 0, 0, 0]), Buffer.from(jsonText, 'utf8')]);

	return Buffer.concat([png.subarray(0, png.length - 12), chunk('iTXt', data), png.subarray(png.length - 12)]);
}

function context(changes) {
	return Object.assign({ format: 1, map: mapName, level_sha256: sha, screen: 'Object Placement', pos: { x: -27851, y: -14055, z: 85500 }, turn: 1024, look: -200, selected: ['switch3'] }, changes);
}

const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'from-shot-test-'));
let n = 0;

function run(jsonText) {
	const file = path.join(folder, `shot-${++n}.png`);

	fs.writeFileSync(file, shot(jsonText));

	const answer = spawnSync(harness, ['--shot-to-scenario', file], { encoding: 'latin1' });

	return { code: answer.status, out: `${answer.stdout || ''}${answer.stderr || ''}`.split('\r\n').join('\n') };   // the harness writes CR LF line ends
}

function refused(changes, words, what) {
	const r = run(JSON.stringify(context(changes)));

	check(r.code === 1 && r.out.includes(words), `${what}: refused, "${words}" (exit ${r.code}: ${r.out.trim().slice(0, 160)})`);
}

try {
	// the right one: the -at line it stands for
	const good = run(JSON.stringify(context({})));
	const name = mapName.replace(/\.rif$/i, '');

	check(good.code === 0, `a right shot is taken (exit ${good.code})`);
	check(good.out.includes(`start-args ${name} -at -27851 -14055 85500 1024 -200 "Object Placement"`), 'its -at line: the level by name, the camera (place, turn, look), the screen');
	check(good.out.includes('the object switch3 selected'), 'its first selected object');
	check(good.out.includes(`sha256 ${sha}`), 'its sha256, for the editor to check');

	const doors = run(JSON.stringify(context({ screen: 'Light Sourcing (with Doors)', selected: [] })));

	check(doors.code === 0 && doors.out.includes('"Light Sourcing (with Doors)"') && doors.out.includes('# nothing selected'), 'a screen\'s whole name, and nothing selected');

	// one field wrong each
	refused({ level_sha256: sha.toUpperCase() }, 'level_sha256 is not 64 lower-case hex', 'upper-case hex');
	refused({ level_sha256: '' }, 'level_sha256 is not 64 lower-case hex', 'no sha256 (the editor could not read the level)');
	refused({ map: '..\\..\\Windows\\win.ini' }, 'the map is not a plain file name', 'a map with folders in it');
	refused({ map: '..' }, 'the map is not a plain file name', 'the map ".."');
	refused({ map: 'a'.repeat(129) }, 'the map is not a plain file name', 'a map name of 129 bytes');
	refused({ map: 'Inévasion.RIF' }, 'the map is not a plain file name', 'a map name with a byte past ASCII');
	refused({ screen: '' }, 'the screen is not 1 to 64 printable', 'an empty screen');
	refused({ screen: 'Mode\nExit' }, 'the screen is not 1 to 64 printable', 'a screen with a line end');
	refused({ pos: { x: 16777217, y: 0, z: 0 } }, 'pos is not {x, y, z}', 'a place past 2^24 (the camera\'s limit)');
	refused({ pos: { x: 1.5, y: 0, z: 0 } }, 'pos is not {x, y, z}', 'a place that is not whole');
	refused({ pos: { x: 0, y: 0 } }, 'pos is not {x, y, z}', 'a place with no z');
	refused({ turn: 4096 }, 'turn is not 0 to 4095', 'a turn of 4096');
	refused({ look: -1025 }, 'turn is not 0 to 4095, or look', 'a look past a quarter turn');
	refused({ selected: 'switch3' }, 'selected is not a list', 'selected not a list');
	refused({ selected: [123] }, 'the first selected name is not', 'a selected name that is a number');
	refused({ selected: ['switch3\nkey Esc'] }, 'the first selected name is not', 'a selected name with a line end');
	refused({ format: 2 }, 'format is not 1', 'format 2');

	// deep nesting and many values (png_meta's own check, then the reader's)
	const deep = run(JSON.stringify(context({ extra: JSON.parse('['.repeat(70) + ']'.repeat(70)) })));

	check(deep.code === 1 && /nested deeper|not one|refused/.test(deep.out), `70 nested lists: refused (exit ${deep.code}: ${deep.out.trim().slice(0, 120)})`);

	const many = run(JSON.stringify(context({ extra: new Array(5000).fill(0) })));

	check(many.code === 1 && many.out.includes('more than 4096 values'), `5000 values: refused (exit ${many.code}: ${many.out.trim().slice(0, 120)})`);
} finally {
	fs.rmSync(folder, { recursive: true, force: true });
}

console.log(failed ? `FAILED from-shot-test (${failed})` : 'passed from-shot-test');
process.exit(failed ? 1 : 0);
