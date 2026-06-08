# 0.3.1

* Fixed some cases of comparisons that contain '-' or '+' against versions that are missing those parts.

# 0.3.0

* Added `skip_build` parameter to control whether comparisons adhere to Semver-10.
* [BC] Changed `version.parse` and `version.compare` to take parameter `error` as a keyword only parameter.

# 0.2.1

* Improved support for mixed type comparisons.

# 0.2.0

* Added support for parsing out 'pre_release' and 'build' components.
* Added `versions.compare`.
* Added 'pre_release' and 'build' support for comparisons.
* Added CI run for Windows (may be dropped at any future point).

# 0.1.2

* Fixed workspace file to work for transitive repos.

# 0.1.1

* Fixed some naming issues and remove mbo references.

# 0.1.0

Initial version.
