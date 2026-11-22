// The test for tools/make-edge.js's refusals: a Hudson hook whose "off" answer the script can't be sure of must stop it, with that
// hook's name, and write nothing. Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHY: make-edge.js reads each Hudson hook's answer out of the game's source by four plain shapes (its step 7), and refuses anything
// else. The real game's 66 hooks only show that the shapes it accepts are read right (tests/edge_asleep_test.cpp). This shows the
// other half: the ones it must refuse are refused. Each case is a made-up game, in a folder of its own that this test makes and then
// deletes: the network files the script expects (empty but for one network function, which every log must name), one Hudson file
// with the hook under test, and a linker log that names both.
//
// The second off-flag (Frost 2026-11-21, for AvPx's SAY marks: "if ((!HudsonRunIsOn && !SayMarksAreOn) || ...)"): shape (a) also reads a
// switch other than HudsonRunIsOn, but only one of Hudson's files starts false and nothing outside Hudson's files names. The cases below
// show both halves: such a switch is read, and one that starts true, that another file names, or that is asked about any other way, is
// refused.
//
// Run from the repository's root: node tests/make_edge_test.js. It answers 0 when every check passed.

'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const script = path.join(__dirname, '..', 'tools', 'make-edge.js');
const networkFiles = [
	'3dc/avp/win95/pldnet.cpp', '3dc/networking.cpp', '3dc/net_close.cpp', '3dc/relay_list.cpp', '3dc/relay_list_text.cpp',
	'3dc/relay_auto.cpp', '3dc/net_host.cpp', '3dc/net_crypto_cng.cpp', '3dc/net_identity_store.cpp', '3dc/net_handshake_a.cpp',
	'3dc/servers_cfg.cpp', '3dc/servers_cfg_file.cpp', '3dc/servers_trust.cpp', '3dc/net_server_key.cpp', '3dc/net_pq_native.cpp',
	'3dc/pq_key_exchange.cpp', '3dc/pq_signature_check.cpp', '3dc/avp/win95/frontend/online_screens_page.cpp',
	'3dc/avp/win95/frontend/online_page.cpp', '3dc/avp/win95/frontend/server_yes_page.cpp', '3dc/avp/win95/winmain.cpp'];

let failed = 0;
function check(good, what) {
	console.log('  ' + (good ? 'ok' : 'FAILED') + '  ' + what);
	if (!good) failed++;
}

// One made-up game with one Hudson hook, run through the script. Gives back its answer, what it said, and the files it wrote. given, when
// there, is the script's three paths instead of the made-up game's own. files, when there, are more files of the made-up game, each path
// from its root (as "avpx/3dc/avp/console.cpp" or "host/ui.cpp") with its text.
function run(hookSource, hookSignature, hookDecorated, given, files) {
	const root = fs.mkdtempSync(path.join(os.tmpdir(), 'make-edge-test-'));
	try {
		const avpx = path.join(root, 'avpx');
		for (const rel of networkFiles) {
			fs.mkdirSync(path.dirname(path.join(avpx, rel)), { recursive: true });
			fs.writeFileSync(path.join(avpx, rel), rel === '3dc/networking.cpp' ? 'void Net_Something()\n{\n}\n' : '');
		}
		fs.mkdirSync(path.join(avpx, '3dc', 'avp'), { recursive: true });
		fs.writeFileSync(path.join(avpx, '3dc', 'avp', 'hudson.cpp'), 'bool HudsonRunIsOn = false;\n\n' + hookSource + '\n');
		fs.writeFileSync(path.join(avpx, '3dc', 'avp', 'hudson.h'), 'void Net_Something();\n');
		const host = path.join(root, 'host');
		fs.mkdirSync(path.join(host, 'edge'), { recursive: true });
		fs.writeFileSync(path.join(host, 'edge_media.cpp'), '// nothing is left for this file in a made-up game\n');
		fs.writeFileSync(path.join(host, 'edge_own.cpp'), '// nor for this one\n');
		for (const [rel, text] of Object.entries(files || {})) {
			fs.mkdirSync(path.dirname(path.join(root, rel)), { recursive: true });
			fs.writeFileSync(path.join(root, rel), text);
		}
		const log = path.join(root, 'link.log');
		fs.writeFileSync(log,
			'a.obj : error LNK2019: unresolved external symbol "void __cdecl Net_Something(void)" (?Net_Something@@YAXXZ) referenced in function x\n' +
			'b.obj : error LNK2019: unresolved external symbol "' + hookSignature + '" (' + hookDecorated + ') referenced in function y\n');
		// run from the made-up game's folder, as the script is run from the tree's root: it takes only paths inside the folder it runs in
		const result = spawnSync(process.execPath, [script, ...(given || ['link.log', 'avpx', 'host/edge'])], { cwd: root, encoding: 'utf8' });
		const written = fs.readdirSync(path.join(host, 'edge'));
		const asleep = written.includes('asleep_hudson.cpp') ? fs.readFileSync(path.join(host, 'edge', 'asleep_hudson.cpp'), 'utf8') : '';
		return { status: result.status, said: (result.stdout || '') + (result.stderr || ''), written, asleep };
	} finally {
		fs.rmSync(root, { recursive: true, force: true });
	}
}

// First the control: a hook of a shape the script reads, so the made-up game itself is known to work and a refusal below is the hook's
const good = run('int Hudson_Good()\n{\n\treturn HudsonRunIsOn ? 1 : 2;\n}', 'int __cdecl Hudson_Good(void)', '?Hudson_Good@@YAHXZ');
check(good.status === 0 && /return 2;/.test(good.asleep), 'the control: a hook of a shape the script reads is written asleep, answering 2');

// A path that leads out of the folder the script runs in is refused before anything is read or written (tools/tree-path.js): the
// same made-up game, its log named through "..", and the edge folder as an absolute path somewhere else
const goodHook = ['int Hudson_Good()\n{\n\treturn HudsonRunIsOn ? 1 : 2;\n}', 'int __cdecl Hudson_Good(void)', '?Hudson_Good@@YAHXZ'];
const upAndOut = run(...goodHook, ['../link.log', 'avpx', 'host/edge']);
check(upAndOut.status === 2 && upAndOut.said.includes('The link log') && upAndOut.written.length === 0,
	'refused, naming the log, with nothing written: a log named through ".."');
const elsewhere = run(...goodHook, ['link.log', 'avpx', path.join(os.tmpdir(), 'edge')]);
check(elsewhere.status === 2 && elsewhere.said.includes('The edge folder') && elsewhere.written.length === 0,
	'refused, naming the edge folder, with nothing written: an edge folder outside the folder it runs in');

const cases = [
	{
		what: 'a "return HudsonRunIsOn && a, b;" hook, which gives b whatever HudsonRunIsOn is',
		source: 'bool Hudson_Comma()\n{\n\treturn HudsonRunIsOn && a, b;\n}', signature: 'bool __cdecl Hudson_Comma(void)', decorated: '?Hudson_Comma@@YA_NXZ',
		name: 'Hudson_Comma',
	},
	{
		what: 'an "if (!HudsonRunIsOn || a, b)" hook, which asks only about b',
		source: 'void Hudson_IfComma()\n{\n\tif (!HudsonRunIsOn || a, b)\n\t{\n\t\treturn;\n\t}\n\tDoSomething();\n}',
		signature: 'void __cdecl Hudson_IfComma(void)', decorated: '?Hudson_IfComma@@YAXXZ', name: 'Hudson_IfComma',
	},
	{
		what: 'an "if (HudsonRunIsOn && a, b)" hook, which runs its block whenever b is set',
		source: 'void Hudson_OnComma()\n{\n\tif (HudsonRunIsOn && a, b)\n\t{\n\t\tDoSomething();\n\t}\n}',
		signature: 'void __cdecl Hudson_OnComma(void)', decorated: '?Hudson_OnComma@@YAXXZ', name: 'Hudson_OnComma',
	},
	{
		what: 'a hook with a /* */ comment, which the script doesn\'t cut away',
		source: 'int Hudson_Block()\n{\n\t/* { */\n\treturn HudsonRunIsOn ? 1 : 2;\n}', signature: 'int __cdecl Hudson_Block(void)', decorated: '?Hudson_Block@@YAHXZ',
		name: 'Hudson_Block',
	},
	{
		what: 'a "return HudsonRunIsOn && a || b;" hook, which gives b when HudsonRunIsOn is false',
		source: 'bool Hudson_Or()\n{\n\treturn HudsonRunIsOn && a || b;\n}', signature: 'bool __cdecl Hudson_Or(void)', decorated: '?Hudson_Or@@YA_NXZ',
		name: 'Hudson_Or',
	},
	{
		what: 'a hook that does something before it asks about HudsonRunIsOn',
		source: 'void Hudson_First()\n{\n\tDoSomething();\n\tif (!HudsonRunIsOn)\n\t{\n\t\treturn;\n\t}\n}', signature: 'void __cdecl Hudson_First(void)',
		decorated: '?Hudson_First@@YAXXZ', name: 'Hudson_First',
	},
];
for (const c of cases) {
	const r = run(c.source, c.signature, c.decorated);
	check(r.status === 1 && r.said.includes(c.name) && r.written.length === 0, 'refused, by its name, with nothing written: ' + c.what);
}

// THE SECOND OFF-FLAG. Read: a switch one of Hudson's files starts false and nothing outside them names, asked about as shape (a) asks,
// alone or with HudsonRunIsOn, both false. Each "off" block here only returns, so the hook is written asleep doing nothing.
const flagFalse = 'bool SayMarksAreOn = false;\n\n';
const bothOff = flagFalse + 'void Hudson_BothOff(int *p)\n{\n\tif ((!HudsonRunIsOn && !SayMarksAreOn) || !p)\n\t{\n\t\treturn;\n\t}\n\t*p = 1;\n}';
const bothOffHook = [bothOff, 'void __cdecl Hudson_BothOff(int *)', '?Hudson_BothOff@@YAXPAH@Z'];
const read = [
	{ what: 'an "if ((!HudsonRunIsOn && !SayMarksAreOn) || ...)" hook, both switches starting false', hook: bothOffHook, name: 'Hudson_BothOff' },
	{
		what: 'an "if (!SayMarksAreOn || HudsonRunIsOn)" hook, the second switch alone',
		hook: [flagFalse + 'void Hudson_SecondOff()\n{\n\tif (!SayMarksAreOn || HudsonRunIsOn)\n\t{\n\t\treturn;\n\t}\n\tDoSomething();\n}',
			'void __cdecl Hudson_SecondOff(void)', '?Hudson_SecondOff@@YAXXZ'], name: 'Hudson_SecondOff',
	},
];
for (const c of read) {
	const r = run(...c.hook);
	const body = (new RegExp(c.name + '\\([^)]*\\)\\n\\{\\n([\\s\\S]*?)\\}').exec(r.asleep) || [])[1];
	check(r.status === 0 && body === '\treturn;\n', 'written asleep, doing nothing but return: ' + c.what);
}

// Refused: the same hook when the second switch doesn't start false, when another file of the game or one of the editor's names it (so
// something the editor compiles could set it), or when it's asked about in a way a false switch doesn't settle
const flagRefused = [
	{
		what: 'the second switch starts true', name: 'Hudson_BothOff',
		hook: [bothOff.replace('SayMarksAreOn = false', 'SayMarksAreOn = true'), bothOffHook[1], bothOffHook[2]],
	},
	{
		what: 'the second switch is defined in no Hudson file', name: 'Hudson_BothOff',
		hook: [bothOff.replace(flagFalse, ''), bothOffHook[1], bothOffHook[2]],
	},
	{
		what: 'another game file sets the second switch', name: 'Hudson_BothOff', hook: bothOffHook,
		files: { 'avpx/3dc/avp/console.cpp': 'void TurnTheMarksOn()\n{\n\tSayMarksAreOn = true;\n}\n' },
		says: 'SayMarksAreOn is no off-flag: avpx/3dc/avp/console.cpp:3 names it',
	},
	{
		what: 'a game header names the second switch', name: 'Hudson_BothOff', hook: bothOffHook,
		files: { 'avpx/3dc/avp/console.h': 'extern bool SayMarksAreOn;\n' },
		says: 'SayMarksAreOn is no off-flag: avpx/3dc/avp/console.h:1 names it',
	},
	{
		what: 'a file of the editor\'s own names the second switch', name: 'Hudson_BothOff', hook: bothOffHook,
		files: { 'host/ui.cpp': 'bool MarksShown()\n{\n\treturn SayMarksAreOn;\n}\n' },
		says: 'SayMarksAreOn is no off-flag: host/ui.cpp:3 names it',
	},
	{
		what: 'an "if ((!HudsonRunIsOn || !SayMarksAreOn) && ...)" hook, which the rest of the condition decides', name: 'Hudson_OrThenAnd',
		hook: [flagFalse + 'void Hudson_OrThenAnd(int *p)\n{\n\tif ((!HudsonRunIsOn || !SayMarksAreOn) && p)\n\t{\n\t\treturn;\n\t}\n\t*p = 1;\n}',
			'void __cdecl Hudson_OrThenAnd(int *)', '?Hudson_OrThenAnd@@YAXPAH@Z'],
	},
	{
		what: 'an "if ((!HudsonRunIsOn && SayMarksAreOn) || ...)" hook, false with both switches off', name: 'Hudson_NoNot',
		hook: [flagFalse + 'void Hudson_NoNot(int *p)\n{\n\tif ((!HudsonRunIsOn && SayMarksAreOn) || !p)\n\t{\n\t\treturn;\n\t}\n\t*p = 1;\n}',
			'void __cdecl Hudson_NoNot(int *)', '?Hudson_NoNot@@YAXPAH@Z'],
	},
	{
		what: 'an "if (!SayMarksAreOn && ...)" hook, which the rest of the condition decides', name: 'Hudson_AndAfter',
		hook: [flagFalse + 'void Hudson_AndAfter(int *p)\n{\n\tif (!SayMarksAreOn && p)\n\t{\n\t\treturn;\n\t}\n\t*p = 1;\n}',
			'void __cdecl Hudson_AndAfter(int *)', '?Hudson_AndAfter@@YAXPAH@Z'],
	},
];
// A case with "says" must also give that reason: the file that names the switch, so it's refused for that and not for some other shape
for (const c of flagRefused) {
	const r = run(...c.hook, undefined, c.files);
	check(r.status === 1 && r.said.includes(c.name) && r.written.length === 0 && (!c.says || r.said.includes(c.says)),
		'refused, by its name, with nothing written: ' + c.what + (c.says ? ', saying "' + c.says + '"' : ''));
}

console.log(failed ? 'SOME CHECKS FAILED' : 'All checks passed');
process.exit(failed ? 1 : 0);
