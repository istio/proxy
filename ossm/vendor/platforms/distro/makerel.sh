#!/bin/bash

if [[ ! -f WORKSPACE ]] ; then
  echo 'You must run this command from the top of the workspace.'
  exit 1
fi

if [[ ! -f version.bzl ]] ; then
  version.bzl is missing.
  exit 1
fi
version=$(sed -n -e 's/^version *= *"\(.*\)".*$/\1/p'  version.bzl)

# tar on macos builds a file with different checksums each time.
if [[ $(uname) != 'Linux' ]] ; then
  echo 'You must run this command from a linux machine.'
  exit 1
fi


dist_file="/tmp/platforms-${version}.tar.gz"
tar czf "$dist_file" BUILD LICENSE MODULE.bazel WORKSPACE WORKSPACE.bzlmod version.bzl cpu os host
sha256=$(shasum -a256 "$dist_file" | cut -d' ' -f1)

path="github.com/bazelbuild/platforms/releases/download/$version/platforms-$version.tar.gz"
cat <<INP


1. Create a new release using the tag $version
2. Copy/paste the text below into the release description field.
3. Upload $dist_file as an artifact.
4. Copy $dist_file to the mirror site.
5. Create the release.
6. Update Bazel distdir_deps.bzl to point to this new release. See the readme.

=============== CUT HERE =============== 
**WORKSPACE setup**

\`\`\`
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "platforms",
    urls = [
        "https://mirror.bazel.build/${path}",
        "https://${path}",
    ],
    sha256 = "$sha256",
)
\`\`\`
=============== CUT HERE =============== 

Use this to update Bazel's distdir_deps.bzl


        "archive": "platforms-$version.tar.gz",
        "sha256": "$sha256",
        "urls": [
            "https://mirror.bazel.build/${path}",
            "https://${path}",
        ],

Copy/paste this to mirror the file.

  gsutil cp /tmp/platforms-$version.tar.gz "gs://bazel-mirror/${path}"
  gsutil setmeta -h "Cache-Control: public, max-age=31536000" "gs://bazel-mirror/${path}"

INP
