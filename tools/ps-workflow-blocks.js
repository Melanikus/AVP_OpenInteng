// ps-workflow-blocks.js: pulls the PowerShell written INSIDE GitHub's workflow files out into files PSScriptAnalyzer can read. Written 2026-11-22
// by Melanikus (Frost) for AVP_OpenInteng, Bishop 2's ruling: "INLINE PowerShell in workflows: CLOSE IT, with no new tool". AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: a workflow step's `run:` text is a script GitHub runs, but it lives in a .yml file, so no scanner that reads .ps1 files ever sees it (Snyk
// Code reads no PowerShell at all). This finds every step whose script runs in PowerShell:
//   - a step that says `shell: pwsh` or `shell: powershell` (or a command line starting with either);
//   - a step that names no shell, in a job whose `defaults: run: shell:` or the workflow's says one of those;
//   - a step that names no shell anywhere, in a job that runs on Windows: GitHub runs those in pwsh;
//   - the same in a composite action (.github/actions/<name>/action.yml), where every step names its shell.
// Each script is written to .ps-scan-blocks/<n>.ps1, and map.json says which workflow, job, step and line it came from, so a finding on the
// script's line L is the workflow's line (first script line + L - 1). GitHub's own `${{ ... }}` expressions are filled in by GitHub before
// PowerShell reads the script, so PowerShell can't parse them: each becomes a variable `${GhExpr<n>}` (braced, so it parses wherever the
// expression stood, inside a string or out), with as many line breaks as it had, so the lines stay where they were.
// A file or a step this can't read for sure is never skipped: it goes into errors.json, and the count calls it a SCAN-ERROR (a blind spot fails).
//
// Use, from the repository's root (no path is typed: it reads the fixed folders below, and writes only the fresh folder .ps-scan-blocks):
//   node tools/ps-workflow-blocks.js
// It reads .github/workflows/*.yml and .yaml, and .github/actions/*/action.yml and .yaml, in the repository and in avpx/ (the AvPx pin).
//
// THE WORKFLOW FILES ARE READ AS HOSTILE (Tao 3): a pull request can change them. The YAML reader below knows only the forms GitHub's workflow
// files here use (mappings, lists, plain and quoted values, | blocks, one-line [..] and {..} values) and refuses anything else: a tab, an
// anchor or alias, a folded > block, a document marker, a key twice, a line indented where nothing can start. A file it refuses is a SCAN-ERROR.
'use strict';

const fs = require('fs');
const path = require('path');

const kOut = '.ps-scan-blocks';
const kMostBytes = 4 * 1024 * 1024;   // the biggest workflow here is about 60 KB

class Refused extends Error {}

// ---- THE YAML READER. Every node keeps the line (from 1) it starts on.
//   { kind: 'map', line, entries: Map(key -> node) }   { kind: 'seq', line, items: [node] }
//   { kind: 'scalar', line, value, plainLines }          a one-line value (plainLines > 1: a plain value folded from more lines)
//   { kind: 'block', line, firstLine, lines, style }     a | block: its lines with the block's indentation taken off; firstLine is its first line's number
function readYaml(text) {
	if (text.includes('\t')) {
		throw new Refused('a tab: YAML refuses tabs for indentation, and this reader refuses them anywhere rather than guess');
	}

	const lines = text.replace(/^\uFEFF/, '').split(/\r?\n/);
	let at = 0;

	const indentOf = (line) => line.length - line.trimStart().length;
	const blank = (line) => line.trim() === '';
	const comment = (line) => line.trim().startsWith('#');
	const where = () => `line ${at + 1}`;
	const skip = () => {
		while (at < lines.length && (blank(lines[at]) || comment(lines[at]))) {
			at++;
		}
	};
	// "key: value" (the value may be empty); keys here are plain words, or quoted
	const keyLine = /^("(?:[^"\\]|\\.)*"|'(?:[^']|'')*'|[^\s#'"\-?:,[\]{}&*!|>%@`][^:#]*?|-[^\s:#][^:#]*?)\s*:(?:\s+(.*))?$/;

	function unquoteKey(key) {
		if (key.startsWith('"')) {
			return JSON.parse(key);
		}

		if (key.startsWith("'")) {
			return key.slice(1, -1).replace(/''/g, "'");
		}

		return key;
	}

	function refuseForms(body) {
		if (/^(---|\.\.\.)(\s|$)/.test(body) || body.startsWith('%')) {
			throw new Refused(`${where()}: a document marker or directive`);
		}

		if (/(^|[\s:[{,])[&*][A-Za-z0-9_-]/.test(body.replace(/"(?:[^"\\]|\\.)*"|'(?:[^']|'')*'/g, '""')) || /^<<\s*:/.test(body)) {
			throw new Refused(`${where()}: an anchor, alias or merge key`);
		}
	}

	// A value written on the key's own line (or a list item's): quoted, a one-line [..] or {..}, or plain (which may go on over more indented lines)
	function inlineScalar(textHere, ownerIndent) {
		const line = at + 1;
		let value;

		if (textHere.startsWith('"')) {
			const m = /^"((?:[^"\\]|\\.)*)"\s*(#.*)?$/.exec(textHere);

			if (!m) {
				throw new Refused(`${where()}: a double-quoted value that doesn't close on its line`);
			}

			value = JSON.parse('"' + m[1].replace(/\\\//g, '/') + '"');
		} else if (textHere.startsWith("'")) {
			const m = /^'((?:[^']|'')*)'\s*(#.*)?$/.exec(textHere);

			if (!m) {
				throw new Refused(`${where()}: a single-quoted value that doesn't close on its line`);
			}

			value = m[1].replace(/''/g, "'");
		} else if (textHere.startsWith('[') || textHere.startsWith('{')) {
			// one-line flow values ([main], {..}): kept as their text; nothing this reads looks inside them
			const close = textHere.startsWith('[') ? ']' : '}';
			const cut = textHere.replace(/\s+#.*$/, '').trimEnd();

			if (!cut.endsWith(close)) {
				throw new Refused(`${where()}: a [..] or {..} value that doesn't close on its line`);
			}

			value = cut;
		} else {
			value = textHere.replace(/\s+#.*$/, '').trimEnd();
		}

		at++;

		// a plain value goes on over the following lines that are indented more than its key (YAML folds them in with spaces)
		let plainLines = 1;

		if (!textHere.startsWith('"') && !textHere.startsWith("'")) {
			while (at < lines.length && !blank(lines[at]) && indentOf(lines[at]) > ownerIndent && !comment(lines[at])) {
				value += ' ' + lines[at].trim().replace(/\s+#.*$/, '');
				plainLines++;
				at++;
			}
		}

		return { kind: 'scalar', line, value, plainLines };
	}

	// A | block (> is refused: what runs is its lines joined, so its line numbers would be wrong)
	function blockScalar(header, ownerIndent) {
		const line = at + 1;
		const m = /^([|>])([+-]?)([1-9]?)([+-]?)\s*(#.*)?$/.exec(header);

		if (!m || (m[2] && m[4])) {
			throw new Refused(`${where()}: a block header this can't read: ${header}`);
		}

		if (m[1] === '>') {
			throw new Refused(`${where()}: a folded (>) block`);
		}

		at++;

		// the block's indentation: given (|2), or its first non-blank line's
		let first = at;

		while (first < lines.length && blank(lines[first])) {
			first++;
		}

		const blockIndent = m[3] ? ownerIndent + Number(m[3]) : (first < lines.length ? indentOf(lines[first]) : ownerIndent + 1);

		if (blockIndent <= ownerIndent) {
			return { kind: 'block', line, firstLine: line + 1, lines: [], style: '|' };
		}

		const firstLine = at + 1;
		const body = [];

		while (at < lines.length) {
			const here = lines[at];

			if (blank(here)) {
				body.push('');
				at++;
				continue;
			}

			const indent = indentOf(here);

			if (indent < blockIndent) {
				if (indent > ownerIndent) {
					throw new Refused(`${where()}: a line of a | block indented less than its first line`);
				}

				break;
			}

			body.push(here.slice(blockIndent));
			at++;
		}

		// the blank lines after the last line belong to no script
		while (body.length && body[body.length - 1] === '') {
			body.pop();
		}

		return { kind: 'block', line, firstLine, lines: body, style: '|' };
	}

	// What follows a key with nothing after its colon: a more indented node, a list at the key's own indentation, or nothing
	function childOf(ownerIndent) {
		const before = at;

		skip();

		if (at >= lines.length) {
			return { kind: 'scalar', line: before, value: '', plainLines: 1 };
		}

		const indent = indentOf(lines[at]);
		const body = lines[at].slice(indent);

		if (indent > ownerIndent) {
			return node(indent);
		}

		if (indent === ownerIndent && (body === '-' || body.startsWith('- '))) {
			return sequence(indent);
		}

		return { kind: 'scalar', line: before, value: '', plainLines: 1 };
	}

	function value(rest, ownerIndent) {
		if (rest === undefined || rest === '' || rest.startsWith('#')) {
			at++;
			return childOf(ownerIndent);
		}

		if (rest.startsWith('|') || rest.startsWith('>')) {
			return blockScalar(rest, ownerIndent);
		}

		return inlineScalar(rest, ownerIndent);
	}

	function mapping(indent) {
		const result = { kind: 'map', line: at + 1, entries: new Map() };

		for (;;) {
			skip();

			if (at >= lines.length) {
				break;
			}

			const here = lines[at];
			const ownIndent = indentOf(here);

			if (ownIndent < indent) {
				break;
			}

			if (ownIndent > indent) {
				throw new Refused(`${where()}: indented more than the mapping it is in`);
			}

			const body = here.slice(ownIndent);

			refuseForms(body);

			if (body === '-' || body.startsWith('- ')) {
				break;   // a list at this indentation belongs to the key above, which took it already: the caller decides
			}

			const m = keyLine.exec(body);

			if (!m) {
				throw new Refused(`${where()}: not a "key: value" line: ${body.slice(0, 80)}`);
			}

			const key = unquoteKey(m[1].trimEnd());

			if (result.entries.has(key)) {
				throw new Refused(`${where()}: the key "${key}" twice in one mapping`);
			}

			result.entries.set(key, value(m[2], ownIndent));
		}

		return result;
	}

	function sequence(indent) {
		const result = { kind: 'seq', line: at + 1, items: [] };

		for (;;) {
			skip();

			if (at >= lines.length) {
				break;
			}

			const here = lines[at];
			const ownIndent = indentOf(here);

			if (ownIndent < indent) {
				break;
			}

			if (ownIndent > indent) {
				throw new Refused(`${where()}: indented more than the list it is in`);
			}

			const body = here.slice(ownIndent);

			refuseForms(body);

			if (!(body === '-' || body.startsWith('- '))) {
				break;
			}

			if (body === '-') {
				at++;
				result.items.push(childOf(ownIndent));
				continue;
			}

			// "- key: value" starts a mapping at the column of the key: the dash is read as a space, and the mapping read from there
			const rest = body.slice(1).trimStart();
			const column = ownIndent + (body.length - rest.length);

			if (keyLine.test(rest) || rest.startsWith('- ')) {
				lines[at] = ' '.repeat(column) + rest;
				result.items.push(node(column));
			} else if (rest.startsWith('|') || rest.startsWith('>')) {
				result.items.push(blockScalar(rest, ownIndent));
			} else {
				result.items.push(inlineScalar(rest, ownIndent));
			}
		}

		return result;
	}

	function node(indent) {
		skip();

		const body = lines[at].slice(indentOf(lines[at]));

		return body === '-' || body.startsWith('- ') ? sequence(indent) : mapping(indent);
	}

	skip();

	if (at >= lines.length) {
		throw new Refused('the file is empty');
	}

	if (indentOf(lines[at]) !== 0) {
		throw new Refused(`${where()}: the file's first line is indented`);
	}

	const root = node(0);

	skip();

	if (at < lines.length) {
		throw new Refused(`${where()}: a line the reader could not place (its indentation)`);
	}

	return root;
}

// ---- WHICH STEPS RUN IN POWERSHELL
const get = (map, key) => (map && map.kind === 'map' ? map.entries.get(key) : undefined);
const text = (n) => (n && n.kind === 'scalar' ? n.value : undefined);
const isPowerShell = (shell) => /^(pwsh|powershell)(\.exe)?(\s|$)/i.test(shell);

function shellOfDefaults(owner) {
	return text(get(get(get(owner, 'defaults'), 'run'), 'shell'));
}

// Where a job runs: 'windows', 'other', or 'unknown' (an expression, or a form this doesn't read), from its runs-on
function systemOf(job) {
	const runsOn = get(job, 'runs-on');
	const labels = [];

	if (runsOn && runsOn.kind === 'scalar') {
		labels.push(runsOn.value);
	} else if (runsOn && runsOn.kind === 'seq') {
		runsOn.items.forEach((item) => labels.push(text(item) || '${{'));
	} else {
		return 'unknown';
	}

	if (labels.some((label) => label.includes('${{') || label.startsWith('[') || label.startsWith('{'))) {
		return 'unknown';
	}

	return labels.some((label) => /windows/i.test(label)) ? 'windows' : 'other';
}

// The script of a step's run: as the lines GitHub hands PowerShell, with the workflow line of its first line
function scriptOf(run) {
	if (run.kind === 'block') {
		return { lines: run.lines, firstLine: run.firstLine, kind: 'block', folded: false };
	}

	if (run.kind === 'scalar') {
		return { lines: [run.value], firstLine: run.line, kind: 'line', folded: run.plainLines > 1 };
	}

	return null;
}

// GitHub's ${{ ... }} become ${GhExpr<n>}, keeping their line breaks, so PowerShell parses the script and its lines stay in place
function placeholders(script) {
	let n = 0;
	const replaced = script.replace(/\$\{\{[\s\S]*?\}\}/g, (expression) => '${GhExpr' + (++n) + '}' + '\n'.repeat((expression.match(/\n/g) || []).length));

	if (replaced.includes('${{')) {
		throw new Refused('a ${{ that never closes');
	}

	return { script: replaced, expressions: n };
}

// Every PowerShell step of one file: { blocks: [{ job, step, firstLine, script, expressions, folded }], errors: [{ line, why }] }
function powerShellOf(yamlText, isAction) {
	const blocks = [];
	const errors = [];
	const root = readYaml(yamlText);

	function take(stepNode, job, stepIndex, defaultShell) {
		if (!stepNode || stepNode.kind !== 'map') {
			errors.push({ line: stepNode ? stepNode.line : 0, why: `${job}, step ${stepIndex + 1}: not a mapping` });
			return;
		}

		const run = get(stepNode, 'run');

		if (!run) {
			return;   // a `uses:` step: an action, not a script
		}

		const name = text(get(stepNode, 'name')) || `step ${stepIndex + 1}`;
		const named = get(stepNode, 'shell');

		// a shell named by an expression (shell: ${{ matrix.shell }}) can't be told before the run: never passed over as "not PowerShell"
		if (named && (named.kind !== 'scalar' || named.value.includes('${{'))) {
			errors.push({ line: named.line, why: `${job}, ${name}: its shell is an expression or a form this doesn't read` });
			return;
		}

		const shell = text(named) || defaultShell;

		if (shell === undefined) {
			errors.push({ line: run.line, why: `${job}, ${name}: its shell can't be told (the job's runs-on is an expression or a form this doesn't read)` });
			return;
		}

		if (!isPowerShell(shell)) {
			return;
		}

		const script = scriptOf(run);

		if (!script) {
			errors.push({ line: run.line, why: `${job}, ${name}: its run: is not a script` });
			return;
		}

		try {
			const filled = placeholders(script.lines.join('\n'));

			blocks.push({ job, step: name, firstLine: script.firstLine, kind: script.kind, script: filled.script + '\n', expressions: filled.expressions, folded: script.folded });
		} catch (e) {
			if (!(e instanceof Refused)) {
				throw e;
			}

			errors.push({ line: run.line, why: `${job}, ${name}: ${e.message}` });
		}
	}

	if (root.kind !== 'map') {
		throw new Refused('the file is not a mapping at its top');
	}

	if (isAction) {
		const runs = get(root, 'runs');

		if (text(get(runs, 'using')) !== 'composite') {
			return { blocks, errors };   // a JavaScript or Docker action has no steps of ours
		}

		const steps = get(runs, 'steps');

		(steps && steps.kind === 'seq' ? steps.items : []).forEach((step, i) => take(step, 'composite action', i, undefined));
		return { blocks, errors };
	}

	const jobs = get(root, 'jobs');

	if (!jobs || jobs.kind !== 'map') {
		throw new Refused('no jobs: mapping');
	}

	const workflowShell = shellOfDefaults(root);

	for (const [jobId, job] of jobs.entries) {
		const steps = get(job, 'steps');

		if (!steps) {
			continue;   // a job that calls another workflow (uses:) has no steps here
		}

		if (steps.kind !== 'seq') {
			errors.push({ line: steps.line, why: `${jobId}: steps: is not a list` });
			continue;
		}

		const system = systemOf(job);
		const fromDefaults = shellOfDefaults(job) || workflowShell;
		const defaultShell = fromDefaults || (system === 'windows' ? 'pwsh' : (system === 'other' ? 'bash' : undefined));

		steps.items.forEach((step, i) => take(step, jobId, i, defaultShell));
	}

	return { blocks, errors };
}

// ---- THE FILES: the fixed folders, read from the repository's root
function workflowFiles(root) {
	const files = [];
	const listed = (folder) => {
		try {
			return fs.readdirSync(folder, { withFileTypes: true });
		} catch (e) {
			return [];
		}
	};

	for (const base of ['', 'avpx']) {
		const github = path.join(root, base, '.github');

		for (const entry of listed(path.join(github, 'workflows'))) {
			if (entry.isFile() && /^[A-Za-z0-9._-]+\.ya?ml$/.test(entry.name)) {
				files.push({ name: path.posix.join(base, '.github/workflows', entry.name), full: path.join(github, 'workflows', entry.name), action: false });
			}
		}

		for (const folder of listed(path.join(github, 'actions'))) {
			if (!folder.isDirectory() || !/^[A-Za-z0-9._-]+$/.test(folder.name)) {
				continue;
			}

			for (const entry of listed(path.join(github, 'actions', folder.name))) {
				if (entry.isFile() && /^action\.ya?ml$/.test(entry.name)) {
					files.push({ name: path.posix.join(base, '.github/actions', folder.name, entry.name), full: path.join(github, 'actions', folder.name, entry.name), action: true });
				}
			}
		}
	}

	return files.sort((a, b) => (a.name < b.name ? -1 : 1));
}

function main() {
	if (process.argv.length > 2) {
		console.log('usage: from the repository\'s root: node tools/ps-workflow-blocks.js (it takes no words)');
		process.exit(2);
	}

	const root = process.cwd();
	const out = path.join(root, kOut);

	if (fs.existsSync(out)) {
		console.log(`ps-workflow-blocks: REFUSED: ${kOut} is there already; it is made fresh each run`);
		process.exit(1);
	}

	const files = workflowFiles(root);

	if (files.length === 0) {
		console.log('ps-workflow-blocks: REFUSED: no workflow files found (run it from the repository\'s root)');
		process.exit(1);
	}

	fs.mkdirSync(out);

	const map = [];
	const errors = [];

	for (const file of files) {
		let found;

		try {
			const stat = fs.lstatSync(file.full);

			if (!stat.isFile() || stat.size > kMostBytes) {
				throw new Refused(stat.isFile() ? `${stat.size} bytes, more than ${kMostBytes}` : 'not a plain file');
			}

			found = powerShellOf(fs.readFileSync(file.full, 'utf8'), file.action);
		} catch (e) {
			if (!(e instanceof Refused)) {
				throw e;
			}

			errors.push({ workflow: file.name, line: 0, why: e.message });
			console.log(`ps-workflow-blocks: SCAN-ERROR ${file.name}: ${e.message}`);
			continue;
		}

		for (const block of found.blocks) {
			const name = `${map.length + 1}.ps1`;

			fs.writeFileSync(path.join(out, name), block.script);
			map.push({ file: name, workflow: file.name, job: block.job, step: block.step, firstLine: block.firstLine, kind: block.kind, expressions: block.expressions, folded: block.folded });
		}

		for (const error of found.errors) {
			errors.push({ workflow: file.name, line: error.line, why: error.why });
			console.log(`ps-workflow-blocks: SCAN-ERROR ${file.name}:${error.line}: ${error.why}`);
		}

		console.log(`ps-workflow-blocks: ${file.name}: ${found.blocks.length} PowerShell steps`);
	}

	fs.writeFileSync(path.join(out, 'map.json'), JSON.stringify(map, null, 1));
	fs.writeFileSync(path.join(out, 'errors.json'), JSON.stringify(errors, null, 1));
	console.log(`ps-workflow-blocks: ${map.length} PowerShell steps from ${files.length} files into ${kOut}; ${errors.length} SCAN-ERRORs`);
}

module.exports = { readYaml, powerShellOf, placeholders, Refused };

if (require.main === module) {
	main();
}
