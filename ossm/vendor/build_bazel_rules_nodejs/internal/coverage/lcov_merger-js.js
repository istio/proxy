/* THIS FILE GENERATED FROM .ts; see BUILD.bazel */ /* clang-format off */"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const readline = require("readline");
function _getArg(argv, key) {
    return argv.find(a => a.startsWith(key)).split('=')[1];
}
function main() {
    return __awaiter(this, void 0, void 0, function* () {
        const argv = process.argv;
        const coverageDir = _getArg(argv, '--coverage_dir=');
        const outputFile = _getArg(argv, '--output_file=');
        const sourceFileManifest = _getArg(argv, '--source_file_manifest=');
        const tmpdir = process.env.TEST_TMPDIR;
        if (!sourceFileManifest || !tmpdir || !outputFile) {
            throw new Error();
        }
        const instrumentedSourceFiles = fs.readFileSync(sourceFileManifest).toString('utf8').split('\n');
        const c8OutputDir = path.join(tmpdir, crypto.randomBytes(4).toString('hex'));
        fs.mkdirSync(c8OutputDir);
        const includes = instrumentedSourceFiles
            .filter(f => ['.js', '.jsx', '.cjs', '.ts', '.tsx', '.mjs'].includes(path.extname(f)))
            .map(f => {
            const p = path.parse(f);
            let targetExt;
            switch (p.ext) {
                case '.mjs':
                    targetExt = '.mjs';
                default:
                    targetExt = '.js';
            }
            return path.format(Object.assign(Object.assign({}, p), { base: undefined, ext: targetExt }));
        });
        let c8;
        try {
            c8 = require('c8');
        }
        catch (e) {
            if (e.code == 'MODULE_NOT_FOUND') {
                console.error('ERROR: c8 npm package is required for bazel coverage');
                process.exit(1);
            }
            throw e;
        }
        yield new c8
            .Report({
            include: includes,
            exclude: includes.length === 0 ? ['**'] : [],
            reportsDirectory: c8OutputDir,
            tempDirectory: coverageDir,
            resolve: '',
            all: true,
            reporter: ['lcovonly']
        })
            .run();
        const inputFile = path.join(c8OutputDir, 'lcov.info');
        const input = readline.createInterface({
            input: fs.createReadStream(inputFile),
        });
        const output = fs.createWriteStream(outputFile);
        input.on('line', line => {
            const patched = line.replace('SF:../../../', 'SF:');
            output.write(patched + '\n');
        });
    });
}
if (require.main === module) {
    main();
}
