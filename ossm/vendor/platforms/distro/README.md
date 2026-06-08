# Updating bazelbuild/platforms

WARNING: These are what worked last time. Reality might be different. 

## Step 1: Make the release

- Pick a new version number
- Update version.bzl
- Run distro/makerel.sh
- Go to the [Releases](https://github.com/bazelbuild/platforms/releases) page
- Draft a new release
  - Name the release with a version number
  - Use the version number as the title
  - Copy the description that makerel.sh produced to the description field.
  - upload the generated tar file

- use https://github.com/bazelbuild/continuous-integration/blob/HEAD/mirror/mirror.sh to mirror the file

## Step 2: Update Bazel

- Edit `distdir_deps.bzl`
- Merge the PR

Sample diff:

```
diff --git a/distdir_deps.bzl b/distdir_deps.bzl
index ed49a563bc..1739a25c2a 100644
--- a/distdir_deps.bzl
+++ b/distdir_deps.bzl
@@ -20,11 +20,11 @@ DIST_DEPS = {
     #
     ########################################
     "platforms": {
-        "archive": "platforms-0.0.2.tar.gz",
-        "sha256": "48a2d8d343863989c232843e01afc8a986eb8738766bfd8611420a7db8f6f0c3",
+        "archive": "platforms-0.0.3.tar.gz",
+        "sha256": "460caee0fa583b908c622913334ec3c1b842572b9c23cf0d3da0c2543a1a157d",
         "urls": [
-            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.2/platforms-0.0.2.tar.gz",
-            "https://github.com/bazelbuild/platforms/releases/download/0.0.2/platforms-0.0.2.tar.gz",
+            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.3/platforms-0.0.3.tar.gz",
+            "https://github.com/bazelbuild/platforms/releases/download/0.0.3/platforms-0.0.3.tar.gz",
         ],
         "used_in": [
             "additional_distfiles",
``` 
