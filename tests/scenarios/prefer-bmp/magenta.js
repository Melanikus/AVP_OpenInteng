// magenta.js: for acceptance-hub-prefer-bmp (twins.cmd). Writes, to its standard output, a 64 x 64 picture of one magenta colour as an
// uncompressed 24-bit BMP, the kind bmp_picture.h takes, so the repository holds no picture: the test makes its own when it runs.
// It takes no file name: twins.cmd sends the output into the file it wants (node magenta.js > <file>).
'use strict';

const width = 64, height = 64, rowBytes = width * 3;	// 192 bytes a row: a multiple of 4, so no row padding
const pixelBytes = rowBytes * height;
const file = Buffer.alloc(54 + pixelBytes);

file.write('BM', 0, 'latin1');
file.writeUInt32LE(file.length, 2);	// the file's size
file.writeUInt32LE(54, 10);			// where the pixels start
file.writeUInt32LE(40, 14);			// BITMAPINFOHEADER
file.writeInt32LE(width, 18);
file.writeInt32LE(height, 22);		// positive: rows bottom-up
file.writeUInt16LE(1, 26);			// planes
file.writeUInt16LE(24, 28);			// bits a pixel
file.writeUInt32LE(0, 30);			// no compression
file.writeUInt32LE(pixelBytes, 34);

for (let at = 54; at < file.length; at += 3) {
	file[at] = 0xff;		// blue
	file[at + 1] = 0x00;	// green
	file[at + 2] = 0xff;	// red
}

process.stdout.write(file);
