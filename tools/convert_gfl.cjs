// Offline conversion only. Vendored converter retains its own GPL-2.0 license.
const fs = require('fs');
const path = require('path');
const SkeletonBinary = require('./vendor/gfSkel2Json/SkeletonBinary');
for (const file of process.argv.slice(2)) {
    const bytes = fs.readFileSync(file);
    const reader = new SkeletonBinary();
    reader.data = new Uint8Array(bytes); // Respect the Buffer's byte offset/length.
    reader.initJson();
    if (reader.nextNum !== bytes.length) throw new Error(`Unread bytes in ${file}: ${reader.nextNum}/${bytes.length}`);
    const out = file.replace(/\.skel$/i, '.json');
    fs.writeFileSync(out, JSON.stringify(reader.json));
    console.log(path.basename(file), Object.keys(reader.json.animations));
}
