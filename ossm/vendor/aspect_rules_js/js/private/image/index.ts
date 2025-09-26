import { createReadStream, createWriteStream } from 'node:fs'
import { readdir, readFile, readlink, stat } from 'node:fs/promises'
import * as path from 'node:path'
import { Readable, Stream } from 'node:stream'
import { pathToFileURL } from 'node:url'
import { createGzip } from 'node:zlib'
import { pack, Pack } from 'tar-stream'

const MTIME = new Date(0)
const MODE_FOR_DIR = 0o755
const MODE_FOR_FILE = 0o555
const MODE_FOR_SYMLINK = 0o775

type HermeticStat = {
    mtime: Date
    mode: number
    size?: number
}

type Owner = {
    gid: number
    uid: number
}

type Entry = {
    is_source: boolean
    is_directory: boolean
    is_external: boolean
    dest: string
    root?: string
    remove_non_hermetic_lines?: boolean
}
type Entries = { [path: string]: Entry }

type Compression = 'gzip' | 'none'

function findKeyByValue(entries: Entries, value: string): string | undefined {
    for (const [key, { dest: val }] of Object.entries(entries)) {
        if (val == value) {
            return key
        }
    }
    return undefined
}

function leftStrip(p: string, p1: string, p2: string) {
    if (p.startsWith(p1)) {
        return p.slice(p1.length)
    } else if (p.startsWith(p2)) {
        return p.slice(p2.length)
    }
    return p
}

async function readlinkSafe(p: string) {
    try {
        const link = await readlink(p)
        return path.resolve(path.dirname(p), link)
    } catch (e) {
        if (e.code == 'EINVAL') {
            return p
        }
        throw e
    }
}

// TODO: drop once we no longer support bazel 5
async function resolveSymlinkLegacy(relativeP: string) {
    let prevHop = path.resolve(relativeP)
    let hopped = false
    let execrootOutOfSandbox = ''
    let execroot = process.env.JS_BINARY__EXECROOT!
    while (true) {
        let nextHop = await readlinkSafe(prevHop)

        if (!execrootOutOfSandbox && !nextHop.startsWith(execroot)) {
            execrootOutOfSandbox = nextHop
                .replace(relativeP, '')
                .replace(/\/$/, '')
            prevHop = nextHop
            continue
        }

        let relativeNextHop = leftStrip(nextHop, execroot, execrootOutOfSandbox)
        let relativePrevHop = leftStrip(prevHop, execroot, execrootOutOfSandbox)

        if (relativeNextHop != relativePrevHop) {
            prevHop = nextHop
            hopped = true
        } else if (!hopped) {
            return undefined
        } else {
            return nextHop
        }
    }
}

async function resolveSymlink(p: string) {
    let prevHop = path.resolve(p)
    let hopped = false

    while (true) {
        // /output-base/sandbox/4/execroot/wksp/bazel-out
        // /output-base/execroot/wksp/bazel-out
        let nextHop = await readlinkSafe(prevHop)
        if (!nextHop.startsWith(process.env.JS_BINARY__EXECROOT!)) {
            return hopped ? prevHop : undefined
        }
        if (nextHop != prevHop) {
            prevHop = nextHop
            hopped = true
        } else if (!hopped) {
            return undefined
        } else {
            return nextHop
        }
    }
}

async function* walk(dir: string, accumulate = '') {
    const dirents = await readdir(dir, { withFileTypes: true })
    for (const dirent of dirents) {
        let isDirectory = dirent.isDirectory()

        if (
            dirent.isSymbolicLink() &&
            !dirent.isDirectory() &&
            !dirent.isFile()
        ) {
            // On OSX we sometimes encounter this bug: https://github.com/nodejs/node/issues/30646
            // The entry is apparently a symlink, but it's ambiguous whether it's a symlink to a
            // file or to a directory, and lstat doesn't tell us either. Determine the type by
            // attempting to read it as a directory.

            try {
                await readdir(path.join(dir, dirent.name))
                isDirectory = true
            } catch (error) {
                if (error.code === 'ENOTDIR') {
                    isDirectory = false
                } else {
                    throw error
                }
            }
        }

        if (isDirectory) {
            yield* walk(
                path.join(dir, dirent.name),
                path.join(accumulate, dirent.name)
            )
        } else {
            yield path.join(accumulate, dirent.name)
        }
    }
}

function add_parents(
    name: string,
    pkg: Pack,
    existing_paths: Set<string>,
    owner: Owner
) {
    const segments = path.dirname(name).split('/')
    let prev = ''
    const stats: HermeticStat = {
        // this is an intermediate directory and bazel does not allow specifying
        // modes for intermediate directories.
        mode: MODE_FOR_DIR,
        mtime: MTIME,
    }
    for (const part of segments) {
        if (!part) {
            continue
        }
        prev = path.join(prev, part)
        // check if the directory has been has been created before.
        if (existing_paths.has(prev)) {
            continue
        }

        existing_paths.add(prev)
        add_directory(prev, pkg, owner, stats)
    }
}

function add_directory(
    name: string,
    pkg: Pack,
    owner: Owner,
    stats: HermeticStat
) {
    pkg.entry({
        type: 'directory',
        name: name.replace(/^\//, ''),
        mode: stats.mode,
        mtime: MTIME,
        gid: owner.gid,
        uid: owner.uid,
    }).end()
}

function add_symlink(
    name: string,
    linkname: string,
    pkg: Pack,
    owner: Owner,
    stats: HermeticStat
) {
    const link_parent = path.dirname(name)
    pkg.entry({
        type: 'symlink',
        name: name.replace(/^\//, ''),
        linkname: path.relative(link_parent, linkname),
        mode: stats.mode,
        mtime: MTIME,
        uid: owner.uid,
        gid: owner.gid,
    }).end()
}

function add_file(
    name: string,
    content: Readable,
    pkg: Pack,
    owner: Owner,
    stats: HermeticStat
) {
    return new Promise((resolve, reject) => {
        const entry = pkg.entry(
            {
                type: 'file',
                name: name.replace(/^\//, ''),
                mode: stats.mode,
                size: stats.size,
                mtime: MTIME,
                uid: owner.uid,
                gid: owner.gid,
            },
            (err) => {
                if (err) {
                    reject(err)
                } else {
                    resolve(undefined)
                }
            }
        )
        content.pipe(entry)
    })
}

export async function build(
    entries: Entries,
    outputPath: string,
    compression: Compression,
    owner: Owner,
    useLegacySymlinkDetection: boolean
) {
    const resolveSymlinkFn = useLegacySymlinkDetection
        ? resolveSymlinkLegacy
        : resolveSymlink
    const output = pack()
    const existing_paths = new Set<string>()

    let write: Stream = output
    if (compression == 'gzip') {
        write = write.pipe(createGzip())
    }
    write.pipe(createWriteStream(outputPath))

    for (const key of Object.keys(entries).sort()) {
        const {
            dest,
            is_directory,
            is_source,
            is_external,
            root,
            remove_non_hermetic_lines,
        } = entries[key]

        // its a treeartifact. expand it and add individual entries.
        if (is_directory) {
            for await (const sub_key of walk(dest)) {
                const new_key = path.join(key, sub_key)
                const new_dest = path.join(dest, sub_key)

                add_parents(new_key, output, existing_paths, owner)

                const stats = await stat(new_dest)
                await add_file(
                    new_key,
                    createReadStream(new_dest),
                    output,
                    owner,
                    stats
                )
            }
            continue
        }

        // create parents of current path.
        add_parents(key, output, existing_paths, owner)

        // A source file from workspace, not an output of a target.
        if (is_source) {
            const originalStat = await stat(dest)
            // use stable mode bits instead of preserving the one from file.
            const stats: HermeticStat = {
                mode: MODE_FOR_FILE,
                mtime: MTIME,
                size: originalStat.size,
            }
            await add_file(key, createReadStream(dest), output, owner, stats)
            continue
        }

        // root indicates where the generated source comes from. it looks like
        // `bazel-out/darwin_arm64-fastbuild` when there's no transition.
        if (!root) {
            // everything except sources should have
            throw new Error(
                `unexpected entry format. ${JSON.stringify(
                    entries[key]
                )}. please file a bug at https://github.com/aspect-build/rules_js/issues/new/choose`
            )
        }

        const realp = await resolveSymlinkFn(dest)

        // it's important that we don't treat any symlink pointing out of execroot since
        // bazel symlinks external files into sandbox to make them available to us.
        if (realp && !is_external) {
            const output_path = realp.slice(realp.indexOf(root))
            // interestingly, bazel 5 and 6 sets different mode bits on symlinks.
            // well use `0o755` to allow owner&group to `rwx` and others `rx`
            // see: https://chmodcommand.com/chmod-775/
            // const stats = await stat(dest)
            const stats: HermeticStat = { mode: MODE_FOR_SYMLINK, mtime: MTIME }
            const linkname = findKeyByValue(entries, output_path)
            if (linkname == undefined) {
                throw new Error(
                    `Couldn't map symbolic link ${output_path} to a path. please file a bug at https://github.com/aspect-build/rules_js/issues/new/choose\n\n` +
                        `dest: ${dest}\n` +
                        `realpath: ${realp}\n` +
                        `outputpath: ${output_path}\n` +
                        `root: ${root}\n` +
                        `runfiles: ${key}\n\n`
                )
            }
            add_symlink(key, linkname, output, owner, stats)
        } else {
            // Due to filesystems setting different bits depending on the os we have to opt-in
            // to use a stable mode for files.
            // In the future, we might want to hand off fine-grained control of these to users
            // see: https://chmodcommand.com/chmod-0555/
            const originalStat = await stat(dest)
            const stats: HermeticStat = {
                mode: MODE_FOR_FILE,
                mtime: MTIME,
                size: originalStat.size,
            }
            let stream: Readable = createReadStream(dest)

            if (remove_non_hermetic_lines) {
                const content = await readFile(dest)
                const replaced = Buffer.from(
                    content
                        .toString()
                        .replace(
                            /.*JS_BINARY__TARGET_CPU=".*?"/g,
                            `export JS_BINARY__TARGET_CPU="$(uname -m)"`
                        )
                        .replace(
                            /.*JS_BINARY__BINDIR=".*"/g,
                            `export JS_BINARY__BINDIR="$(pwd)"`
                        )
                )
                stream = Readable.from(replaced)
                stats.size = replaced.byteLength
            }

            await add_file(key, stream, output, owner, stats)
        }
    }

    output.finalize()
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
    const [
        entriesPath,
        outputPath,
        compression,
        owner,
        useLegacySymlinkDetection,
    ] = process.argv.slice(2)
    const raw_entries = await readFile(entriesPath)
    const entries: Entries = JSON.parse(raw_entries.toString())
    const [uid, gid] = owner.split(':').map(Number)
    build(
        entries,
        outputPath,
        compression as Compression,
        { uid, gid } as Owner,
        !!useLegacySymlinkDetection
    )
}
