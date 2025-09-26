// This script creates output that is copy/pasted into /nodejs/private/node_versions.bzl to
// add all published Node.js versions >= 8.0.0.
// See the update-nodejs-versions script in package.json

const https = require("https");

const MIN_VERSION = [8, 0, 0];
const MAX_VERSION = [22, 99, 99];

const REPOSITORY_TYPES = {
  "darwin-arm64.tar.gz": "darwin_arm64",
  "darwin-x64.tar.gz": "darwin_amd64",
  "linux-x64.tar.xz": "linux_amd64",
  "linux-arm64.tar.xz": "linux_arm64",
  "linux-s390x.tar.xz": "linux_s390x",
  "win-x64.zip": "windows_amd64",
  "linux-ppc64le.tar.xz": "linux_ppc64le",
};

function getText(url) {
  return new Promise((resolve, reject) => {
    https.get(url, (res) => {
      if (res.statusCode !== 200) {
        return reject(new Error());
      }
      const body = [];
      res.on("data", (chunk) => body.push(String(chunk)));
      res.on("end", () => resolve(body.join("")));
    });
  });
}

async function getJson(url) {
  return JSON.parse(await getText(url));
}

function versionCompare(lhs, rhs) {
  if (lhs[0] !== rhs[0]) {
    return lhs[0] - rhs[0];
  }
  if (lhs[1] !== rhs[1]) {
    return lhs[1] - rhs[1];
  }
  return lhs[2] - rhs[2];
}

async function getNodeJsVersions() {
  const json = await getJson("https://nodejs.org/dist/index.json");

  return (json.map(({version}) => version.slice(1).split('.').map(Number))
              .filter(
                  (version) => versionCompare(version, MIN_VERSION) >= 0 &&
                      versionCompare(version, MAX_VERSION) <= 0)
              .sort(versionCompare));
}

async function getNodeJsVersion(version) {
  const text = await getText(
    `https://nodejs.org/dist/v${version.join(".")}/SHASUMS256.txt`
  );

  return {
    version,
    repositories: text
      .split("\n")
      .filter(Boolean)
      .map((line) => {
        const [sha, filename] = line.split(/\s+/);
        const type = REPOSITORY_TYPES[filename.replace(/^node-v[\d.]+-/, "")];
        return type ? { filename, sha, type } : undefined;
      })
      .filter(Boolean),
  };
}

async function main() {
  const versions = await getNodeJsVersions();
  const nodeRepositories = await Promise.all(versions.map(getNodeJsVersion));
  console.log('""" Generated code; do not edit');
  console.log('Update by running npm run update-nodejs-versions\n"""\n');
  // Suppress buildifier
  console.log('# @unsorted-dict-items');
  console.log('NODE_VERSIONS = {');
  nodeRepositories.forEach(({ version, repositories }) => {
    console.log(
      [
        `# ${version.join(".")}`,
        ...repositories.map(
          ({ filename, sha, type }) =>
            `"${version.join(
              "."
            )}-${type}": ("${filename}", "${filename.replace(
              /(\.tar)?\.[^.]+$/,
              ""
            )}", "${sha}"),`
        ),
      ]
        .map((line) => `    ${line}`)
        .join("\n")
    );
  });
  console.log("}");
}

if (require.main === module) {
  main();
}
