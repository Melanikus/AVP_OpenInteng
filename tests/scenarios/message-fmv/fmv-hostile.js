// fmv-hostile.js <kind>: writes one hostile "video" to standard output, for make-fmvs.cmd beside it (acceptance-strategy-message-fmv; Dietrich 2026-11-18). It takes no path:
// the .cmd sends the bytes into the run's own data folder. Kinds:
//   not-smacker  104 bytes that start "BIKi" (a Bink file's start) with a Smacker message's numbers after it: not a Smacker video
//   too-short    50 bytes of "SMK2" and noughts: fewer than a Smacker header's 104
'use strict';
const kind = process.argv[2];
let bytes;
if (kind === 'not-smacker') {
	bytes = Buffer.alloc(104 + 40, 0x5a);
	bytes.fill(0, 0, 104);
	bytes.write('BIKi', 0, 'latin1');
	bytes.writeUInt32LE(128, 4);
	bytes.writeUInt32LE(96, 8);
	bytes.writeUInt32LE(228, 12);
	bytes.writeInt32LE(-6666, 16);
	bytes.writeUInt32LE(0xc0005622, 72);
} else if (kind === 'too-short') {
	bytes = Buffer.alloc(50, 0);
	bytes.write('SMK2', 0, 'latin1');
} else {
	process.stderr.write('usage: node fmv-hostile.js not-smacker|too-short > <file>\n');
	process.exit(2);
}
process.stdout.write(bytes);
