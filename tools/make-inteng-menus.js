// make-inteng-menus.js: writes src/host/inteng_menus.cpp, every menu of the old editor (Rebellion's inteng, 2000) as the editor shows
// them, from docs/inteng/inteng-menus.json (inteng.exe's 16 menu tables) and docs/inteng/menu-status.json (what the editor does of them).
// Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (A6, the all-menus build). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// Use, from the repository's root:
//   node tools/make-inteng-menus.js           writes src/host/inteng_menus.cpp
//   node tools/make-inteng-menus.js --check   writes nothing: says whether src/host/inteng_menus.cpp is what the two files make, byte for
//                                             byte (the gate runs this), and gives back 1 when it isn't
//
// THE MENUS FILE IS HOSTILE (Tao 3): it was read out of a program we didn't write. So before anything is made of it, its sha256 has to be
// the pinned one below, it's read with JSON.parse alone, and its whole shape is checked: the table count, each table's and each record's
// fields (no more and no fewer) and their types. Every text has to be plain printable letters (0x20 to 0x7E), with at most one "&" and a
// letter or digit after it, and fit the menu bar with its tag. Anything else refuses the whole file, saying why, and nothing is written.
// The status file is ours, but it's checked the same way. The C++ written escapes "\", the double quote and every "?" (so no "??" can
// ever make a trigraph), so no text can end a string or change what it means.
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// The menus file's sha256, as Vasquez's read wrote it (docs/inteng/README.md says which read and how to make it again)
const kMenusSha256 = 'bf0cdf842d5b52b9a48d9598cb86848e6826d8721ff35bdd84ffe9702d3527ee';

// The menu bar's limits (src/ui/menu_bar.h): the generated file checks them again with static_assert, so the two can never drift apart
const kNameMost = 64;
const kTagMost = 24;
// (96 items under one title: Texture Map's longest drop-down has 61 and the drop-down scrolls; 16 titles on one bar: Waypoint's second row, page 3, has 14)
const kMostMenus = 16;
const kMostItems = 96;
// Each screen leaves room on its bar for the editor's own two menus (its View menu, and on the hub its Screens menu)
const kOursMenus = 2;
const kTableCount = 16;

// The code-built menus' file (docs/inteng/inteng-menus-built.json): the pages inteng registers for the six screens that build their menus in code
const kBuiltSha256 = '5932e880bf462dafddf720498cad35f11f47b4aa7c4be3b095f143c34d98527c';
const kBuiltScreens = ['Strategy Data', 'Path', 'Waypoint', 'Texture Map', 'Hierarchy', 'Track Editing'];
// A built screen that is not a stand-in but a screen with a table of its own (Track Editing, 0x5e6e98): its pages are added to that screen, whose table stays its bar when no page is shown
const kBuiltOnTable = ['Track Editing'];
const kMostPages = 12;           // page numbers of one row run 0 to 11 (the program's own run 0 to 4; Track Editing's run 2 to 11)
const kMostBuiltRecords = 400;   // records on one page (Texture Map's biggest has 91)

const kTagWords = { none: '', wip: ' [wip]', blender: ' [Blender]', noavp: ' [no AvP]', unfinished: ' [unfinished in inteng]' };
const kTagCpp = { none: 'None', wip: 'Wip', blender: 'Blender', noavp: 'NoAvp', unfinished: 'Unfinished' };
const kWorkCpp = { none: 'None', quit: 'Quit', screen: 'ToScreen', hub: 'ToHub', keys: 'Keys', find: 'Find', moverel: 'MoveRelative', delete: 'DeleteObject', createlike: 'CreateLikeSelected', setbase: 'SetBaseModule', findsmall: 'FindSmallModules', keysmode: 'ConstructionKeys', screenshot: 'Screenshot', copytrack: 'CopyTrack', pastetrackabs: 'PasteTrackAbsolute', findnotdone: 'FindNotDone', writeasc: 'WriteAsc', writeascdoors: 'WriteAscDoorsClosed', writec: 'WriteC', linkcurrent: 'LinkSetCurrent', linknormal: 'LinkNormal', linkbranch: 'LinkBranch', linkdelete: 'LinkDelete', linkclose: 'LinkClose', selectlight: 'SelectLights', selectobject: 'SelectObjects', lighthelp: 'LightHelp', hideall: 'HideAllLights', unhideall: 'UnhideAllLights', quickcreate: 'QuickCreate', altloc: 'AltLocation', screenwhite: 'ToScreenNoTextures', recalcentry: 'RecalcEntryPoints', recalcnormals: 'RecalculateNormals', moverelpoly: 'MoveRelToPoly', replaceshape: 'ReplaceShape', adjadd: 'AdjacentAdd', adjdelete: 'AdjacentDelete', adjsetcurrent: 'AdjacentSetCurrent', deletemoduledata: 'DeleteModuleData', mergeobjects: 'MergeObjects', linkexit: 'LinkExit', viewpolygon: 'ViewPolygon', viewpolyline: 'ViewPolyline', viewfilledpolyline: 'ViewFilledPolyline', linkview: 'LinkView', linkhide: 'LinkHide', deletelinks: 'DeleteLinksPicked', deletelinksall: 'DeleteLinksAll', typemodule: 'ChangeToModule', typeplaced: 'ChangeToPlaced', rescale: 'Rescale', centretop: 'MoveCentreTop', centrebottom: 'MoveCentreBottom', reverselinks: 'ReverseLinksPicked', reverselinksall: 'ReverseLinksAll', linkopen: 'LinkOpen', chooserstart: 'ChooserStart', chooserwith: 'ChooserWith', chooserwithout: 'ChooserWithout', choosercreate: 'ChooserStartCreate', autolinkmodule: 'AutoLinkModule', autolinkenv: 'AutoLinkEnvironment', autolinkmodulenew: 'AutoLinkModuleNew', autolinkenvnew: 'AutoLinkEnvironmentNew', setvisibility: 'SetVisibility', autolinkload: 'AutoLinkLoadFile', linkexitnew: 'LinkExitNew', linkexitall: 'LinkExitAll', linkhelp: 'LinkHelp', linkwithavp: 'LinkWithAvp', viewdistance: 'ViewDistanceToggle', onlylinked: 'OnlyLinkedToggle', selectpoly: 'SelectByPolygons', preferbmp: 'PreferBmp', polyviewdist: 'PolygonViewDistance', showall: 'ShowAllModules', objhide: 'HideObjects', objunhidelast: 'UnhideLastObject', objunhideall: 'UnhideAllObjects', strategy: 'Strategy', findobject: 'FindObject', mixed: 'MixedView', polycounts: 'ShowPolyCounts', selectcentres: 'SelectWithCentres', path: 'PathCommand', hierarchy: 'HierarchyCommand', savecopy: 'SaveCopy', waypoint: 'WaypointCommand', doorsclose: 'DoorsClose', doorsopen: 'DoorsOpen', lighttoggle: 'LightToggle', lightswitchallon: 'LightSwitchAllOn', saveintgame: 'SaveIntensitiesGame', saveinttools: 'SaveIntensitiesTools', special: 'SpecialCommand', soften: 'SoftEdge', lighthier: 'LightHierarchy', lightseq: 'LightSequence', trackedit: 'TrackEdit', texturemap: 'TextureMapCommand', viewlighting: 'ViewLighting', opcounts: 'PlacementPolyCounts', snapvertex: 'SnapToVertex', gravity: 'GravityToggle', splitscreen: 'SplitScreenToggle', moverelmodule: 'MoveRelativeToModule', fixtears: 'FixTearsSelected', fixtearsall: 'FixTearsEvery', recentrepoly: 'RecentrePolygon', pastetrackrel: 'PasteTrackRelative', exitmode: 'ExitMode', adjexit: 'AdjacentExit', adjshowall: 'AdjacentShowAll', adjviewdistance: 'AdjacentViewDistance', moduleoverlaps: 'ModuleOverlaps', placementkeys: 'PlacementKeys', placedhier: 'PlacedHierarchy', acoustic: 'AcousticCommand', textscale: 'TextScale', videomode: 'VideoModeEnter', videobrowse: 'VideoBrowse' };


// What an item of a stand-in's own menus may do (the ones of the tables' items that need no table, and the strategy mode's)
const kWorkOfStandIn = ['find', 'hub', 'strategy', 'path', 'savecopy', 'findobject', 'trackedit', 'acoustic', 'videobrowse'];

class Refused extends Error {}

function refuse(why) {
	throw new Refused(why);
}

function exactKeys(object, keys, where) {
	if (typeof object !== 'object' || object === null || Array.isArray(object)) {
		refuse(`${where}: not an object`);
	}

	const have = Object.keys(object).sort().join(',');
	const want = keys.slice().sort().join(',');

	if (have !== want) {
		refuse(`${where}: its fields are ${have}, not ${want}`);
	}
}

function text(value, where) {
	if (typeof value !== 'string') {
		refuse(`${where}: not a text`);
	}

	if (value.length === 0) {
		refuse(`${where}: empty`);
	}

	if (!/^[\x20-\x7e]*$/.test(value)) {
		refuse(`${where}: a letter outside 0x20 to 0x7E`);
	}

	return value;
}

// inteng's words: plain letters, and at most one "&", which marks the letter after it (a letter or a digit)
function words(value, where) {
	text(value, where);

	const amps = value.split('&').length - 1;

	if (amps > 1) {
		refuse(`${where}: more than one "&"`);
	}

	if (amps === 1 && !/&[A-Za-z0-9]/.test(value)) {
		refuse(`${where}: an "&" with no letter or digit after it`);
	}

	return value;
}

function hex(value, where) {
	if (typeof value !== 'string' || !/^0x[0-9a-f]{1,8}$/.test(value)) {
		refuse(`${where}: not a number written 0x and 1 to 8 small hex digits`);
	}

	return parseInt(value, 16);
}

// Reads the menus file's bytes: refuses them unless their sha256 is expected, then checks their whole shape
function readMenus(bytes, expectedSha256) {
	const sha = crypto.createHash('sha256').update(bytes).digest('hex');

	if (sha !== expectedSha256) {
		refuse(`the menus file's sha256 is ${sha}, not the pinned ${expectedSha256}`);
	}

	let menus;

	try {
		menus = JSON.parse(bytes.toString('utf8'));
	} catch (e) {
		refuse('the menus file isn\'t JSON');
	}

	exactKeys(menus, ['what', 'sha256', 'imageBase', 'tables'], 'the menus file');
	text(menus.what, 'what');
	text(menus.sha256, 'sha256');
	text(menus.imageBase, 'imageBase');

	if (!Array.isArray(menus.tables) || menus.tables.length !== kTableCount) {
		refuse(`the menus file has ${Array.isArray(menus.tables) ? menus.tables.length : 'no'} tables, not ${kTableCount}`);
	}

	menus.tables.forEach((table, t) => {
		const where = `table ${t}`;

		exactKeys(table, ['address', 'screen', 'screenBasis', 'usedBy', 'referencedAt', 'endMarkerAt', 'records'], where);
		hex(table.address, `${where}'s address`);

		for (const field of ['screen', 'screenBasis', 'usedBy', 'referencedAt', 'endMarkerAt']) {
			text(table[field], `${where}'s ${field}`);
		}

		if (!Array.isArray(table.records) || table.records.length === 0) {
			refuse(`${where}: no records`);
		}

		table.records.forEach((record, r) => {
			const at = `${where}, record ${r}`;

			exactKeys(record, ['address', 'kind', 'words', 'command'], at);
			hex(record.address, `${at}'s address`);

			if (!Number.isInteger(record.kind) || record.kind < 0 || record.kind > 2) {
				refuse(`${at}: its kind is not 0, 1 or 2`);
			}

			if (r === 0 && record.kind !== 0) {
				refuse(`${at}: a table starts with a title (kind 0)`);
			}

			if (record.kind === 2 && table.records[r - 1].kind === 0) {
				refuse(`${at}: a kind 2 with no kind 1 above it`);
			}

			// A blank line in a menu (a separator) is the one record with no words: an item (kind 1) with command 0
			if (record.words === '') {
				if (record.kind !== 1 || hex(record.command, `${at}'s command`) !== 0) {
					refuse(`${at}: no words, and it isn't a blank line (kind 1, command 0)`);
				}
			} else {
				words(record.words, `${at}'s words`);
			}

			hex(record.command, `${at}'s command`);
		});
	});

	return menus;
}

// Reads the code-built menus' file (inteng's Strategy, Path, Waypoint, Texture Map and Hierarchy screens): refuses it unless its sha256 is the
// pinned one, then checks its whole shape. Like the other menus file it was read out of a program we didn't write, so it is hostile input (Tao 3).
function readBuilt(bytes, expectedSha256) {
	const sha = crypto.createHash('sha256').update(bytes).digest('hex');

	if (sha !== expectedSha256) {
		refuse(`the built menus file's sha256 is ${sha}, not the pinned ${expectedSha256}`);
	}

	let built;

	try {
		built = JSON.parse(bytes.toString('utf8'));
	} catch (e) {
		refuse('the built menus file isn\'t JSON');
	}

	exactKeys(built, ['what', 'gameKind', 'screens'], 'the built menus file');
	text(built.what, 'built what');
	exactKeys(built.gameKind, ['value', 'evidence'], 'built gameKind');

	if (!Number.isInteger(built.gameKind.value) || built.gameKind.value < 0 || built.gameKind.value > 99) {
		refuse('built gameKind\'s value is not a whole number from 0 to 99');
	}

	text(built.gameKind.evidence, 'built gameKind\'s evidence');

	if (!Array.isArray(built.screens) || built.screens.length !== kBuiltScreens.length) {
		refuse(`the built menus file has ${Array.isArray(built.screens) ? built.screens.length : 'no'} screens, not ${kBuiltScreens.length}`);
	}

	built.screens.forEach((screen, k) => {
		const where = `built screen ${k}`;

		exactKeys(screen, ['screen', 'builder', 'registrar', 'pages'], where);

		if (screen.screen !== kBuiltScreens[k]) {
			refuse(`${where} is ${JSON.stringify(screen.screen)}, not ${kBuiltScreens[k]}`);
		}

		text(screen.builder, `${where}'s builder`);
		text(screen.registrar, `${where}'s registrar`);

		if (!Array.isArray(screen.pages) || screen.pages.length === 0 || screen.pages.length > 4 * kMostPages) {
			refuse(`${where}: its pages are not a list of 1 to ${4 * kMostPages}`);
		}

		screen.pages.forEach((page, p) => {
			const at = `${where}, page ${p}`;

			exactKeys(page, ['row', 'page', 'kinds', 'base', 'offsetCorrection', 'records'], at);

			if (page.row !== 1 && page.row !== 2) {
				refuse(`${at}: its row is not 1 or 2`);
			}

			if (!Number.isInteger(page.page) || page.page < 0 || page.page >= kMostPages) {
				refuse(`${at}: its page number is not a whole number from 0 to ${kMostPages - 1}`);
			}

			if (!(page.kinds === 'all' || page.kinds === 'other' || (Array.isArray(page.kinds) && page.kinds.length >= 1 && page.kinds.length <= 8
				&& page.kinds.every((kind, i) => Number.isInteger(kind) && kind >= 0 && kind <= 99 && page.kinds.indexOf(kind) === i)))) {
				refuse(`${at}: its kinds are not "all", "other" or a list of different whole numbers from 0 to 99`);
			}

			hex(page.base, `${at}'s base`);

			if (!Number.isInteger(page.offsetCorrection) || page.offsetCorrection < -8 || page.offsetCorrection > 0x40) {
				refuse(`${at}: its offsetCorrection is not a whole number from -8 to 64`);
			}

			if (!Array.isArray(page.records) || page.records.length === 0 || page.records.length > kMostBuiltRecords) {
				refuse(`${at}: its records are not a list of 1 to ${kMostBuiltRecords}`);
			}

			let last = -1;

			page.records.forEach((record, r) => {
				const here = `${at}, record ${r}`;

				exactKeys(record, ['kind', 'words', 'command'], here);

				if (!Number.isInteger(record.kind) || record.kind < 0 || record.kind > 3) {
					refuse(`${here}: its kind is not 0, 1, 2 or 3`);
				}

				// a page starts with a title, and a level is entered one at a time (a kind 2 needs a kind 1 above it, a kind 3 a kind 2)
				if (record.kind > last + 1) {
					refuse(`${here}: a kind ${record.kind} with no kind ${record.kind - 1} above it`);
				}

				last = record.kind;

				if (record.words === '') {
					if (record.kind < 1 || hex(record.command, `${here}'s command`) !== 0) {
						refuse(`${here}: no words, and it isn't a blank line (kind 1 to 3, command 0)`);
					}
				} else {
					words(record.words, `${here}'s words`);
				}

				hex(record.command, `${here}'s command`);
			});
		});
	});

	return built;
}

// The pages of a built screen that are registered for the game kind, one for each row and page number: a page of kinds "all" is registered whatever the kind;
// of the pages that share a row and number, the one whose list holds the kind, else the one of "other". Refuses a row and number with none, or with two.
function livePages(screen, kind) {
	const groups = new Map();

	for (const page of screen.pages) {
		const key = `${page.row}:${page.page}`;

		if (!groups.has(key)) {
			groups.set(key, []);
		}

		groups.get(key).push(page);
	}

	const live = [];

	for (const [key, group] of groups) {
		const listed = group.filter((page) => Array.isArray(page.kinds) && page.kinds.includes(kind));
		const all = group.filter((page) => page.kinds === 'all');
		const other = group.filter((page) => page.kinds === 'other');
		const chosen = listed.length > 0 ? listed : all.length > 0 ? all : other;

		if (chosen.length !== 1 || (all.length > 0 && group.length > 1)) {
			refuse(`${screen.screen}: row:page ${key} has ${chosen.length} live pages for game kind ${kind}`);
		}

		live.push(chosen[0]);
	}

	return live.sort((a, b) => a.row - b.row || a.page - b.page);
}

// Reads the status file: what the editor does of each screen and item
function readStatus(bytes, menus) {
	let status;

	try {
		status = JSON.parse(bytes.toString('utf8'));
	} catch (e) {
		refuse('the status file isn\'t JSON');
	}

	// "extras" is optional: items of OURS on a built page that inteng has no record for (Track Editing's Turn End). "additions" is optional too: items of OURS put into one of
	// inteng's tables (the hub's File > Save, the owner's), each NAMED by its note, which the C++ carries as a comment over the item
	exactKeys(status, ['screens', 'items', 'standIns'].concat('extras' in status ? ['extras'] : [], 'additions' in status ? ['additions'] : []), 'the status file');

	if ('additions' in status) {
		if (!Array.isArray(status.additions)) {
			refuse('the status file\'s additions are not a list');
		}

		status.additions.forEach((addition, i) => {
			const at = `status addition ${i}`;

			exactKeys(addition, ['table', 'title', 'after', 'words', 'action', 'does', 'note'], at);

			const s = menus.tables.findIndex((t) => t.address === addition.table);

			if (s < 0) {
				refuse(`${at}: no table ${addition.table}`);
			}

			text(addition.title, `${at}'s title`);
			text(addition.after, `${at}'s after`);
			words(addition.words, `${at}'s words`);

			if (shown(addition.words).length > kNameMost) {
				refuse(`${at}: its words are longer than ${kNameMost} letters`);
			}

			// its number is OURS: its table's, past every record (0xff00 to 0xffff), so it can never be one of inteng's
			const action = hex(addition.action, `${at}'s action`);

			if (Math.floor(action / 0x10000) !== s + 1 || action % 0x10000 < 0xff00) {
				refuse(`${at}: its action ${addition.action} is not one of ours for table ${addition.table} (0x${(s + 1).toString(16)}ff00 to 0x${(s + 1).toString(16)}ffff)`);
			}

			if (!Object.keys(kWorkCpp).includes(addition.does) || addition.does === 'none') {
				refuse(`${at}: it does ${addition.does}`);
			}

			// the note becomes a C++ comment: plain letters, no "\" (a line ending in one would take the next line into the comment) and no "??" (a trigraph)
			text(addition.note, `${at}'s note`);

			if (addition.note.length > 400 || addition.note.includes('\\') || addition.note.includes('??')) {
				refuse(`${at}: its note is longer than 400 letters or holds a "\\" or "??"`);
			}
		});
	}

	if ('extras' in status) {
		if (!Array.isArray(status.extras)) {
			refuse('the status file\'s extras are not a list');
		}

		status.extras.forEach((extra, i) => {
			const at = `status extra ${i}`;

			// "menu" is optional (Vasquez 11-20, the owner's "Alien auto waypoint" drop-down): extras with the same menu on one page are the items of ONE menu of that title
			exactKeys(extra, ['table', 'row', 'page', 'title', 'command', 'does'].concat('menu' in extra ? ['menu'] : []), at);

			if ('menu' in extra && (typeof extra.menu !== 'string' || extra.menu.length < 1 || extra.menu.length > kNameMost || !/^[ -~]+$/.test(extra.menu))) {
				refuse(`${at}: its menu`);
			}

			if (typeof extra.table !== 'string' || !extra.table.startsWith('built:') || !kBuiltScreens.includes(extra.table.slice(6))) {
				refuse(`${at}: no built screen ${extra.table}`);
			}

			if (!Number.isInteger(extra.row) || extra.row < 1 || extra.row > 2 || !Number.isInteger(extra.page) || extra.page < 0 || extra.page > 15) {
				refuse(`${at}: its row or page`);
			}

			if (typeof extra.title !== 'string' || extra.title.length < 1 || extra.title.length > kNameMost || !/^[ -~]+$/.test(extra.title)) {
				refuse(`${at}: its title`);
			}

			hex(extra.command, `${at}'s command`);

			if (!Object.keys(kWorkCpp).includes(extra.does) || extra.does === 'none') {
				refuse(`${at}: it does ${extra.does}`);
			}
		});
	}

	if (!Array.isArray(status.screens) || status.screens.length !== kTableCount) {
		refuse(`the status file has ${Array.isArray(status.screens) ? status.screens.length : 'no'} screens, not ${kTableCount}`);
	}

	status.screens.forEach((screen, s) => {
		exactKeys(screen, ['table', 'name', 'tag'], `status screen ${s}`);

		if (screen.table !== menus.tables[s].address) {
			refuse(`status screen ${s} is ${screen.table}, but the menus file's table ${s} is ${menus.tables[s].address}`);
		}

		text(screen.name, `status screen ${s}'s name`);

		if (!['wip', 'blender', 'noavp', 'unfinished'].includes(screen.tag)) {
			refuse(`status screen ${s}'s tag is not wip, blender, noavp or unfinished`);
		}
	});

	if (!Array.isArray(status.standIns) || status.standIns.length === 0) {
		refuse('the status file has no standIns');
	}

	status.standIns.forEach((standIn, k) => {
		// A stand-in may hold menus of its own, for a screen whose menu inteng builds in code (so no table has it): the items we know from
		// inteng's strings, each with what it does or its tag. Without them the screen is Exit and a "being mapped" line.
		exactKeys(standIn, 'menus' in standIn ? ['name', 'tag', 'menus'] : 'built' in standIn ? ['name', 'tag', 'built'] : ['name', 'tag'], `stand-in ${k}`);
		text(standIn.name, `stand-in ${k}'s name`);

		// A stand-in whose menus are inteng's own, read from the code that builds them (docs/inteng/inteng-menus-built.json): it names the built screen
		// and has the same name
		if ('built' in standIn && (standIn.built !== standIn.name || !kBuiltScreens.includes(standIn.built))) {
			refuse(`stand-in ${k}: its built screen ${JSON.stringify(standIn.built)} is not its own name, or is not one of ${kBuiltScreens.join(', ')}`);
		}

		if (!['wip', 'blender', 'unfinished'].includes(standIn.tag)) {
			refuse(`stand-in ${k}'s tag is not wip, blender or unfinished`);
		}

		if ('menus' in standIn) {
			if (!Array.isArray(standIn.menus) || standIn.menus.length === 0 || standIn.menus.length > kMostMenus - kOursMenus - 1) {
				refuse(`stand-in ${k}'s menus are not a list of 1 to ${kMostMenus - kOursMenus - 1}`);
			}

			standIn.menus.forEach((menu, m) => {
				const where = `stand-in ${k}, menu ${m}`;

				exactKeys(menu, ['title', 'items'], where);
				words(menu.title, `${where}'s title`);

				if (!Array.isArray(menu.items) || menu.items.length === 0 || menu.items.length > kMostItems) {
					refuse(`${where}: its items are not a list of 1 to ${kMostItems}`);
				}

				menu.items.forEach((item, i) => {
					const at = `${where}, item ${i}`;
					const keys = Object.keys(item).sort().join(',');

					if (!['does,name', 'name,tag', 'command,does,name', 'command,name,tag'].includes(keys)) {
						refuse(`${at}: its fields are ${keys}`);
					}

					words(item.name, `${at}'s name`);

					// the real command number of the record, when it is known (a menu inteng builds in code: strategy-menu-pairs.md)
					if ('command' in item) {
						hex(item.command, `${at}'s command`);
					}


					if ('does' in item && !kWorkOfStandIn.includes(item.does)) {
						refuse(`${at}: it does ${item.does}`);
					}

					if ('tag' in item && !['wip', 'blender', 'noavp'].includes(item.tag)) {
						refuse(`${at}: its tag is ${item.tag}`);
					}
				});
			});
		}
	});

	if (!Array.isArray(status.items)) {
		refuse('the status file\'s items are not a list');
	}

	status.items.forEach((item, i) => {
		const at = `status item ${i}`;
		const keys = Object.keys(item).sort().join(',');

		if (!['command,does,table', 'command,does,table,to', 'command,table,tag'].includes(keys)) {
			refuse(`${at}: its fields are ${keys}`);
		}

		// A table of inteng's file by its address, or a code-built screen as "built:" and its name
		if (!menus.tables.some((table) => table.address === item.table) && !(typeof item.table === 'string' && item.table.startsWith('built:') && kBuiltScreens.includes(item.table.slice(6)))) {
			refuse(`${at}: no table ${item.table}`);
		}

		hex(item.command, `${at}'s command`);

		if ('does' in item && !Object.keys(kWorkCpp).includes(item.does) && item.does !== 'none') {
			refuse(`${at}: it does ${item.does}`);
		}

		if ('tag' in item && !['wip', 'blender', 'noavp', 'unfinished'].includes(item.tag)) {
			refuse(`${at}: its tag is ${item.tag}`);
		}

		if ((item.does === 'screen' || item.does === 'screenwhite') && typeof item.to !== 'string') {
			refuse(`${at}: a screen item with nowhere to go`);
		}
	});

	return status;
}

// The words as shown: inteng's without its "&"
function shown(value) {
	return value.replace('&', '');
}

// The place, in the words as shown (name), of the letter inteng underlines: the one after the "&" of its own words, which end the name (a sub-item's name is "<its parents' words>: <its
// words>"); -1 when they have none. FUN_0046ce70 0x46ceac draws the letter after a "&" underlined (view-state-table.md H-5c).
function accessIn(name, words) {
	const at = words.indexOf('&');

	return at < 0 ? -1 : name.length - shown(words).length + at;
}

// One page of a code-built screen as bar menus: the same rules as a table's (a kind 0 is a title, a kind 1 an item under it, a kind 2 an item under the last
// kind 1 and a kind 3 one under the last kind 2, each shown as "its parents' words: its words"; a blank line is left out), the items keyed by inteng's own
// command numbers. What an item does is the status file's, found by the screen and the command: the same command is the same item wherever it is.
function builtPageMenus(screen, page, s, status, screenOf, tag) {
	const menus = [];
	const records = page.records;
	const tableKey = `built:${screen.screen}`;

	records.forEach((record, r) => {
		const command = parseInt(record.command, 16);
		const next = records[r + 1];
		const action = (s + 1) * 0x10000 + (page.row - 1) * 0x4000 + page.page * 0x400 + r;
		const where = `${screen.screen}, row ${page.row} page ${page.page}, record ${r}`;

		const addItem = (menu, name, words) => {
			const said = status.items.filter((i) => i.table === tableKey && parseInt(i.command, 16) === command);

			if (said.length > 1) {
				refuse(`${tableKey}: the status file names command ${record.command} more than once`);
			}

			let itemTag = tag;
			let work = 'none';
			let toScreen = -1;

			if (said.length === 1 && said[0].does) {
				itemTag = 'none';
				work = said[0].does;
				toScreen = work === 'screen' || work === 'screenwhite' ? screenOf(said[0].to) : -1;
			} else if (said.length === 1) {
				itemTag = said[0].tag;
			} else if (name === 'Exit') {
				itemTag = 'none';
				work = 'hub';
			}

			if (name.length + kTagWords[itemTag].length > kNameMost) {
				refuse(`${where}: "${name}${kTagWords[itemTag]}" is longer than ${kNameMost} letters`);
			}

			menu.items.push({ name, action, command, tag: itemTag, work, toScreen, access: accessIn(name, words) });
		};

		if (record.kind === 0) {
			const menu = { title: shown(record.words), access: record.words.indexOf('&'), items: [] };

			menus.push(menu);

			// A title with a command and nothing under it is a menu of one item with its words
			if (command !== 0 && (!next || next.kind === 0)) {
				addItem(menu, shown(record.words), record.words);
			}

			return;
		}

		if (next && next.kind === record.kind + 1) {
			return;   // its items follow, each shown as "its words: theirs"
		}

		if (record.words === '') {
			return;   // a blank line: the bar has no separators yet
		}

		// the words of the parents, from the nearest kind below this one up to the kind 1
		const chain = [];
		let want = record.kind - 1;

		for (let p = r - 1; p >= 0 && want >= 1; p--) {
			if (records[p].kind === want) {
				chain.unshift(shown(records[p].words));
				want--;
			} else if (records[p].kind < want) {
				break;
			}
		}

		addItem(menus[menus.length - 1], [...chain, shown(record.words)].join(': '), record.words);
	});

	// Items of ours on this page (status "extras"), after inteng's, each with an action number past the page's records: a menu of one item, or (with "menu") an item of that menu, the
	// menu where its first item is, its items in the order given
	(status.extras || []).filter((e) => e.table === tableKey && e.row === page.row && e.page === page.page).forEach((extra, k) => {
		const item = { name: extra.title, action: (s + 1) * 0x10000 + (page.row - 1) * 0x4000 + page.page * 0x400 + 0x3ff - k, command: parseInt(extra.command, 16), tag: 'none', work: extra.does, toScreen: -1 };
		const into = 'menu' in extra ? menus.find((m) => m.extraMenu === extra.menu) : undefined;

		if (into) {
			into.items.push(item);
		} else {
			menus.push('menu' in extra ? { title: extra.menu, extraMenu: extra.menu, items: [item] } : { title: extra.title, items: [item] });
		}
	});

	return menus;
}

// A stand-in screen made of a built screen: the live pages for the game kind, both rows, every page's menus kept. Row 1 page 0 is the bar the screen starts
// with and row 2 page 0 the second bar under it; the other pages are for the handlers that switch pages (inteng's method +0x18).
function buildBuiltScreen(standIn, s, built, status, screenOf) {
	if (!built) {
		refuse(`${standIn.name} needs the built menus file, which wasn't given`);
	}

	const source = built.screens.find((screen) => screen.screen === standIn.built);
	const pages = livePages(source, built.gameKind.value).map((page) => {
		const menus = builtPageMenus(source, page, s, status, screenOf, standIn.tag);
		const most = page.row === 1 ? kMostMenus - kOursMenus : kMostMenus;

		if (menus.length === 0 || menus.length > most) {
			refuse(`${standIn.name}, row ${page.row} page ${page.page} has ${menus.length} menus: not 1 to ${most}`);
		}

		for (const menu of menus) {
			if (menu.items.length > kMostItems) {
				refuse(`${standIn.name}, row ${page.row} page ${page.page}'s menu ${menu.title} has ${menu.items.length} items: more than ${kMostItems}`);
			}
		}

		return { row: page.row, page: page.page, menus };
	});

	const first = pages.find((page) => page.row === 1 && page.page === 0);
	const second = pages.find((page) => page.row === 2 && page.page === 0);

	if (!first) {
		refuse(`${standIn.name} has no row 1 page 0 for game kind ${built.gameKind.value}`);
	}

	// The way back: Exit has to be on the bar the screen starts with, or the editor could never leave it
	if (!first.menus.some((menu) => menu.items.some((item) => item.name === 'Exit' && (item.work === 'hub' || item.work === 'exitmode')))) {
		refuse(`${standIn.name}'s first bar has no Exit`);
	}

	return { name: standIn.name, table: '', menus: first.menus, secondRow: second ? second.menus : [], pages };
}

// The pages of a built screen that has a table of its own, added to that screen (Track Editing): the same rules as a stand-in's pages, but no row 1 page 0 is needed (the table is the bar the screen
// starts with) and the pages are the live ones for the game kind
function attachBuiltPages(screen, s, built, status, screenOf, tag) {
	const source = built.screens.find((b) => b.screen === screen.name);

	if (!source) {
		refuse(`${screen.name} has no pages in the built menus file`);
	}

	screen.pages = livePages(source, built.gameKind.value).map((page) => {
		const menus = builtPageMenus(source, page, s, status, screenOf, tag);
		const most = page.row === 1 ? kMostMenus - kOursMenus : kMostMenus;

		if (menus.length === 0 || menus.length > most) {
			refuse(`${screen.name}, row ${page.row} page ${page.page} has ${menus.length} menus: not 1 to ${most}`);
		}

		for (const menu of menus) {
			if (menu.items.length > kMostItems) {
				refuse(`${screen.name}, row ${page.row} page ${page.page}'s menu ${menu.title} has ${menu.items.length} items: more than ${kMostItems}`);
			}
		}

		return { row: page.row, page: page.page, menus };
	});
}

// Turns the files into the screens: each table's titles and items, then the stand-ins
function build(menus, status, built) {
	const screenOf = (to) => {
		const table = menus.tables.findIndex((t) => t.address === to);

		if (table >= 0) {
			return table;
		}

		const standIn = status.standIns.findIndex((s) => s.name === to);

		if (standIn < 0) {
			refuse(`a screen item goes to ${to}, which is neither a table nor a stand-in`);
		}

		return kTableCount + standIn;
	};

	const itemOf = (table, command) => status.items.filter((i) => i.table === table && parseInt(i.command, 16) === command);
	const screens = [];
	const used = new Set();

	menus.tables.forEach((table, s) => {
		const screen = { name: status.screens[s].name, table: table.address, menus: [] };
		const records = table.records;

		// level: 0 an item, 1 an item that opens a CASCADE (inteng's kind 1 with kind 2 records under it; reassess/dialog-look-drake.md BAR-10), 2 a row of that cascade (kind 2)
		const addItem = (menu, name, r, command, words, level = 0) => {
			const said = level === 1 ? [] : itemOf(table.address, command);

			if (said.length > 1) {
				refuse(`table ${table.address}: the status file names command ${command} more than once`);
			}

			let tag = status.screens[s].tag;
			let work = 'none';
			let toScreen = -1;

			if (said.length === 1 && said[0].does) {
				tag = 'none';
				work = said[0].does;
				toScreen = work === 'screen' || work === 'screenwhite' ? screenOf(said[0].to) : -1;
			} else if (said.length === 1) {
				tag = said[0].tag;
			} else if (name === 'Exit') {
				// Every screen's own Exit is the mode's Exit: inteng's tear-down (the mode's write-back, the level saved, "Camera position saved") and then the hub (FUN_00447550)
				tag = 'none';
				work = 'exitmode';
			}

			if (level === 1) {
				tag = 'none';   // a cascade's own row picks nothing (BAR-12): no tag, no work
			}

			if (name.length + kTagWords[tag].length > kNameMost) {
				refuse(`table ${table.address}, record ${r}: "${name}${kTagWords[tag]}" is longer than ${kNameMost} letters`);
			}

			menu.items.push({ name, action: (s + 1) * 0x10000 + r, command, tag, work, toScreen, access: accessIn(name, words), level });
		};

		records.forEach((record, r) => {
			const command = parseInt(record.command, 16);

			if (record.kind === 0) {
				const menu = { title: shown(record.words), access: record.words.indexOf('&'), items: [] };
				const next = records[r + 1];

				screen.menus.push(menu);

				// A title with a command and nothing under it is a menu of one item with its words (ours until run 1)
				if (command !== 0 && (!next || next.kind === 0)) {
					addItem(menu, shown(record.words), r, command, record.words);
				}
			} else {
				const menu = screen.menus[screen.menus.length - 1];
				const next = records[r + 1];

				if (record.kind === 1 && next && next.kind === 2) {
					addItem(menu, shown(record.words), r, command, record.words, 1);   // a cascade's own row (BAR-10); its kind-2 rows follow it, level 2
					return;
				}

				if (record.words === '') {
					return;   // a blank line: the bar has no separators yet (ours until run 1), so it's left out
				}

				addItem(menu, shown(record.words), r, command, record.words, record.kind === 2 ? 2 : 0);
			}
		});

		// Our items put into this table (status "additions"), each after the item it names, in its menu
		(status.additions || []).forEach((addition, i) => {
			if (addition.table !== table.address) {
				return;
			}

			const at = `status addition ${i}`;
			const menu = screen.menus.find((m) => m.title === addition.title);
			const after = menu ? menu.items.findIndex((item) => item.name === addition.after && !item.level) : -1;
			const action = parseInt(addition.action, 16);

			if (after < 0) {
				refuse(`${at}: table ${table.address} has no menu "${addition.title}" with an item "${addition.after}" outside a cascade`);
			}

			if (screen.menus.some((m) => m.items.some((item) => item.action === action))) {
				refuse(`${at}: its action ${addition.action} is already an item's`);
			}

			const name = shown(addition.words);

			menu.items.splice(after + 1, 0, { name, action, command: 0, tag: 'none', work: addition.does, toScreen: -1, access: accessIn(name, addition.words), note: addition.note });
		});

		if (screen.menus.length > kMostMenus - kOursMenus) {
			refuse(`table ${table.address} has ${screen.menus.length} menus: more than the bar holds with the editor's own`);
		}

		for (const menu of screen.menus) {
			if (menu.items.length > kMostItems) {
				refuse(`table ${table.address}'s menu ${menu.title} has ${menu.items.length} items: more than ${kMostItems}`);
			}

			for (const item of menu.items) {
				used.add(item.action);
			}
		}

		screens.push(screen);
	});

	for (const name of kBuiltOnTable) {
		const s = screens.findIndex((screen) => screen.name === name && screen.table);

		if (s < 0) {
			refuse(`${name} is built on a table, and no table screen has that name`);
		}

		attachBuiltPages(screens[s], s, built, status, screenOf, status.screens[s].tag);
	}

	status.standIns.forEach((standIn, k) => {
		const s = kTableCount + k;
		const exit = { name: 'Exit', action: (s + 1) * 0x10000 + 0, command: 0, tag: 'none', work: 'exitmode', toScreen: -1 };

		if (standIn.built) {
			screens.push(buildBuiltScreen(standIn, s, built, status, screenOf));
			return;
		}

		if (!standIn.menus) {
			const items = [
				exit,
				{ name: 'Menu being mapped from inteng', action: (s + 1) * 0x10000 + 1, command: 0, tag: standIn.tag, work: 'none', toScreen: -1 },
			];

			screens.push({ name: standIn.name, table: '', menus: [{ title: 'Mode', items }] });
			return;
		}

		// Its own menus, then Mode with Exit (unless its menus hold an Exit). An item's number is its screen's, then its place among the screen's items (Exit is 0).
		let next = 1;
		const menus = standIn.menus.map((menu) => ({
			title: shown(menu.title),
			access: menu.title.indexOf('&'),
			items: menu.items.map((item) => {
				const name = shown(item.name);
				const tag = 'tag' in item ? item.tag : 'none';
				const work = 'does' in item ? item.does : 'none';

				if (name.length + kTagWords[tag].length > kNameMost) {
					refuse(`${standIn.name}: "${name}${kTagWords[tag]}" is longer than ${kNameMost} letters`);
				}

				return { name, action: (s + 1) * 0x10000 + next++, command: 'command' in item ? parseInt(item.command, 16) : 0, tag, work, toScreen: -1, access: accessIn(name, item.name) };
			}),
		}));

		// a stand-in whose own menus hold its Exit (inteng's own bar, as Acoustic's page 7: Mode > Exit) gets no second one
		if (!menus.some((menu) => menu.items.some((item) => item.name === 'Exit'))) {
			menus.push({ title: 'Mode', items: [exit] });
		}
		screens.push({ name: standIn.name, table: '', menus });
	});

	return screens;
}

// A text as a C++ string literal: "\" and the double quote escaped, and every "?" written "\?" so no "??" trigraph can form
function cpp(value) {
	return '"' + value.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\?/g, '\\?') + '"';
}

function emit(screens) {
	const out = [];
	const everyMenu = (screen) => [...screen.menus, ...(screen.pages ? screen.pages.flatMap((p) => p.menus) : [])];
	const longest = Math.max(...screens.flatMap((s) => everyMenu(s).flatMap((m) => m.items.map((i) => i.name.length + kTagWords[i.tag].length))));

	out.push('// GENERATED by tools/make-inteng-menus.js from docs/inteng/inteng-menus.json and docs/inteng/menu-status.json: never edit it by');
	out.push('// hand. Change those two files and run the tool; its --check (the gate runs it) says whether this file is still what they make.');
	out.push('// See inteng_menus.h. AVP_OpenInteng\'s own code: see License.txt, "AVP_OpenInteng\'s own code".');
	out.push('');
	out.push('#include "inteng_menus.h"');
	out.push('');
	out.push('#include "menu_bar.h"');
	out.push('');
	out.push('namespace host');
	out.push('{');
	out.push(`\tstatic_assert(${longest} <= ui::kMenuNameMost, "the longest name and its tag fit the menu bar");`);
	out.push(`\tstatic_assert(${kMostMenus} <= ui::kMenuMostMenus && ${kMostItems} <= ui::kMenuMostItems && ${kTagMost} <= ui::kMenuTagMost, "the tool's limits are the bar's");`);
	out.push('');
	out.push('\tnamespace');
	out.push('\t{');

	// One list of menus as the item arrays and the menu array, named by its tag (a screen's number, or its number, row and page)
	const emitMenus = (tag, menus) => {
		menus.forEach((menu, m) => {
			out.push(`\t\tIntengItem const kItems${tag}_${m}[] =`);
			out.push('\t\t{');

			if (menu.items.length === 0) {
				out.push('\t\t\t{ "", 0, 0, IntengTag::None, IntengWork::None, -1, -1 },   // none: an array can\'t be empty');
			}

			for (const item of menu.items) {
				if (item.note) {
					out.push(`\t\t\t// OURS, NAMED (docs/inteng/menu-status.json, additions): ${item.note}`);
				}

				out.push(`\t\t\t{ ${cpp(item.name)}, 0x${item.action.toString(16)}, 0x${item.command.toString(16)}, IntengTag::${kTagCpp[item.tag]}, IntengWork::${kWorkCpp[item.work]}, ${item.toScreen}, ${'access' in item ? item.access : -1}${item.level ? `, ${item.level}` : ''} },`);
			}

			out.push('\t\t};');
		});
		out.push(`\t\tIntengMenu const kMenus${tag}[] =`);
		out.push('\t\t{');

		menus.forEach((menu, m) => {
			out.push(`\t\t\t{ ${cpp(menu.title)}, kItems${tag}_${m}, ${menu.items.length}, ${'access' in menu ? menu.access : -1} },`);
		});

		out.push('\t\t};');
	};

	screens.forEach((screen, s) => {
		out.push(`\t\t// ${s}: ${screen.name}${screen.table ? ', inteng\'s table at ' + screen.table + (screen.pages ? ', and the pages inteng builds in code (docs/inteng/inteng-menus-built.json)' : '') : screen.pages ? ', a stand-in whose menus are the ones inteng builds in code (docs/inteng/inteng-menus-built.json)' : ', a stand-in for a menu inteng builds in code'}`);

		if (screen.table) {
			emitMenus(`${s}`, screen.menus);
		}

		if (screen.pages) {
			// every live page, in row and page order: row 1 page 0 is the screen's first bar and row 2 page 0 its second bar
			for (const page of screen.pages) {
				emitMenus(`${s}_${page.row}_${page.page}`, page.menus);
			}

			out.push(`\t\tIntengPage const kPages${s}[] =`);
			out.push('\t\t{');

			for (const page of screen.pages) {
				out.push(`\t\t\t{ ${page.row}, ${page.page}, kMenus${s}_${page.row}_${page.page}, ${page.menus.length} },`);
			}

			out.push('\t\t};');
		} else if (!screen.table) {
			emitMenus(`${s}`, screen.menus);
		}

		out.push('');
	});

	out.push('\t}');
	out.push('');
	out.push('\tIntengScreen const kIntengScreens[] =');
	out.push('\t{');

	screens.forEach((screen, s) => {
		if (screen.pages) {
			const second = screen.pages.find((p) => p.row === 2 && p.page === 0);
			const firstName = screen.table ? `kMenus${s}` : `kMenus${s}_1_0`;

			out.push(`\t\t{ ${cpp(screen.name)}, ${cpp(screen.table)}, ${firstName}, ${screen.menus.length}, ${second ? `kMenus${s}_2_0, ${second.menus.length}` : 'nullptr, 0'}, kPages${s}, ${screen.pages.length} },`);
		} else {
			out.push(`\t\t{ ${cpp(screen.name)}, ${cpp(screen.table)}, kMenus${s}, ${screen.menus.length}, nullptr, 0, nullptr, 0 },`);
		}
	});

	out.push('\t};');
	out.push('');
	out.push(`\tsize_t const kIntengScreenCount = sizeof(kIntengScreens) / sizeof(kIntengScreens[0]);`);
	out.push('}');
	out.push('');
	return out.join('\n');
}

function make(root) {
	const menus = readMenus(fs.readFileSync(path.join(root, 'docs', 'inteng', 'inteng-menus.json')), kMenusSha256);
	const status = readStatus(fs.readFileSync(path.join(root, 'docs', 'inteng', 'menu-status.json')), menus);

	const built = readBuilt(fs.readFileSync(path.join(root, 'docs', 'inteng', 'inteng-menus-built.json')), kBuiltSha256);

	return emit(build(menus, status, built));
}

module.exports = { readMenus, readBuilt, livePages, readStatus, build, emit, cpp, Refused, kMenusSha256, kBuiltSha256, kBuiltScreens };

if (require.main === module) {
	const root = path.join(__dirname, '..');
	const target = path.join(root, 'src', 'host', 'inteng_menus.cpp');
	const check = process.argv.includes('--check');

	try {
		const made = make(root);

		if (check) {
			const now = fs.existsSync(target) ? fs.readFileSync(target, 'utf8') : '';

			if (now !== made) {
				console.error('make-inteng-menus: src/host/inteng_menus.cpp is NOT what the two files make: run the tool and commit what it writes');
				process.exit(1);
			}

			console.log('make-inteng-menus: src/host/inteng_menus.cpp is what the two files make');
		} else {
			fs.writeFileSync(target, made);
			console.log('make-inteng-menus: wrote src/host/inteng_menus.cpp');
		}
	} catch (e) {
		if (e instanceof Refused) {
			console.error(`make-inteng-menus: refused: ${e.message}`);
			process.exit(1);
		}

		throw e;
	}
}
