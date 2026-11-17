// tree-path.js: where a tool run from the tree's root may read and write. Written 2026-11-17 by Melanikus (Vasquez) for AVP_OpenInteng,
// for make-edge.js and check-edge-link.js (Snyk Code's path traversal findings on the editor's first CI run).
//
// A path named on the command line comes from outside the tool, so it's held to the folder the tool is run from: the tree's root, where
// docs/host-symbols.md runs both. The path is made relative to that folder, and each of its parts is checked: a name of letters, digits,
// spaces and . _ - + ( ), which never starts with a '.' (so it's never '.' or '..') or a space. Then the path is made again from the folder
// and the checked parts alone. So nothing typed can lead a tool anywhere else: not up and out with .., not to another drive, not to a
// device or a stream (no ':' gets through).
'use strict';
const path = require('path');

const kPart = /^[A-Za-z0-9_+()-][A-Za-z0-9 ._+()-]{0,199}$/;

// The path, inside the folder this is run from, or the tool stops. what says which path it is, in the words a refusal uses.
function insideTree(given, what) {
	const refuse = () => {
		console.error(`${what} must be a path inside the folder this is run from (the tree's root), each part a plain name: ${JSON.stringify(String(given))} isn't`);
		process.exit(2);
	};

	if (typeof given !== 'string' || given === '' || given.includes('\0')) {
		refuse();
	}

	const root = process.cwd();
	const relative = path.relative(root, path.resolve(root, given));

	if (relative === '' || path.isAbsolute(relative)) {
		refuse();
	}

	const parts = relative.split(/[\\/]+/);

	if (!parts.every(part => kPart.test(part))) {
		refuse();
	}

	let made = root;

	for (const part of parts) {
		made = path.join(made, path.basename(part));
	}

	return made;
}

module.exports = { insideTree };
