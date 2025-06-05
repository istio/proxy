"use strict";
const fs = require("fs");
const path = require("path");
const assert = require("assert");

const main = async function (typ, dir) {
    const wasm_file = path.join(
        __dirname,
        "..",
        dir,
        "test",
        "hello_world_" + typ + "_wasm_bindgen",
        "hello_world_" + typ + "_wasm_bindgen_bg.wasm",
    );
    const buf = fs.readFileSync(wasm_file);
    assert.ok(buf);

    const res = await WebAssembly.instantiate(buf);
    assert.ok(res);
    assert.strictEqual(res.instance.exports.double(2), 4);
};

["bundler", "web", "deno", "nomodules", "nodejs"].forEach((typ) => {
    main(typ, process.argv.length > 2 ? process.argv[2] : "").catch(function (
        err,
    ) {
        console.error(err);
        process.exit(1);
    });
});
