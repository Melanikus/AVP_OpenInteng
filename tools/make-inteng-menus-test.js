// make-inteng-menus-test.js: the test of tools/make-inteng-menus.js. Each refusal the tool makes has a check of its own, on a file made
// wrong in one way (the menus file is hostile, Tao 3), and the C++ it writes is checked to escape what it must. Written 2026-11-15 by
// Melanikus (Dietrich) for AVP_OpenInteng (A6). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Use, from the repository's root: node tools/make-inteng-menus-test.js. It prints "ok" or "FAILED" for each check, as the C++ tests do,
// and gives back 1 if any failed. It reads docs/inteng's two files and writes nothing.
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const tool = require('./make-inteng-menus.js');

const root = path.join(__dirname, '..');
const goodBytes = fs.readFileSync(path.join(root, 'docs', 'inteng', 'inteng-menus.json'));
const statusBytes = fs.readFileSync(path.join(root, 'docs', 'inteng', 'menu-status.json'));
const builtBytes = fs.readFileSync(path.join(root, 'docs', 'inteng', 'inteng-menus-built.json'));
let failed = 0;

function check(good, what) {
	console.log(`  ${good ? 'ok' : 'FAILED'}  ${what}`);
	failed += good ? 0 : 1;
}

const sha = (bytes) => crypto.createHash('sha256').update(bytes).digest('hex');

// Reads a menus file made wrong by change, checked against its own sha256 so the shape checks are reached; gives back the refusal's words,
// or "" if it wasn't refused
function refusal(change) {
	const menus = JSON.parse(goodBytes.toString('utf8'));

	change(menus);

	const bytes = Buffer.from(JSON.stringify(menus, null, '\t'));

	// The whole way the tool goes: read, then build, so a refusal made while building (a name too long) is reached too
	try {
		const read = tool.readMenus(bytes, sha(bytes));

		tool.build(read, tool.readStatus(statusBytes, read), tool.readBuilt(builtBytes, tool.kBuiltSha256));
		return '';
	} catch (e) {
		if (!(e instanceof tool.Refused)) {
			throw e;
		}

		return e.message;
	}
}

function refusedFor(change, words, what) {
	const why = refusal(change);

	check(why.includes(words), `${what} (refused: "${why}")`);
}

console.log('The real files:');
check(sha(goodBytes) === tool.kMenusSha256, 'docs/inteng/inteng-menus.json is the pinned file');

const menus = tool.readMenus(goodBytes, tool.kMenusSha256);
const built = tool.readBuilt(builtBytes, tool.kBuiltSha256);
const screens = tool.build(menus, tool.readStatus(statusBytes, menus), built);

check(menus.tables.length === 16 && screens.length === 23, 'its 16 tables make 16 screens, and the status file adds 7 stand-ins');
check(sha(builtBytes) === tool.kBuiltSha256, 'docs/inteng/inteng-menus-built.json is the pinned file');

const strategy = screens.find((s) => s.name === 'Strategy Data');

check(strategy && strategy.pages.length === 6 && strategy.menus.length === 4 && strategy.secondRow.length === 8, "Strategy Data has the 6 live pages inteng registers for game kind 0, 4 titles on row 1 and 8 on row 2");
check(strategy && strategy.pages.some((p) => p.row === 2 && p.page === 1 && p.menus.reduce((n, m) => n + m.items.length, 0) > 0 && p.menus.length === 4), 'its second row page 1 is the set for the kinds other than 1 and 4 (4 titles), not the 36-record one');
check(screens.filter((s) => s.pages && !s.table).length === 5 && screens.filter((s) => s.pages && !s.table).every((s) => s.menus.some((m) => m.items.some((i) => i.work === 'hub' || i.work === 'exitmode'))), 'the five code-built screens each have an Exit on their first bar');
const track = screens.find((s) => s.name === 'Track Editing');

check(track && track.table === '0x5e6e98' && track.pages && track.pages.length === 13 && track.menus.length === 3, 'Track Editing keeps its table as its bar and has the 13 pages inteng registers (pages 2 to 11; bar A of 2, 3 and 4 is the table)');
check(track && ['2:2', '3:2', '4:2', '5:2', '6:2', '7:1', '8:2', '9:2', '10:2', '11:2', '2:1', '3:1', '4:1'].every((k) => track.pages.some((p) => `${p.page}:${p.row}` === k)), "Track Editing has each page and row of Frost's track-menu-pairs.md");
check(track && track.pages.find((p) => p.page === 2 && p.row === 2).menus.map((m) => m.title).join('|') === 'Edit Section End Position|Next Section|Previous Section|Add Section|Play Track|Properties|Sound|Start Sound|End Sound|Delete', "Track Editing's page 2 bar B has inteng's ten titles in order");
check(track && track.pages.find((p) => p.page === 3 && p.row === 2).menus.some((m) => m.items.some((i) => i.command === 0x517)), "page 3 has Finish End Pos Edit (0x517)");
check(strategy && strategy.menus.flatMap((m) => m.items).some((i) => i.command === 0x2), "the items carry inteng's own command numbers (Strategy Data's Exit is 0x2)");

// The letter inteng underlines (the one after its '&', FUN_0046ce70 0x46ceac; view-state-table.md H-5c) is kept as a place in the words shown
const hub = screens[0];
const fileMenu = hub.menus.find((m) => m.title === 'File');
const quit = fileMenu && fileMenu.items.find((i) => i.name === 'Quit');
check(fileMenu && fileMenu.access === 0 && quit && quit.access === 0, "the hub's &File and &Quit underline their first letter (access 0)");
const subItem = screens.flatMap((sc) => sc.menus).flatMap((m) => m.items).find((i) => i.name.includes(': ') && i.access >= 0);
check(subItem && subItem.access >= subItem.name.indexOf(': ') + 2, `a sub-item's letter is in its own words, after its parents' ("${subItem ? subItem.name : ''}")`);
check(screens.flatMap((sc) => sc.menus).every((m) => m.access === undefined || (m.access >= -1 && m.access < m.title.length)) && screens.flatMap((sc) => sc.menus).flatMap((m) => m.items).every((i) => i.access === undefined || (i.access >= -1 && i.access < i.name.length)), 'every underlined place is inside its words, or -1');
const written = tool.emit(screens);
check(written.includes('{ "Quit", 0x10001, 0xa, IntengTag::None, IntengWork::Quit, -1, 0 },') && /\{ "File", kItems0_0, \d+, 0 \},/.test(written), 'the C++ written carries the place: Quit 0, File 0');

console.log('Every refusal, each on a file made wrong in one way:');
refusedFor(() => {}, '', '(the same file, read back, is taken)');
check(refusal(() => {}) === '', 'a file with nothing wrong is taken');

let wrongSha = '';

try {
	tool.readMenus(goodBytes, '0'.repeat(64));
} catch (e) {
	wrongSha = e.message;
}

check(wrongSha.includes('sha256'), `a file whose sha256 isn't the pinned one is refused before it's read (refused: "${wrongSha}")`);
refusedFor((m) => { m.tables[0].records[1].words = 'Qu\x01it'; }, 'outside 0x20 to 0x7E', 'a control letter in a record\'s words');
refusedFor((m) => { m.tables[0].records[1].words = 'Quét'; }, 'outside 0x20 to 0x7E', 'a letter above 0x7E in a record\'s words');
refusedFor((m) => { m.tables[0].records[1].words = '&Qu&it'; }, 'more than one "&"', 'two "&" in one record\'s words');
refusedFor((m) => { m.tables[0].records[1].words = 'Quit &'; }, 'no letter or digit after it', 'an "&" with nothing after it');
refusedFor((m) => { m.tables[0].records[1].words = '&' + 'Q'.repeat(70); }, 'longer than', 'words too long for the bar with their tag');
refusedFor((m) => { delete m.tables[0].records[1].command; }, 'its fields are', 'a record with a field missing');
refusedFor((m) => { m.tables[0].records[1].extra = 1; }, 'its fields are', 'a record with a field too many');
refusedFor((m) => { m.tables[0].records[1].kind = '1'; }, 'not 0, 1 or 2', 'a record\'s kind of the wrong type (a text)');
refusedFor((m) => { m.tables[0].records[1].kind = 3; }, 'not 0, 1 or 2', 'a record\'s kind past 2');
refusedFor((m) => { m.tables[0].records[1].command = 10; }, 'not a number written 0x', 'a command of the wrong type (a number)');
refusedFor((m) => { m.tables[0].records[1].words = ''; m.tables[0].records[1].command = '0xa'; }, 'isn\'t a blank line', 'no words on a record that isn\'t a blank line');
refusedFor((m) => { m.tables[0].records[0].kind = 1; }, 'starts with a title', 'a table that doesn\'t start with a title');
refusedFor((m) => { m.tables.pop(); }, 'not 16', 'a file of 15 tables');
refusedFor((m) => { delete m.tables[3].usedBy; }, 'its fields are', 'a table with a field missing');

// The code-built menus' file, hostile like the other (Tao 3): each refusal on a file made wrong in one way
function builtRefusal(change) {
	const file = JSON.parse(builtBytes.toString('utf8'));

	change(file);

	const bytes = Buffer.from(JSON.stringify(file, null, '\t'));

	try {
		const read = tool.readBuilt(bytes, sha(bytes));

		tool.build(menus, tool.readStatus(statusBytes, menus), read);
		return '';
	} catch (e) {
		if (!(e instanceof tool.Refused)) {
			throw e;
		}

		return e.message;
	}
}

function builtRefusedFor(change, words, what) {
	const why = builtRefusal(change);

	check(why.includes(words), `${what} (refused: "${why}")`);
}

console.log('The code-built menus file, each refusal on a file made wrong in one way:');
check(builtRefusal(() => {}) === '', 'a built file with nothing wrong is taken');

let builtWrongSha = '';

try {
	tool.readBuilt(builtBytes, '0'.repeat(64));
} catch (e) {
	builtWrongSha = e.message;
}

check(builtWrongSha.includes('sha256'), `a built file whose sha256 isn't the pinned one is refused before it's read (refused: "${builtWrongSha}")`);
builtRefusedFor((f) => { f.screens[0].pages[0].records[0].words = 'Qu\x01it'; }, 'outside 0x20 to 0x7E', "a control letter in a built record's words");
builtRefusedFor((f) => { f.screens[0].pages[0].records[0].words = '&Qu&it'; }, 'more than one "&"', "two '&' in one built record's words");
builtRefusedFor((f) => { f.screens[0].pages[0].records[0].command = 5; }, 'not a number written 0x', 'a built command of the wrong type');
builtRefusedFor((f) => { f.screens[0].pages[0].records[0].kind = 4; }, 'not 0, 1, 2 or 3', "a built record's kind past 3");
builtRefusedFor((f) => { f.screens[0].pages[0].records[1].kind = 3; }, 'with no kind 2 above it', 'a kind 3 straight under a kind 0');
builtRefusedFor((f) => { f.screens[0].pages[0].records[0].kind = 1; }, 'a kind 1 with no kind 0 above it', "a page that doesn't start with a title");
builtRefusedFor((f) => { f.screens[0].pages[0].records[1].words = ''; f.screens[0].pages[0].records[1].command = '0x5'; }, "isn't a blank line", "no words on a built record that isn't a blank line");
builtRefusedFor((f) => { f.screens[0].pages[0].records[0].extra = 1; }, 'its fields are', 'a built record with a field too many');
builtRefusedFor((f) => { delete f.screens[0].pages[0].base; }, 'its fields are', 'a built page with a field missing');
builtRefusedFor((f) => { f.screens[0].pages[0].row = 3; }, 'its row is not 1 or 2', 'a built page of row 3');
builtRefusedFor((f) => { f.screens[0].pages[0].page = 99; }, 'its page number', 'a built page number too big');
builtRefusedFor((f) => { f.screens[0].pages[0].kinds = 'some'; }, 'its kinds are not', 'built kinds that are not all, other or a list');
builtRefusedFor((f) => { f.screens[0].pages[0].kinds = [1, 1]; }, 'its kinds are not', 'a list of built kinds with a repeat');
builtRefusedFor((f) => { f.screens[0].pages[0].records = []; }, 'its records are not a list', 'a built page with no records');
builtRefusedFor((f) => { f.screens[0].screen = 'Path'; }, 'is "Path", not Strategy Data', 'a built screen out of its place');
builtRefusedFor((f) => { f.screens.pop(); }, 'not 6', 'a built file of 5 screens');
builtRefusedFor((f) => { f.gameKind.value = -1; }, 'gameKind', 'a game kind below 0');
builtRefusedFor((f) => { delete f.gameKind.evidence; }, 'its fields are', 'a game kind with no evidence');
builtRefusedFor((f) => { f.screens[0].pages.push(JSON.parse(JSON.stringify(f.screens[0].pages[0]))); }, 'live pages', 'two live pages for one row and page number');
builtRefusedFor((f) => { f.screens[0].pages = f.screens[0].pages.filter((p) => !(p.row === 1 && p.page === 0)); }, 'no row 1 page 0', 'a screen with no row 1 page 0');
builtRefusedFor((f) => { f.screens[0].pages[0].records.forEach((r) => { if (r.words.includes('Exit')) { r.words = 'Leave'; } }); }, 'has no Exit', 'a built screen with no Exit on its first bar');
builtRefusedFor((f) => { f.screens[0].pages[0].records[1].words = 'X'.repeat(70); }, 'longer than', 'a built item longer than the bar takes');

let notJson = '';
const broken = Buffer.from('{ "what": ');

try {
	tool.readMenus(broken, sha(broken));
} catch (e) {
	notJson = e.message;
}

check(notJson.includes('isn\'t JSON'), `a file that isn't JSON is refused (refused: "${notJson}")`);

console.log('The C++ written can\'t be broken out of:');
check(tool.cpp('a"b') === '"a\\"b"', 'a double quote is escaped');
check(tool.cpp('a\\b') === '"a\\\\b"', 'a backslash is escaped');
check(tool.cpp('a??=b') === '"a\\?\\?=b"', 'every "?" is written "\\?", so no trigraph can form');

console.log(failed ? `SOME CHECKS FAILED: ${failed}` : 'All checks passed');
process.exit(failed ? 1 : 0);
