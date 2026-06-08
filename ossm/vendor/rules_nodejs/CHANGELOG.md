## [5.8.2](https://github.com/bazelbuild/rules_nodejs/compare/5.8.1...5.8.2) (2023-02-24)


### Bug Fixes

* allow root repositories to override node toolchain version under ([ce13837](https://github.com/bazelbuild/rules_nodejs/commit/ce13837))



## [5.8.1](https://github.com/bazelbuild/rules_nodejs/compare/5.7.3...5.8.1) (2023-02-16)


### Bug Fixes

* **builtin:** convert pkg_web to use cjs instead of js ([#3500](https://github.com/bazelbuild/rules_nodejs/issues/3500)) ([d36a73a](https://github.com/bazelbuild/rules_nodejs/commit/d36a73a))
* **concatjs:** resolve error with TypeScript 5.0 ([e073e18](https://github.com/bazelbuild/rules_nodejs/commit/e073e18))


### Features

* provide [@nodejs](https://github.com/nodejs) repository ([a5755eb](https://github.com/bazelbuild/rules_nodejs/commit/a5755eb)), closes [#3375](https://github.com/bazelbuild/rules_nodejs/issues/3375)



## [5.7.3](https://github.com/bazelbuild/rules_nodejs/compare/5.7.2...5.7.3) (2022-12-09)


### Bug Fixes

* **builtin:** entry point from sources used when used as tool ([#3605](https://github.com/bazelbuild/rules_nodejs/issues/3605)) ([417711d](https://github.com/bazelbuild/rules_nodejs/commit/417711d))



## [5.7.2](https://github.com/bazelbuild/rules_nodejs/compare/5.7.1...5.7.2) (2022-11-27)


### Bug Fixes

* check RUNFILES env variable in @bazel/runfiles helper ([#3602](https://github.com/bazelbuild/rules_nodejs/issues/3602)) ([11395ea](https://github.com/bazelbuild/rules_nodejs/commit/11395ea))
* yarn binary is now run from separate [@yarn](https://github.com/yarn) repo ([dafc2db](https://github.com/bazelbuild/rules_nodejs/commit/dafc2db)), closes [#3530](https://github.com/bazelbuild/rules_nodejs/issues/3530)



## [5.7.1](https://github.com/bazelbuild/rules_nodejs/compare/5.7.0...5.7.1) (2022-10-26)



# [5.7.0](https://github.com/bazelbuild/rules_nodejs/compare/5.6.0...5.7.0) (2022-10-06)


### Bug Fixes

* **builtin:** fix a bug where the launcher produces incorrect runfiles path on windows ([#3562](https://github.com/bazelbuild/rules_nodejs/issues/3562)) ([b02128b](https://github.com/bazelbuild/rules_nodejs/commit/b02128b))
* **builtin:** use updated rules_js launcher logic to source RUNFILES ([#3557](https://github.com/bazelbuild/rules_nodejs/issues/3557)) ([c725169](https://github.com/bazelbuild/rules_nodejs/commit/c725169))


### Features

* add npm binary & files to toolchain provider ([#3570](https://github.com/bazelbuild/rules_nodejs/issues/3570)) ([7ca0688](https://github.com/bazelbuild/rules_nodejs/commit/7ca0688))



# [5.6.0](https://github.com/bazelbuild/rules_nodejs/compare/5.5.4...5.6.0) (2022-09-27)


### Bug Fixes

* **builtin:** properly quote env vars passed to nodejs_binary ([#3553](https://github.com/bazelbuild/rules_nodejs/issues/3553)) ([ffa3ffb](https://github.com/bazelbuild/rules_nodejs/commit/ffa3ffb))
* **typescript:** include all .json in js_library DeclarationInfo ([#3556](https://github.com/bazelbuild/rules_nodejs/issues/3556)) ([f297e81](https://github.com/bazelbuild/rules_nodejs/commit/f297e81)), closes [#3551](https://github.com/bazelbuild/rules_nodejs/issues/3551)
* canonicalize @platforms//cpu:aarch64 ([#3555](https://github.com/bazelbuild/rules_nodejs/issues/3555)) ([b341421](https://github.com/bazelbuild/rules_nodejs/commit/b341421)), closes [/github.com/bazelbuild/platforms/blob/212a486d66569b29c95b00364e2584e80fd08614/cpu/BUILD#L16-L20](https://github.com//github.com/bazelbuild/platforms/blob/212a486d66569b29c95b00364e2584e80fd08614/cpu/BUILD/issues/L16-L20)


### Features

* **create:** introduce `--workspaceDir` flag ([3a28a02](https://github.com/bazelbuild/rules_nodejs/commit/3a28a02))
* add support for darwin arm for concatjs ([#3554](https://github.com/bazelbuild/rules_nodejs/issues/3554)) ([acf88a1](https://github.com/bazelbuild/rules_nodejs/commit/acf88a1))



## [5.5.4](https://github.com/bazelbuild/rules_nodejs/compare/5.5.3...5.5.4) (2022-09-10)


### Bug Fixes

* `ts_project` fail if `root_dir` used deep in source tree ([#3535](https://github.com/bazelbuild/rules_nodejs/issues/3535)) ([dccbb63](https://github.com/bazelbuild/rules_nodejs/commit/dccbb63))
* make catch-all node_modules target name configurable in yarn_install and npm_install ([#3538](https://github.com/bazelbuild/rules_nodejs/issues/3538)) ([6c462c4](https://github.com/bazelbuild/rules_nodejs/commit/6c462c4))



## [5.5.3](https://github.com/bazelbuild/rules_nodejs/compare/5.5.2...5.5.3) (2022-08-01)


### Bug Fixes

* delete ngrx from README. Currently not used ([#3513](https://github.com/bazelbuild/rules_nodejs/issues/3513)) ([828d77c](https://github.com/bazelbuild/rules_nodejs/commit/828d77c))
* **concatjs:** sync with internal change to ensure it works with `tsickle` host ([#3510](https://github.com/bazelbuild/rules_nodejs/issues/3510)) ([78a0528](https://github.com/bazelbuild/rules_nodejs/commit/78a0528))
* limit concurrency when generating BUILD files in npm_install and yarn_install ([#3509](https://github.com/bazelbuild/rules_nodejs/issues/3509)) ([4001716](https://github.com/bazelbuild/rules_nodejs/commit/4001716))



## [5.5.2](https://github.com/bazelbuild/rules_nodejs/compare/5.5.1...5.5.2) (2022-07-10)


### Bug Fixes

* **builtin:** remove unnecessary loader script ([#3495](https://github.com/bazelbuild/rules_nodejs/issues/3495)) ([1641136](https://github.com/bazelbuild/rules_nodejs/commit/1641136))
* **create:** make `--packageManager` flag work ([#3498](https://github.com/bazelbuild/rules_nodejs/issues/3498)) ([cd3db48](https://github.com/bazelbuild/rules_nodejs/commit/cd3db48))



## [5.5.1](https://github.com/bazelbuild/rules_nodejs/compare/5.5.0...5.5.1) (2022-06-24)


### Bug Fixes

* **builtin:** fix an bug where a nodejs_binary would fail to resolve an npm package when the linker is disabled ([#3492](https://github.com/bazelbuild/rules_nodejs/issues/3492)) ([8a2dfc8](https://github.com/bazelbuild/rules_nodejs/commit/8a2dfc8))
* **typescript:** remove protobufjs dependency ([#3491](https://github.com/bazelbuild/rules_nodejs/issues/3491)) ([d46502d](https://github.com/bazelbuild/rules_nodejs/commit/d46502d))
* deterministic output from ts_options_validator ([#3462](https://github.com/bazelbuild/rules_nodejs/issues/3462)) ([d69c646](https://github.com/bazelbuild/rules_nodejs/commit/d69c646)), closes [#3461](https://github.com/bazelbuild/rules_nodejs/issues/3461)



# [5.5.0](https://github.com/bazelbuild/rules_nodejs/compare/5.4.1...5.5.0) (2022-05-18)


### Bug Fixes

* **docs:** stray text in npm_install docs ([#3450](https://github.com/bazelbuild/rules_nodejs/issues/3450)) ([6d519eb](https://github.com/bazelbuild/rules_nodejs/commit/6d519eb))
* **examples:** fix architect example on m1 ([#3447](https://github.com/bazelbuild/rules_nodejs/issues/3447)) ([d234328](https://github.com/bazelbuild/rules_nodejs/commit/d234328))
* **typescript:** correctly process diagnostics in worker mode ([#3441](https://github.com/bazelbuild/rules_nodejs/issues/3441)) ([e4842c1](https://github.com/bazelbuild/rules_nodejs/commit/e4842c1))
* set correct linking location for yarn_install using package.json from external repository ([#3442](https://github.com/bazelbuild/rules_nodejs/issues/3442)) ([55a84d1](https://github.com/bazelbuild/rules_nodejs/commit/55a84d1))
* **concatjs:** adding devmode to BazelOpts ([#3433](https://github.com/bazelbuild/rules_nodejs/issues/3433)) ([5afaab8](https://github.com/bazelbuild/rules_nodejs/commit/5afaab8))


### Features

* **builtin:** expand make vars in nodejs_binary/test env attr ([#3456](https://github.com/bazelbuild/rules_nodejs/issues/3456)) ([353593c](https://github.com/bazelbuild/rules_nodejs/commit/353593c))
* **rollup:** support esm configurations to be provided ([#3435](https://github.com/bazelbuild/rules_nodejs/issues/3435)) ([7bac805](https://github.com/bazelbuild/rules_nodejs/commit/7bac805))
* expose [@nodejs](https://github.com/nodejs)_host//:bin/node without using alias ([#3434](https://github.com/bazelbuild/rules_nodejs/issues/3434)) ([506eebc](https://github.com/bazelbuild/rules_nodejs/commit/506eebc))



## [5.4.2](https://github.com/bazelbuild/rules_nodejs/compare/5.4.1...5.4.2) (2022-04-29)


### Bug Fixes

* **concatjs:** adding devmode to BazelOpts ([#3433](https://github.com/bazelbuild/rules_nodejs/issues/3433)) ([5afaab8](https://github.com/bazelbuild/rules_nodejs/commit/5afaab8))


### Features

* expose [@nodejs](https://github.com/nodejs)_host//:bin/node without using alias ([7d338cb](https://github.com/bazelbuild/rules_nodejs/commit/7d338cb))



## [5.4.1](https://github.com/bazelbuild/rules_nodejs/compare/5.4.0...5.4.1) (2022-04-25)


### Bug Fixes

* **concatjs:** resolve error with TypeScript 4.7 ([#3420](https://github.com/bazelbuild/rules_nodejs/issues/3420)) ([1074231](https://github.com/bazelbuild/rules_nodejs/commit/1074231))
* enable stardoc generation for rules that depend on ts in core ([#3394](https://github.com/bazelbuild/rules_nodejs/issues/3394)) ([5d1c2ad](https://github.com/bazelbuild/rules_nodejs/commit/5d1c2ad))
* **builtin:** fix a bug where mjs entry points were not added to ([#3406](https://github.com/bazelbuild/rules_nodejs/issues/3406)) ([e24473c](https://github.com/bazelbuild/rules_nodejs/commit/e24473c))
* **builtin:** improve execution requirements for copy file operations ([#3413](https://github.com/bazelbuild/rules_nodejs/issues/3413)) ([43e478d](https://github.com/bazelbuild/rules_nodejs/commit/43e478d))
* **jasmine:** allow cjs specs + add cjs/mjs tests ([#3401](https://github.com/bazelbuild/rules_nodejs/issues/3401)) ([dd7e778](https://github.com/bazelbuild/rules_nodejs/commit/dd7e778))



# [5.4.0](https://github.com/bazelbuild/rules_nodejs/compare/5.3.1...5.4.0) (2022-04-06)


### Bug Fixes

* exports_directories_only causes node to resolve from runfiles/node_modules ([#3380](https://github.com/bazelbuild/rules_nodejs/issues/3380)) ([5bf3782](https://github.com/bazelbuild/rules_nodejs/commit/5bf3782))
* use -R in copy_file(is_dir=True) so macos & linux behavior are the same ([#3383](https://github.com/bazelbuild/rules_nodejs/issues/3383)) ([2fd97fb](https://github.com/bazelbuild/rules_nodejs/commit/2fd97fb))
* use python3 instead of python in unittest.bash ([#3382](https://github.com/bazelbuild/rules_nodejs/issues/3382)) ([b74d12d](https://github.com/bazelbuild/rules_nodejs/commit/b74d12d))


### Features

* **builtin:** add basic ESM support ([bc62f37](https://github.com/bazelbuild/rules_nodejs/commit/bc62f37))
* **jasmine:** add basic ESM support ([b4b2c74](https://github.com/bazelbuild/rules_nodejs/commit/b4b2c74))



## [5.3.1](https://github.com/bazelbuild/rules_nodejs/compare/5.3.0...5.3.1) (2022-03-29)


### Bug Fixes

* condition on target instead of exec ([#3373](https://github.com/bazelbuild/rules_nodejs/issues/3373)) ([43159a5](https://github.com/bazelbuild/rules_nodejs/commit/43159a5))
* **builtin:** require correct runfiles path to chdir script ([#3374](https://github.com/bazelbuild/rules_nodejs/issues/3374)) ([9ed16c0](https://github.com/bazelbuild/rules_nodejs/commit/9ed16c0))



# [5.3.0](https://github.com/bazelbuild/rules_nodejs/compare/5.2.0...5.3.0) (2022-03-20)


### Bug Fixes

* **builtin:** `yarn_install` with vendored yarn `.cjs` file breaks ([#3350](https://github.com/bazelbuild/rules_nodejs/issues/3350)) ([4a025c0](https://github.com/bazelbuild/rules_nodejs/commit/4a025c0))
* **builtin:** default STDOUT_CAPTURE_IS_NOT_AN_OUTPUT to falsey ([#3364](https://github.com/bazelbuild/rules_nodejs/issues/3364)) ([11952b1](https://github.com/bazelbuild/rules_nodejs/commit/11952b1))
* **concatjs:** tsc-wrapped compilation workers are subject to linker race-conditions ([#3370](https://github.com/bazelbuild/rules_nodejs/issues/3370)) ([d907eb5](https://github.com/bazelbuild/rules_nodejs/commit/d907eb5))
* **jasmine:** sharded test never fail when using Jasmine < 4 ([#3360](https://github.com/bazelbuild/rules_nodejs/issues/3360)) ([add1452](https://github.com/bazelbuild/rules_nodejs/commit/add1452))
* update source for generated docs ([#3354](https://github.com/bazelbuild/rules_nodejs/issues/3354)) ([097732b](https://github.com/bazelbuild/rules_nodejs/commit/097732b))
* **runfiles:** use normalized paths when guarding runfiles root and node_modules on Windows ([#3331](https://github.com/bazelbuild/rules_nodejs/issues/3331)) ([7993296](https://github.com/bazelbuild/rules_nodejs/commit/7993296))


### Features

* **builtin:** add silent_on_success option to npm_package_bin ([#3336](https://github.com/bazelbuild/rules_nodejs/issues/3336)) ([78aefa3](https://github.com/bazelbuild/rules_nodejs/commit/78aefa3))



# [5.2.0](https://github.com/bazelbuild/rules_nodejs/compare/5.1.0...5.2.0) (2022-03-01)


### Bug Fixes

* **builtin:** avoid unnecessary chdir to prevent worker threads from failing ([550673f](https://github.com/bazelbuild/rules_nodejs/commit/550673f))
* **builtin:** take custom node_repositories value into account when checking if Node version exists for the platform ([#3339](https://github.com/bazelbuild/rules_nodejs/issues/3339)) ([5a1cbfa](https://github.com/bazelbuild/rules_nodejs/commit/5a1cbfa))
* **builtin:** use srcs on genrule to not build tool for host ([#3344](https://github.com/bazelbuild/rules_nodejs/issues/3344)) ([17e3e2b](https://github.com/bazelbuild/rules_nodejs/commit/17e3e2b))
* **typescript:** account for rootDir when predicting json output paths ([#3348](https://github.com/bazelbuild/rules_nodejs/issues/3348)) ([bd36cd0](https://github.com/bazelbuild/rules_nodejs/commit/bd36cd0)), closes [#3330](https://github.com/bazelbuild/rules_nodejs/issues/3330)


### Features

* **builtin:** perform make variable substitution in npm_package_bin env vars ([#3343](https://github.com/bazelbuild/rules_nodejs/issues/3343)) ([dfe4392](https://github.com/bazelbuild/rules_nodejs/commit/dfe4392))
* **examples:** example jest add junit reporter ([#3338](https://github.com/bazelbuild/rules_nodejs/issues/3338)) ([840395f](https://github.com/bazelbuild/rules_nodejs/commit/840395f))
* **typescript:** warn the user when rootDirs looks wrong in ts_proje… ([#3126](https://github.com/bazelbuild/rules_nodejs/issues/3126)) ([8df86cc](https://github.com/bazelbuild/rules_nodejs/commit/8df86cc))



# [5.1.0](https://github.com/bazelbuild/rules_nodejs/compare/5.0.2...5.1.0) (2022-02-02)


### Bug Fixes

* **builtin:** make linker aspect run in constant time per target ([522fd7c](https://github.com/bazelbuild/rules_nodejs/commit/522fd7c))
* **builtin:** reduce linker debug string allocations ([#3309](https://github.com/bazelbuild/rules_nodejs/issues/3309)) ([fb2eeac](https://github.com/bazelbuild/rules_nodejs/commit/fb2eeac))
* **typescript:** include workspace name in relativize helper ([f78a2b8](https://github.com/bazelbuild/rules_nodejs/commit/f78a2b8))


### Features

* **typescript:** added support for using non-file targets in srcs of ts_project ([96d37b6](https://github.com/bazelbuild/rules_nodejs/commit/96d37b6))
* **typescript:** moved file functions to tslb.bzl ([20c5c58](https://github.com/bazelbuild/rules_nodejs/commit/20c5c58))



## [5.0.2](https://github.com/bazelbuild/rules_nodejs/compare/5.0.1...5.0.2) (2022-01-27)


### Bug Fixes

* **jasmine:** can not reference runner when exports_directories_only=… ([#3293](https://github.com/bazelbuild/rules_nodejs/issues/3293)) ([0be0eeb](https://github.com/bazelbuild/rules_nodejs/commit/0be0eeb))
* use robocopy in copy_file#is_directory so we don't hit 254 file path limit of xcopy ([#3295](https://github.com/bazelbuild/rules_nodejs/issues/3295)) ([ed0249b](https://github.com/bazelbuild/rules_nodejs/commit/ed0249b))
* **builtin:** pass kwargs from node_repositories helper ([#3287](https://github.com/bazelbuild/rules_nodejs/issues/3287)) ([b446fa1](https://github.com/bazelbuild/rules_nodejs/commit/b446fa1))
* **jasmine:** replace deprecated Jasmine APIs that have been removed in version 4 ([#3283](https://github.com/bazelbuild/rules_nodejs/issues/3283)) ([bde750b](https://github.com/bazelbuild/rules_nodejs/commit/bde750b)), closes [#3289](https://github.com/bazelbuild/rules_nodejs/issues/3289)



## [5.0.1](https://github.com/bazelbuild/rules_nodejs/compare/5.0.0...5.0.1) (2022-01-24)


### Bug Fixes

* **builtin:** prevent usage with InputArtifact directories ([553ef27](https://github.com/bazelbuild/rules_nodejs/commit/553ef27))
* **create:** add missing workspace dependency call ([d15c3dd](https://github.com/bazelbuild/rules_nodejs/commit/d15c3dd))



# [5.0.0](https://github.com/bazelbuild/rules_nodejs/compare/5.0.0-rc.2...5.0.0) (2022-01-20)


### Bug Fixes

* **builtin:** npm_package_bin include runfiles in DefaultInfo ([#3261](https://github.com/bazelbuild/rules_nodejs/issues/3261)) ([e915877](https://github.com/bazelbuild/rules_nodejs/commit/e915877))
* **cypress:** use depsets for runfiles and data ([#3240](https://github.com/bazelbuild/rules_nodejs/issues/3240)) ([4889a1a](https://github.com/bazelbuild/rules_nodejs/commit/4889a1a))
* **typescript:** propagate tags to validate_options ([#3260](https://github.com/bazelbuild/rules_nodejs/issues/3260)) ([4615198](https://github.com/bazelbuild/rules_nodejs/commit/4615198))



# [5.0.0-rc.2](https://github.com/bazelbuild/rules_nodejs/compare/5.0.0-rc.1...5.0.0-rc.2) (2022-01-17)


### Bug Fixes

* **builtin:** when running vendored yarn, prefix command with path to node ([#3255](https://github.com/bazelbuild/rules_nodejs/issues/3255)) ([ccbf739](https://github.com/bazelbuild/rules_nodejs/commit/ccbf739))
* angular example needs bump for 5.0 ([#3245](https://github.com/bazelbuild/rules_nodejs/issues/3245)) ([4fd864c](https://github.com/bazelbuild/rules_nodejs/commit/4fd864c))
* guard node_modules roots for dynamic multi-linked npm deps ([#3248](https://github.com/bazelbuild/rules_nodejs/issues/3248)) ([5ad9753](https://github.com/bazelbuild/rules_nodejs/commit/5ad9753))
* warning logic for yarn berry attrs ([eaf70f2](https://github.com/bazelbuild/rules_nodejs/commit/eaf70f2))



# [5.0.0-rc.1](https://github.com/bazelbuild/rules_nodejs/compare/5.0.0-rc.0...5.0.0-rc.1) (2022-01-14)



# [5.0.0-rc.0](https://github.com/bazelbuild/rules_nodejs/compare/4.4.0...5.0.0-rc.0) (2022-01-14)


### Bug Fixes

* allow cypress to run on m1 macs ([#3088](https://github.com/bazelbuild/rules_nodejs/issues/3088)) ([ac96783](https://github.com/bazelbuild/rules_nodejs/commit/ac96783))
* create a bazel-out node_modules tree using copy_file in the external repo when exports_directories_only is True ([#3241](https://github.com/bazelbuild/rules_nodejs/issues/3241)) ([f5eed08](https://github.com/bazelbuild/rules_nodejs/commit/f5eed08))
* filter out .d.ts before passing srcs to transpiler ([#3238](https://github.com/bazelbuild/rules_nodejs/issues/3238)) ([11460e8](https://github.com/bazelbuild/rules_nodejs/commit/11460e8))
* **typescript:** better error when transpiler is used and no declarations are emitted ([72c3662](https://github.com/bazelbuild/rules_nodejs/commit/72c3662)), closes [#3209](https://github.com/bazelbuild/rules_nodejs/issues/3209)
* add arm64 as a platform to //packages/concatjs:docs_scrub_platform ([#3089](https://github.com/bazelbuild/rules_nodejs/issues/3089)) ([8161dcc](https://github.com/bazelbuild/rules_nodejs/commit/8161dcc))
* bump jasmine-reporters to 2.5.0 ([#3180](https://github.com/bazelbuild/rules_nodejs/issues/3180)) ([efbc33b](https://github.com/bazelbuild/rules_nodejs/commit/efbc33b))
* change all cfg=host and cfg=target executable attributes to cfg=exec ([9fd3fb9](https://github.com/bazelbuild/rules_nodejs/commit/9fd3fb9))
* don't symlink execroot node_modules when under bazel run ([d19e20b](https://github.com/bazelbuild/rules_nodejs/commit/d19e20b))
* merge conflict on 5.x branch ([c91f5b6](https://github.com/bazelbuild/rules_nodejs/commit/c91f5b6))
* normalize module path passed to runfiles helper for robustness ([#3094](https://github.com/bazelbuild/rules_nodejs/issues/3094)) ([20e121b](https://github.com/bazelbuild/rules_nodejs/commit/20e121b))
* remove _repository_args from nodejs_binary ([90c7fe0](https://github.com/bazelbuild/rules_nodejs/commit/90c7fe0))
* remove node and use toolchain ([#3144](https://github.com/bazelbuild/rules_nodejs/issues/3144)) ([cb83746](https://github.com/bazelbuild/rules_nodejs/commit/cb83746))
* remove trailing forward slash when resolving workspace root link in runfiles MANIFEST ([#3093](https://github.com/bazelbuild/rules_nodejs/issues/3093)) ([bcff217](https://github.com/bazelbuild/rules_nodejs/commit/bcff217))
* turn off preserve_symlinks in e2e/node_loader_no_preserve_symlinks test ([5410ab2](https://github.com/bazelbuild/rules_nodejs/commit/5410ab2))
* update linker to be tolerant to linking to different output trees ([0d93719](https://github.com/bazelbuild/rules_nodejs/commit/0d93719))
* **builtin:** detect yarn 2+ berry and adjust CLI args ([#3195](https://github.com/bazelbuild/rules_nodejs/issues/3195)) ([9b2c08b](https://github.com/bazelbuild/rules_nodejs/commit/9b2c08b)), closes [#3071](https://github.com/bazelbuild/rules_nodejs/issues/3071) [#1599](https://github.com/bazelbuild/rules_nodejs/issues/1599)
* **builtin:** don't use local:1 spawn ([#3084](https://github.com/bazelbuild/rules_nodejs/issues/3084)) ([f77e9fd](https://github.com/bazelbuild/rules_nodejs/commit/f77e9fd))
* **builtin:** fixed missing dist targets ([#3068](https://github.com/bazelbuild/rules_nodejs/issues/3068)) ([6c13dac](https://github.com/bazelbuild/rules_nodejs/commit/6c13dac))
* **builtin:** handle external repository file paths in js_library strip_prefix check ([08c75a2](https://github.com/bazelbuild/rules_nodejs/commit/08c75a2))
* **builtin:** js_library: propagate all default_runfiles ([#3183](https://github.com/bazelbuild/rules_nodejs/issues/3183)) ([d07b104](https://github.com/bazelbuild/rules_nodejs/commit/d07b104)), closes [#3182](https://github.com/bazelbuild/rules_nodejs/issues/3182)
* **builtin:** pkg_npm shouldn't assume the name of the nodejs toolchain ([#3129](https://github.com/bazelbuild/rules_nodejs/issues/3129)) ([552178e](https://github.com/bazelbuild/rules_nodejs/commit/552178e))
* **builtin:** provide a DeclarationInfo from js_library is any input files are directories (TreeArtifacts) ([a1d49ae](https://github.com/bazelbuild/rules_nodejs/commit/a1d49ae))
* **builtin:** retrieve yarn shasums from Github releases, rather than npm registry ([88ce34d](https://github.com/bazelbuild/rules_nodejs/commit/88ce34d))
* **builtin:** support mjs/cjs files as javascript files in `js_library` ([9e9bf01](https://github.com/bazelbuild/rules_nodejs/commit/9e9bf01))
* **create:** relocate help argument evaluation ([441e3b8](https://github.com/bazelbuild/rules_nodejs/commit/441e3b8))
* **esbuild:** allow passing additional args to the npm install for esbuild ([#3063](https://github.com/bazelbuild/rules_nodejs/issues/3063)) ([fb2165c](https://github.com/bazelbuild/rules_nodejs/commit/fb2165c))
* **esbuild:** allow whitespace within args ([#2998](https://github.com/bazelbuild/rules_nodejs/issues/2998)) ([181b55d](https://github.com/bazelbuild/rules_nodejs/commit/181b55d)), closes [#2997](https://github.com/bazelbuild/rules_nodejs/issues/2997)
* **esbuild:** do not ignore annotations when the `minify` shorthand attribute is disabled ([#3106](https://github.com/bazelbuild/rules_nodejs/issues/3106)) ([b1275c5](https://github.com/bazelbuild/rules_nodejs/commit/b1275c5))
* **esbuild:** format attribute not working with multiple entry points ([#3103](https://github.com/bazelbuild/rules_nodejs/issues/3103)) ([1000e2b](https://github.com/bazelbuild/rules_nodejs/commit/1000e2b))
* **examples:** run jest updateSnapshot in the workspace ([#3041](https://github.com/bazelbuild/rules_nodejs/issues/3041)) ([e005d82](https://github.com/bazelbuild/rules_nodejs/commit/e005d82))
* **typescript:** add build_test to ensure typecheck is run under --build_tests_only ([#3196](https://github.com/bazelbuild/rules_nodejs/issues/3196)) ([9622443](https://github.com/bazelbuild/rules_nodejs/commit/9622443))
* **typescript:** don't set resolveJsonModule in generated tsconfig if tsconfig is dict and resolve_json_module is unset ([19cd74e](https://github.com/bazelbuild/rules_nodejs/commit/19cd74e))
* **typescript:** ts_project transpiler produces js_library ([#3187](https://github.com/bazelbuild/rules_nodejs/issues/3187)) ([c9a66e0](https://github.com/bazelbuild/rules_nodejs/commit/c9a66e0))
* [#3054](https://github.com/bazelbuild/rules_nodejs/issues/3054) regression in linker behavior in 4.4.2 ([#3059](https://github.com/bazelbuild/rules_nodejs/issues/3059)) ([92965b2](https://github.com/bazelbuild/rules_nodejs/commit/92965b2))
* correct bzl_library target graph ([#3049](https://github.com/bazelbuild/rules_nodejs/issues/3049)) ([07df333](https://github.com/bazelbuild/rules_nodejs/commit/07df333))
* don't link runfiles node_modules to execroot node_modules if there is an external workspace node_modules ([#3060](https://github.com/bazelbuild/rules_nodejs/issues/3060)) ([1d5defa](https://github.com/bazelbuild/rules_nodejs/commit/1d5defa))
* terser semver compatibility range ([da7399e](https://github.com/bazelbuild/rules_nodejs/commit/da7399e))
* unset INIT_CWD and npm_config_registry before calling yarn in yarn_install ([b62e1e8](https://github.com/bazelbuild/rules_nodejs/commit/b62e1e8))
* update tsconfigs to include darwin_arm64-fastbuild in rootDirs ([#3087](https://github.com/bazelbuild/rules_nodejs/issues/3087)) ([1f75f40](https://github.com/bazelbuild/rules_nodejs/commit/1f75f40))
* **typescript:** don't declare outputs that collide with inputs ([#3046](https://github.com/bazelbuild/rules_nodejs/issues/3046)) ([9b47df1](https://github.com/bazelbuild/rules_nodejs/commit/9b47df1))
* **typescript:** give better error when noEmit is set ([#3047](https://github.com/bazelbuild/rules_nodejs/issues/3047)) ([74dd86e](https://github.com/bazelbuild/rules_nodejs/commit/74dd86e))
* simplify portion of linker and fix case where runfiles node_modules symlinks are missing under bazel run ([8c1f26c](https://github.com/bazelbuild/rules_nodejs/commit/8c1f26c))


### chore

* update default nodejs version to 16 ([#3107](https://github.com/bazelbuild/rules_nodejs/issues/3107)) ([b0930fb](https://github.com/bazelbuild/rules_nodejs/commit/b0930fb)), closes [#3101](https://github.com/bazelbuild/rules_nodejs/issues/3101)


### Code Refactoring

* depend on bazel-skylib at runtime ([#3056](https://github.com/bazelbuild/rules_nodejs/issues/3056)) ([e5d4803](https://github.com/bazelbuild/rules_nodejs/commit/e5d4803))
* move yarn fetch to a separate external repo ([975ae9b](https://github.com/bazelbuild/rules_nodejs/commit/975ae9b))
* **builtin:** npm_umd_bundle no longer allows directory sources ([c87ec6b](https://github.com/bazelbuild/rules_nodejs/commit/c87ec6b))


### Features

* add bzlmod extension ([#3146](https://github.com/bazelbuild/rules_nodejs/issues/3146)) ([878ece2](https://github.com/bazelbuild/rules_nodejs/commit/878ece2))
* add src patch being copied to progress message of vendored copy_file ([7aafe15](https://github.com/bazelbuild/rules_nodejs/commit/7aafe15))
* add src patch being copied to progress message of vendored copy_file ([#3206](https://github.com/bazelbuild/rules_nodejs/issues/3206)) ([ddc985c](https://github.com/bazelbuild/rules_nodejs/commit/ddc985c))
* default package_path to the directory of the package.json file in yarn_install and npm_install ([#3233](https://github.com/bazelbuild/rules_nodejs/issues/3233)) ([dcbad88](https://github.com/bazelbuild/rules_nodejs/commit/dcbad88))
* macros nodejs_binary_toolchains nodejs_test_toolchains input multiple toolchains nodejs_binary or nodejs_test ([#3132](https://github.com/bazelbuild/rules_nodejs/issues/3132)) ([55a7521](https://github.com/bazelbuild/rules_nodejs/commit/55a7521))
* set exports_directories_only default to True in yarn_install & npm_install repository rules ([ee0e507](https://github.com/bazelbuild/rules_nodejs/commit/ee0e507))
* use tree artifacts via copy_directory with exports_directories_only ([91fa0ae](https://github.com/bazelbuild/rules_nodejs/commit/91fa0ae))
* **builtin:** npm_install/yarn_install node repo ([#3164](https://github.com/bazelbuild/rules_nodejs/issues/3164)) ([8e277e4](https://github.com/bazelbuild/rules_nodejs/commit/8e277e4))
* **esbuild:** add option to disable metafile generation ([#3066](https://github.com/bazelbuild/rules_nodejs/issues/3066)) ([837cb23](https://github.com/bazelbuild/rules_nodejs/commit/837cb23))
* **esbuild:** make Starlark build settings usable as defines ([#3122](https://github.com/bazelbuild/rules_nodejs/issues/3122)) ([f22502d](https://github.com/bazelbuild/rules_nodejs/commit/f22502d))
* **typescript:** allow alternative transpilers ([#3134](https://github.com/bazelbuild/rules_nodejs/issues/3134)) ([507ec3d](https://github.com/bazelbuild/rules_nodejs/commit/507ec3d)), closes [#3133](https://github.com/bazelbuild/rules_nodejs/issues/3133)
* support --stamp env vars in npm_package_bin ([#3162](https://github.com/bazelbuild/rules_nodejs/issues/3162)) ([38fee32](https://github.com/bazelbuild/rules_nodejs/commit/38fee32))
* **node:** use multiple versions of node, can run old and new toolchains and default behaviour is not broken ([#3125](https://github.com/bazelbuild/rules_nodejs/issues/3125)) ([12a521d](https://github.com/bazelbuild/rules_nodejs/commit/12a521d))


### BREAKING CHANGES

* vendored_yarn attribute is removed
* The default nodejs version is now 16.12.0.
To go back to the previous default, put this in your WORKSPACE:

```starlark
load("@build_bazel_rules_nodejs//:index.bzl", "node_repositories")

node_repositories(
    node_version = "14.17.5",
)
```
* build_bazel_rules_nodejs now depends on bazel_skylib.
You can install it in your WORKSPACE directly, or use our helper macro like so:
* **builtin:** npm_umd_bundle users cannot use exports_directories_only



# [4.4.0](https://github.com/bazelbuild/rules_nodejs/compare/4.3.0...4.4.0) (2021-10-11)


### Bug Fixes

* ts_proto_library: use correct output path for external protos ([#3002](https://github.com/bazelbuild/rules_nodejs/issues/3002)) ([b48176f](https://github.com/bazelbuild/rules_nodejs/commit/b48176f))
* **typescript:** typescript downleveling breaking ESM variant of Angular v13 compiler ([#2987](https://github.com/bazelbuild/rules_nodejs/issues/2987)) ([5e4d17d](https://github.com/bazelbuild/rules_nodejs/commit/5e4d17d))
* update jasmine-reporters to v2.5.0 to fix xmldom vulnerability ([#2994](https://github.com/bazelbuild/rules_nodejs/issues/2994)) ([8ca234b](https://github.com/bazelbuild/rules_nodejs/commit/8ca234b))


### Features

* **core:** patch bazel-skylib; core can use npm ([#3008](https://github.com/bazelbuild/rules_nodejs/issues/3008)) ([e6ead39](https://github.com/bazelbuild/rules_nodejs/commit/e6ead39))
* **examples:** change angular example to ts_project ([#2209](https://github.com/bazelbuild/rules_nodejs/issues/2209)) ([73e625a](https://github.com/bazelbuild/rules_nodejs/commit/73e625a))



# [4.3.0](https://github.com/bazelbuild/rules_nodejs/compare/4.2.0...4.3.0) (2021-09-28)


### Bug Fixes

* **cypress:** use correct label to reference plugins/base.js ([#2977](https://github.com/bazelbuild/rules_nodejs/issues/2977)) ([6acec9d](https://github.com/bazelbuild/rules_nodejs/commit/6acec9d)), closes [#2976](https://github.com/bazelbuild/rules_nodejs/issues/2976)
* **esbuild:** fix depending on testonly targets  ([#2984](https://github.com/bazelbuild/rules_nodejs/issues/2984)) ([4278ef1](https://github.com/bazelbuild/rules_nodejs/commit/4278ef1))
* **runfiles:** don't use false as a no-error value ([#2974](https://github.com/bazelbuild/rules_nodejs/issues/2974)) ([de1eaf6](https://github.com/bazelbuild/rules_nodejs/commit/de1eaf6))


### Features

* **builtin:** expose a concrete toolchain for rules that don't under… ([#2960](https://github.com/bazelbuild/rules_nodejs/issues/2960)) ([64ad805](https://github.com/bazelbuild/rules_nodejs/commit/64ad805))
* **esbuild:** bump version of esbuild to 0.13.2 ([#2985](https://github.com/bazelbuild/rules_nodejs/issues/2985)) ([4bb25bf](https://github.com/bazelbuild/rules_nodejs/commit/4bb25bf))
* **typescript:** support for ESM variant of the Angular compiler plugin ([#2982](https://github.com/bazelbuild/rules_nodejs/issues/2982)) ([6f97a7c](https://github.com/bazelbuild/rules_nodejs/commit/6f97a7c))



# [4.2.0](https://github.com/bazelbuild/rules_nodejs/compare/4.1.0...4.2.0) (2021-09-17)


### Bug Fixes

* **builtin:** pkg_npm unable to copy files from transitioned actions ([#2942](https://github.com/bazelbuild/rules_nodejs/issues/2942)) ([4291e20](https://github.com/bazelbuild/rules_nodejs/commit/4291e20))
* **typescript:** exclude package(-lock).json from default ts_project srcs ([0245b6d](https://github.com/bazelbuild/rules_nodejs/commit/0245b6d))
* **worker:** check if the input digest present ([b43c594](https://github.com/bazelbuild/rules_nodejs/commit/b43c594))


### Features

* **builtin:** add a toolchain to new core that exposes the node for any platform ([20f4a8f](https://github.com/bazelbuild/rules_nodejs/commit/20f4a8f))
* **builtin:** add support for stopping propagation of `link_node_modules` aspect ([dedc982](https://github.com/bazelbuild/rules_nodejs/commit/dedc982))
* introduce "core" package at /nodejs ([a32cf5c](https://github.com/bazelbuild/rules_nodejs/commit/a32cf5c))



# [4.1.0](https://github.com/bazelbuild/rules_nodejs/compare/4.0.0...4.1.0) (2021-09-10)


### Bug Fixes

* **typescript:** write json srcs to generated tsconfig for resolveJsonModule+composite projects ([c70a07b](https://github.com/bazelbuild/rules_nodejs/commit/c70a07b))
* readme ([52ee4ef](https://github.com/bazelbuild/rules_nodejs/commit/52ee4ef))
* remove dependency on shelljs in esbuild-update script ([0f17126](https://github.com/bazelbuild/rules_nodejs/commit/0f17126))
* update `tsutils` to version `3.21.0` ([bba5494](https://github.com/bazelbuild/rules_nodejs/commit/bba5494)), closes [/github.com/bazelbuild/rules_nodejs/blob/9b454e38f7e2bbc64f75ee9a7dcb6ff45f1c7a12/third_party/github.com/bazelbuild/rules_typescript/internal/tsetse/rules/check_return_value_rule.ts#L60-L62](https://github.com//github.com/bazelbuild/rules_nodejs/blob/9b454e38f7e2bbc64f75ee9a7dcb6ff45f1c7a12/third_party/github.com/bazelbuild/rules_typescript/internal/tsetse/rules/check_return_value_rule.ts/issues/L60-L62)
* use execSync to call npm ([acc64f9](https://github.com/bazelbuild/rules_nodejs/commit/acc64f9))


### Features

* **builtin:** add support for using a local .nvmrc file for providing a node version ([#2911](https://github.com/bazelbuild/rules_nodejs/issues/2911)) ([44740df](https://github.com/bazelbuild/rules_nodejs/commit/44740df))
* decouple @bazel/worker from rules_typescript ([#2918](https://github.com/bazelbuild/rules_nodejs/issues/2918)) ([bda0472](https://github.com/bazelbuild/rules_nodejs/commit/bda0472))



# [4.0.0](https://github.com/bazelbuild/rules_nodejs/compare/4.0.0-rc.1...4.0.0) (2021-08-24)


### Bug Fixes

* **create:** remove skylib install ([0125a46](https://github.com/bazelbuild/rules_nodejs/commit/0125a46))
* update Skylib hash ([010d12f](https://github.com/bazelbuild/rules_nodejs/commit/010d12f))
* **cypress:** add js files from JSModuleInfo to plugin wrapper sources if available ([1274c59](https://github.com/bazelbuild/rules_nodejs/commit/1274c59))
* update usages of `ExternalNpmPackageInfo.path` to be safe and default to empty string ([eacbcf7](https://github.com/bazelbuild/rules_nodejs/commit/eacbcf7)), closes [/github.com/bazelbuild/rules_postcss/blob/2bd16fda40cd4bf4fbf0b477b968366ec1602103/internal/plugin.bzl#L30-L41](https://github.com//github.com/bazelbuild/rules_postcss/blob/2bd16fda40cd4bf4fbf0b477b968366ec1602103/internal/plugin.bzl/issues/L30-L41)



# [4.0.0-rc.1](https://github.com/bazelbuild/rules_nodejs/compare/4.0.0-rc.0...4.0.0-rc.1) (2021-08-19)


### Bug Fixes

* **cypress:** don't eager-fetch the npm repository just to get the cypress toolchain ([e661da6](https://github.com/bazelbuild/rules_nodejs/commit/e661da6))
* **esbuild:** distribute esbuild_repositories function in the built-in, not the @bazel/esbuild package ([c164c6d](https://github.com/bazelbuild/rules_nodejs/commit/c164c6d))

### Features

* **builtin:** added support of linux ppc64le ([582ecc1](https://github.com/bazelbuild/rules_nodejs/commit/582ecc1))



# [4.0.0-rc.0](https://github.com/bazelbuild/rules_nodejs/compare/4.0.0-beta.1...4.0.0-rc.0) (2021-08-13)


### Bug Fixes

* remove current directory prefix when running from execroot ([9771b74](https://github.com/bazelbuild/rules_nodejs/commit/9771b74))
* **builtin:** correctly calculate pkg._directDependency when a mapped node_module is used ([32551a5](https://github.com/bazelbuild/rules_nodejs/commit/32551a5))
* **typescript:** do not re-declare .json output files in srcs if they are already generated files ([38a9584](https://github.com/bazelbuild/rules_nodejs/commit/38a9584))
* **typescript:** document tsc_test for typecheck-only ([20f90c5](https://github.com/bazelbuild/rules_nodejs/commit/20f90c5))
* version script should replace beta versions ([e8f5f06](https://github.com/bazelbuild/rules_nodejs/commit/e8f5f06))


### Features

* **esbuild:** add support for plugins via supplying a configuration file ([5551bff](https://github.com/bazelbuild/rules_nodejs/commit/5551bff))
* **esbuild:** add support for plugins via supplying a configuration file ([#2840](https://github.com/bazelbuild/rules_nodejs/issues/2840)) ([c95d9ca](https://github.com/bazelbuild/rules_nodejs/commit/c95d9ca))
* **esbuild:** support stamping via config file ([413f73d](https://github.com/bazelbuild/rules_nodejs/commit/413f73d))



# [4.0.0-beta.1](https://github.com/bazelbuild/rules_nodejs/compare/4.0.0-beta.0...4.0.0-beta.1) (2021-07-27)


### Bug Fixes

* add missing SHA for rules_proto ([#2830](https://github.com/bazelbuild/rules_nodejs/issues/2830)) ([e822b95](https://github.com/bazelbuild/rules_nodejs/commit/e822b95))
* **esbuild:** generate correct path mappings as mappings aspect no longer produces an array ([#2834](https://github.com/bazelbuild/rules_nodejs/issues/2834)) ([b79e3f4](https://github.com/bazelbuild/rules_nodejs/commit/b79e3f4))
* **typescript:** fix bug in ts_project (write_tsconfig_rule) when extending from a generated tsconfig in same folder ([4e396eb](https://github.com/bazelbuild/rules_nodejs/commit/4e396eb))


### Features

* **builtin:** add package_json_remove and package_json_replace attributes to yarn_install & npm_install ([b68be36](https://github.com/bazelbuild/rules_nodejs/commit/b68be36))



# [4.0.0-beta.0](https://github.com/bazelbuild/rules_nodejs/compare/3.5.0...4.0.0-beta.0) (2021-07-02)


### Bug Fixes

* **builtin:** don't expose any darwin_arm64 repo or toolchains if not supported by the node version ([6748383](https://github.com/bazelbuild/rules_nodejs/commit/6748383)), closes [#2779](https://github.com/bazelbuild/rules_nodejs/issues/2779)
* **builtin:** fix npm_install & yarn_install post_install_patches when symlink_node_modules is enabled ([5fce733](https://github.com/bazelbuild/rules_nodejs/commit/5fce733))
* **builtin:** generated nodejs repository for windows references non-existent file ([4487698](https://github.com/bazelbuild/rules_nodejs/commit/4487698))
* **builtin:** propogate tags to both generated targets in generated_file_test ([e980107](https://github.com/bazelbuild/rules_nodejs/commit/e980107))
* **builtin:** runfile resolution incorrect if entry starts similarly ([3be2902](https://github.com/bazelbuild/rules_nodejs/commit/3be2902))
* **builtin:** write stdout/stderr to correct path under chdir ([#2681](https://github.com/bazelbuild/rules_nodejs/issues/2681)) ([99760a5](https://github.com/bazelbuild/rules_nodejs/commit/99760a5)), closes [#2680](https://github.com/bazelbuild/rules_nodejs/issues/2680)
* **esbuild:** prefer finding entry_point files in deps rather than srcs ([#2692](https://github.com/bazelbuild/rules_nodejs/issues/2692)) ([5f4bb15](https://github.com/bazelbuild/rules_nodejs/commit/5f4bb15))
* **esbuild:** provide JSModuleInfo of output bundle ([#2685](https://github.com/bazelbuild/rules_nodejs/issues/2685)) ([82ef1a1](https://github.com/bazelbuild/rules_nodejs/commit/82ef1a1))
* **esbuild:** update update script file paths after removal of _README.md ([#2695](https://github.com/bazelbuild/rules_nodejs/issues/2695)) ([f320ef0](https://github.com/bazelbuild/rules_nodejs/commit/f320ef0))
* **jasmine:** don't assume entry_point is a label as it may now be a dict ([3fa2e5f](https://github.com/bazelbuild/rules_nodejs/commit/3fa2e5f))
* **jasmine:** unhanded promise rejection causes tests suit to pass ([a511f3d](https://github.com/bazelbuild/rules_nodejs/commit/a511f3d)), closes [3.7.0/lib/jasmine.js#L267](https://github.com/3.7.0/lib/jasmine.js/issues/L267) [#2688](https://github.com/bazelbuild/rules_nodejs/issues/2688)
* **terser:** make terser resolve more robust by not assuming a single /terser/ segment in the path ([95fc9ba](https://github.com/bazelbuild/rules_nodejs/commit/95fc9ba))
* **typescript:** collect coverage in ts_project ([8e7bc1c](https://github.com/bazelbuild/rules_nodejs/commit/8e7bc1c)), closes [#2762](https://github.com/bazelbuild/rules_nodejs/issues/2762)
* allow for only stderr to be set on npm_package_bin ([a04a7ef](https://github.com/bazelbuild/rules_nodejs/commit/a04a7ef))
* **builtin:** add two missing locations where Mac M1 support needs to be declared ([ad20275](https://github.com/bazelbuild/rules_nodejs/commit/ad20275)), closes [#2733](https://github.com/bazelbuild/rules_nodejs/issues/2733)
* **builtin:** support directory_file_path entry_point in nodejs_binary & nodejs_test when --bazel_patch_module_resolver is set ([50e6d1d](https://github.com/bazelbuild/rules_nodejs/commit/50e6d1d))
* **typescript:** fixed "output was not created" error for ts_project with supports_workers ([9a3e5c9](https://github.com/bazelbuild/rules_nodejs/commit/9a3e5c9))
* **typescript:** repair error reporting when a ts_project is missing declaration=True ([5f0be65](https://github.com/bazelbuild/rules_nodejs/commit/5f0be65))
* make generated_file_test `.update`'s visibility same as test rule ([#2677](https://github.com/bazelbuild/rules_nodejs/issues/2677)) ([1ce9dce](https://github.com/bazelbuild/rules_nodejs/commit/1ce9dce))


### chore

* update Bazel minimum version to LTS ([a9c5966](https://github.com/bazelbuild/rules_nodejs/commit/a9c5966))
* **builtin:** flip default for pkg_npm#validate ([16a099e](https://github.com/bazelbuild/rules_nodejs/commit/16a099e))


### Code Refactoring

* **typescript:** tsconfig default to tsconfig.json ([c6ae95c](https://github.com/bazelbuild/rules_nodejs/commit/c6ae95c))


### Features

* **esbuild:** allow for .ts, .tsx and .jsx entry points ([e3edb28](https://github.com/bazelbuild/rules_nodejs/commit/e3edb28))
* **labs:** ts_proto_library directly exports commonjs ([5f26d0f](https://github.com/bazelbuild/rules_nodejs/commit/5f26d0f))
* add package_name to ts_library ([d2d4d16](https://github.com/bazelbuild/rules_nodejs/commit/d2d4d16))
* **builtin:** add validate attribute on pkg_npm ([39eea25](https://github.com/bazelbuild/rules_nodejs/commit/39eea25)), closes [#2782](https://github.com/bazelbuild/rules_nodejs/issues/2782)
* **builtin:** document how nodejs_binary#entry_point can use a direc… ([#2579](https://github.com/bazelbuild/rules_nodejs/issues/2579)) ([fcdcf63](https://github.com/bazelbuild/rules_nodejs/commit/fcdcf63))
* **cypress:** cypress executable toolchain ([#2668](https://github.com/bazelbuild/rules_nodejs/issues/2668)) ([f1f5ee6](https://github.com/bazelbuild/rules_nodejs/commit/f1f5ee6))
* **esbuild:** add support for toolchains ([#2704](https://github.com/bazelbuild/rules_nodejs/issues/2704)) ([ae011bf](https://github.com/bazelbuild/rules_nodejs/commit/ae011bf))
* **esbuild:** filter ts declaration files by default ([f83cf48](https://github.com/bazelbuild/rules_nodejs/commit/f83cf48))
* **typescript:** support typescript 4.3 ([e576acd](https://github.com/bazelbuild/rules_nodejs/commit/e576acd))
* add opt-in exports_directories_only mode to yarn_install and npm_install (defaults to False) ([a7200aa](https://github.com/bazelbuild/rules_nodejs/commit/a7200aa))
* support dict style directory_file_path entry_point in nodejs_binary, nodejs_test & jasmine_node_test ([5fafe19](https://github.com/bazelbuild/rules_nodejs/commit/5fafe19))
* support directory_file_path entry_point in npm_umd_bundle ([8bee1b3](https://github.com/bazelbuild/rules_nodejs/commit/8bee1b3))


### Performance Improvements

* **cypress:** export cypress as a directory symlink ([8ea7ff4](https://github.com/bazelbuild/rules_nodejs/commit/8ea7ff4))


### BREAKING CHANGES

* **builtin:** The `@bazel/runfiles` `lookupDirectory` method has been
removed. Use the `resolve` method instead
* for our 4.0, rules_nodejs requires the current LTS of Bazel. This is a policy restriction to save us time tracking down support issues on older versions of Bazel, not a technical one. You can patch this check out locally if you really need to continue using older Bazel, but this puts you on unsupported track.
* **builtin:** Just follow the printed instructions to either set the right package_name or disable the validation
* **typescript:** ts_project tsconfig attribute now defaults to just 'tsconfig.json' rather than '[name].json'



# [3.6.0](https://github.com/bazelbuild/rules_nodejs/compare/3.5.1...3.6.0) (2021-06-09)


### Bug Fixes

* allow for only stderr to be set on npm_package_bin ([fa8f5b1](https://github.com/bazelbuild/rules_nodejs/commit/fa8f5b1))
* **builtin:** add two missing locations where Mac M1 support needs to be declared ([2ad950f](https://github.com/bazelbuild/rules_nodejs/commit/2ad950f)), closes [#2733](https://github.com/bazelbuild/rules_nodejs/issues/2733)
* **builtin:** propogate tags to both generated targets in generated_file_test ([c4403fc](https://github.com/bazelbuild/rules_nodejs/commit/c4403fc))
* **builtin:** support directory_file_path entry_point in nodejs_binary & nodejs_test when --bazel_patch_module_resolver is set ([51676ef](https://github.com/bazelbuild/rules_nodejs/commit/51676ef))
* **jasmine:** don't assume entry_point is a label as it may now be a dict ([3683466](https://github.com/bazelbuild/rules_nodejs/commit/3683466))
* **jasmine:** unhanded promise rejection causes tests suit to pass ([3c4ef58](https://github.com/bazelbuild/rules_nodejs/commit/3c4ef58)), closes [3.7.0/lib/jasmine.js#L267](https://github.com/3.7.0/lib/jasmine.js/issues/L267) [#2688](https://github.com/bazelbuild/rules_nodejs/issues/2688)
* **terser:** make terser resolve more robust by not assuming a single /terser/ segment in the path ([4709ffb](https://github.com/bazelbuild/rules_nodejs/commit/4709ffb))
* **typescript:** fixed "output was not created" error for ts_project with supports_workers ([807b07b](https://github.com/bazelbuild/rules_nodejs/commit/807b07b))
* **typescript:** repair error reporting when a ts_project is missing declaration=True ([cd08efe](https://github.com/bazelbuild/rules_nodejs/commit/cd08efe))


### Features

* add opt-in exports_directories_only mode to yarn_install and npm_install (defaults to False) ([3d182cf](https://github.com/bazelbuild/rules_nodejs/commit/3d182cf))
* support dict style directory_file_path entry_point in nodejs_binary, nodejs_test & jasmine_node_test ([737674f](https://github.com/bazelbuild/rules_nodejs/commit/737674f))
* support directory_file_path entry_point in npm_umd_bundle ([4e44178](https://github.com/bazelbuild/rules_nodejs/commit/4e44178))



## [3.5.1](https://github.com/bazelbuild/rules_nodejs/compare/3.5.0...3.5.1) (2021-05-25)


### Bug Fixes

* **builtin:** generated nodejs repository for windows references non-existent file ([c1663c5](https://github.com/bazelbuild/rules_nodejs/commit/c1663c5))
* **builtin:** write stdout/stderr to correct path under chdir ([#2681](https://github.com/bazelbuild/rules_nodejs/issues/2681)) ([36311bb](https://github.com/bazelbuild/rules_nodejs/commit/36311bb)), closes [#2680](https://github.com/bazelbuild/rules_nodejs/issues/2680)
* **esbuild:** prefer finding entry_point files in deps rather than srcs ([#2692](https://github.com/bazelbuild/rules_nodejs/issues/2692)) ([dd4c4f3](https://github.com/bazelbuild/rules_nodejs/commit/dd4c4f3))
* **esbuild:** provide JSModuleInfo of output bundle ([#2685](https://github.com/bazelbuild/rules_nodejs/issues/2685)) ([233254d](https://github.com/bazelbuild/rules_nodejs/commit/233254d))
* **esbuild:** update update script file paths after removal of _README.md ([#2695](https://github.com/bazelbuild/rules_nodejs/issues/2695)) ([25a5ac4](https://github.com/bazelbuild/rules_nodejs/commit/25a5ac4))
* make generated_file_test `.update`'s visibility same as test rule ([#2677](https://github.com/bazelbuild/rules_nodejs/issues/2677)) ([30bc86c](https://github.com/bazelbuild/rules_nodejs/commit/30bc86c))


### Features

* **builtin:** document how nodejs_binary#entry_point can use a direc… ([#2579](https://github.com/bazelbuild/rules_nodejs/issues/2579)) ([ceddd1d](https://github.com/bazelbuild/rules_nodejs/commit/ceddd1d))



# [3.5.0](https://github.com/bazelbuild/rules_nodejs/compare/3.4.2...3.5.0) (2021-05-11)


### Bug Fixes

* **builtin:** account for racy deletion of symlink in linker ([#2662](https://github.com/bazelbuild/rules_nodejs/issues/2662)) ([e9a683d](https://github.com/bazelbuild/rules_nodejs/commit/e9a683d))
* **builtin:** include optionalDependencies in strictly visible packages ([#2657](https://github.com/bazelbuild/rules_nodejs/issues/2657)) ([2a1ed31](https://github.com/bazelbuild/rules_nodejs/commit/2a1ed31))
* **builtin:** linker incorrectly resolves workspace `node_modules` for windows ([#2659](https://github.com/bazelbuild/rules_nodejs/issues/2659)) ([7cf7d73](https://github.com/bazelbuild/rules_nodejs/commit/7cf7d73))
* **concatjs:** devserver not passing through tags to all targets ([#2646](https://github.com/bazelbuild/rules_nodejs/issues/2646)) ([8abc8e0](https://github.com/bazelbuild/rules_nodejs/commit/8abc8e0))
* **docs:** correct title of stamping docs ([4bea5b2](https://github.com/bazelbuild/rules_nodejs/commit/4bea5b2))
* **protractor:** unable to specify `server` as configurable attribute ([#2643](https://github.com/bazelbuild/rules_nodejs/issues/2643)) ([4965db6](https://github.com/bazelbuild/rules_nodejs/commit/4965db6))


### Features

* **builtin:** add js_library JSEcmaScriptModuleInfo support ([#2658](https://github.com/bazelbuild/rules_nodejs/issues/2658)) ([5ad1596](https://github.com/bazelbuild/rules_nodejs/commit/5ad1596))
* **builtin:** allow bundling ESM output with the pkg_npm rule ([#2648](https://github.com/bazelbuild/rules_nodejs/issues/2648)) ([911529f](https://github.com/bazelbuild/rules_nodejs/commit/911529f))
* **concatjs:** enable junit report for karma_web_test ([#2630](https://github.com/bazelbuild/rules_nodejs/issues/2630)) ([28e8d23](https://github.com/bazelbuild/rules_nodejs/commit/28e8d23))
* **esbuild:** add support for multiple entry points ([#2663](https://github.com/bazelbuild/rules_nodejs/issues/2663)) ([b4f322a](https://github.com/bazelbuild/rules_nodejs/commit/b4f322a))
* **esbuild:** default log-level flag to warning, unless overridden ([#2664](https://github.com/bazelbuild/rules_nodejs/issues/2664)) ([8ffea3e](https://github.com/bazelbuild/rules_nodejs/commit/8ffea3e))



## [3.4.2](https://github.com/bazelbuild/rules_nodejs/compare/3.4.1...3.4.2) (2021-04-28)


### Bug Fixes

* **builtin:** allow bazel version to have semver build metadata ([#2624](https://github.com/bazelbuild/rules_nodejs/issues/2624)) ([6a2e136](https://github.com/bazelbuild/rules_nodejs/commit/6a2e136))


### Features

* **builtin:** add version 16.x.x versions of NodeJS ([#2626](https://github.com/bazelbuild/rules_nodejs/issues/2626)) ([fc34588](https://github.com/bazelbuild/rules_nodejs/commit/fc34588))



## [3.4.1](https://github.com/bazelbuild/rules_nodejs/compare/3.4.0...3.4.1) (2021-04-22)


### Bug Fixes

* **builtin:** don't restart npm_install rule just to look up a label ([#2621](https://github.com/bazelbuild/rules_nodejs/issues/2621)) ([16d3a25](https://github.com/bazelbuild/rules_nodejs/commit/16d3a25)), closes [#2620](https://github.com/bazelbuild/rules_nodejs/issues/2620)
* **builtin:** gracefully handle the case of empty yarn_urls ([#2619](https://github.com/bazelbuild/rules_nodejs/issues/2619)) ([fea3db3](https://github.com/bazelbuild/rules_nodejs/commit/fea3db3))
* **builtin:** properly parse status file value containing spaces ([#2615](https://github.com/bazelbuild/rules_nodejs/issues/2615)) ([406dcb5](https://github.com/bazelbuild/rules_nodejs/commit/406dcb5))
* **builtin:** resolve vendored node/yarn from external repo ([#2614](https://github.com/bazelbuild/rules_nodejs/issues/2614)) ([3564940](https://github.com/bazelbuild/rules_nodejs/commit/3564940)), closes [#2019](https://github.com/bazelbuild/rules_nodejs/issues/2019)
* **concatjs:** update karma to 6.3.2 and fix [#2093](https://github.com/bazelbuild/rules_nodejs/issues/2093) ([#2603](https://github.com/bazelbuild/rules_nodejs/issues/2603)) ([c80479d](https://github.com/bazelbuild/rules_nodejs/commit/c80479d))
* **esbuild:** correct rule argument documentation stating default target ([#2608](https://github.com/bazelbuild/rules_nodejs/issues/2608)) ([e710a6b](https://github.com/bazelbuild/rules_nodejs/commit/e710a6b))
* **examples:** transpile Angular es5 bundle to SystemJS ([#2562](https://github.com/bazelbuild/rules_nodejs/issues/2562)) ([b0175cd](https://github.com/bazelbuild/rules_nodejs/commit/b0175cd))
* **typescript:** handle .tsx inputs to angular ([#2613](https://github.com/bazelbuild/rules_nodejs/issues/2613)) ([901df38](https://github.com/bazelbuild/rules_nodejs/commit/901df38)), closes [#2542](https://github.com/bazelbuild/rules_nodejs/issues/2542)
* add support for terser 5 under node 12 and higher ([#2558](https://github.com/bazelbuild/rules_nodejs/issues/2558)) ([bd53eb5](https://github.com/bazelbuild/rules_nodejs/commit/bd53eb5))



# [3.4.0](https://github.com/bazelbuild/rules_nodejs/compare/3.3.0...3.4.0) (2021-04-14)


### Bug Fixes

* **esbuild:** use run_node to invoke linker before running esuild ([be184c2](https://github.com/bazelbuild/rules_nodejs/commit/be184c2))
* **typescript:** output path for .json in root package ([#2602](https://github.com/bazelbuild/rules_nodejs/issues/2602)) ([1c50e96](https://github.com/bazelbuild/rules_nodejs/commit/1c50e96)), closes [#2598](https://github.com/bazelbuild/rules_nodejs/issues/2598)


### Features

* add pre and post install patches to yarn_install and npm_install ([#2607](https://github.com/bazelbuild/rules_nodejs/issues/2607)) ([d805f33](https://github.com/bazelbuild/rules_nodejs/commit/d805f33))
* support for multi-linked first party dependencies ([e90b4ae](https://github.com/bazelbuild/rules_nodejs/commit/e90b4ae))
* **esbuild:** add output_css flag to esbuild() ([#2545](https://github.com/bazelbuild/rules_nodejs/issues/2545)) ([c5ed4f8](https://github.com/bazelbuild/rules_nodejs/commit/c5ed4f8))
* **esbuild:** allow ts / tsx files in esbuilds srcs ([#2594](https://github.com/bazelbuild/rules_nodejs/issues/2594)) ([9e91872](https://github.com/bazelbuild/rules_nodejs/commit/9e91872))



# [3.3.0](https://github.com/bazelbuild/rules_nodejs/compare/3.2.3...3.3.0) (2021-04-08)


### Bug Fixes

* **builtin:** provide proper error if npm_package_bin has no outs ([#2557](https://github.com/bazelbuild/rules_nodejs/issues/2557)) ([c47b770](https://github.com/bazelbuild/rules_nodejs/commit/c47b770))
* **esbuild:** 'output' is passed twice when used ([#2587](https://github.com/bazelbuild/rules_nodejs/issues/2587)) ([57218a6](https://github.com/bazelbuild/rules_nodejs/commit/57218a6))
* **esbuild:** files not being found when building external repo ([#2563](https://github.com/bazelbuild/rules_nodejs/issues/2563)) ([d10e17c](https://github.com/bazelbuild/rules_nodejs/commit/d10e17c))
* **esbuild:** update to esbuild 0.11 ([#2559](https://github.com/bazelbuild/rules_nodejs/issues/2559)) ([e9e8fe7](https://github.com/bazelbuild/rules_nodejs/commit/e9e8fe7)), closes [#2552](https://github.com/bazelbuild/rules_nodejs/issues/2552)
* **jasmine:** transitive specs are no longer added to the test suite ([#2576](https://github.com/bazelbuild/rules_nodejs/issues/2576)) ([e7eaf34](https://github.com/bazelbuild/rules_nodejs/commit/e7eaf34))


### Features

* introduce package for runfile helpers ([2c883d1](https://github.com/bazelbuild/rules_nodejs/commit/2c883d1))
* make node toolchain_type public so new toolchains can be added ([#2591](https://github.com/bazelbuild/rules_nodejs/issues/2591)) ([b606b79](https://github.com/bazelbuild/rules_nodejs/commit/b606b79)), closes [#2565](https://github.com/bazelbuild/rules_nodejs/issues/2565)
* **esbuild:** Script to update esbuild to the latest available version ([#2492](https://github.com/bazelbuild/rules_nodejs/issues/2492)) ([472ed62](https://github.com/bazelbuild/rules_nodejs/commit/472ed62))
* **esbuild:** support location expansion in esbuild args ([#2564](https://github.com/bazelbuild/rules_nodejs/issues/2564)) ([eb3bd7e](https://github.com/bazelbuild/rules_nodejs/commit/eb3bd7e))
* **typescript:** add support for "jsx: preserve" compiler option ([#2574](https://github.com/bazelbuild/rules_nodejs/issues/2574)) ([425dbd6](https://github.com/bazelbuild/rules_nodejs/commit/425dbd6))



## [3.2.3](https://github.com/bazelbuild/rules_nodejs/compare/3.2.2...3.2.3) (2021-03-25)


### Bug Fixes

* **builtin:** add transitive typings to runfiles provider produced by js_library ([#2547](https://github.com/bazelbuild/rules_nodejs/issues/2547)) ([41117fa](https://github.com/bazelbuild/rules_nodejs/commit/41117fa))
* **builtin:** always install source-map-support ([#2538](https://github.com/bazelbuild/rules_nodejs/issues/2538)) ([97b3886](https://github.com/bazelbuild/rules_nodejs/commit/97b3886)), closes [#2520](https://github.com/bazelbuild/rules_nodejs/issues/2520)
* **esbuild:** allow empty string as an input to sourcemap for bazel 2.x support ([#2549](https://github.com/bazelbuild/rules_nodejs/issues/2549)) ([3b3e020](https://github.com/bazelbuild/rules_nodejs/commit/3b3e020))
* **typescript:** update documentation now that ts_project is recommended ([#2548](https://github.com/bazelbuild/rules_nodejs/issues/2548)) ([a8d8b0f](https://github.com/bazelbuild/rules_nodejs/commit/a8d8b0f))
* tsconfig validator fails on chained tsconfig references ([#2512](https://github.com/bazelbuild/rules_nodejs/issues/2512)) ([bfd74e5](https://github.com/bazelbuild/rules_nodejs/commit/bfd74e5))
* **examples:** remove relativeLinkResolution ([#2530](https://github.com/bazelbuild/rules_nodejs/issues/2530)) ([8ef60e5](https://github.com/bazelbuild/rules_nodejs/commit/8ef60e5))


### Features

* **builtin:** first experimental rules for npm tarballs ([#2544](https://github.com/bazelbuild/rules_nodejs/issues/2544)) ([aa09b57](https://github.com/bazelbuild/rules_nodejs/commit/aa09b57))
* **esbuild:** add 'sourcemap' option to configure sourcemap generation ([#2528](https://github.com/bazelbuild/rules_nodejs/issues/2528)) ([8d0218c](https://github.com/bazelbuild/rules_nodejs/commit/8d0218c))



## [3.2.2](https://github.com/bazelbuild/rules_nodejs/compare/3.2.1...3.2.2) (2021-03-08)


### Bug Fixes

* **esbuild:** run npm version check as postinstall ([#2500](https://github.com/bazelbuild/rules_nodejs/issues/2500)) ([2efe437](https://github.com/bazelbuild/rules_nodejs/commit/2efe437))
* **esbuild:** set correct base url when rule is at root ([#2506](https://github.com/bazelbuild/rules_nodejs/issues/2506)) ([92e8169](https://github.com/bazelbuild/rules_nodejs/commit/92e8169))
* **rollup:** include externals config in worker cache key ([de9dd86](https://github.com/bazelbuild/rules_nodejs/commit/de9dd86))


### Features

* **builtin:** add env attribute to nodejs test and binary and run_node helper ([#2499](https://github.com/bazelbuild/rules_nodejs/issues/2499)) ([c9b159f](https://github.com/bazelbuild/rules_nodejs/commit/c9b159f))
* **esbuild:** add max_threads setting to limit number of threads used ([8e7c731](https://github.com/bazelbuild/rules_nodejs/commit/8e7c731))
* **examples:** update angular_bazel_architect to version 11 ([#2495](https://github.com/bazelbuild/rules_nodejs/issues/2495)) ([b8a4dcd](https://github.com/bazelbuild/rules_nodejs/commit/b8a4dcd))



## [3.2.1](https://github.com/bazelbuild/rules_nodejs/compare/3.2.0...3.2.1) (2021-02-23)


### Bug Fixes

* remove `--keep-names` ([4a26898](https://github.com/bazelbuild/rules_nodejs/commit/4a26898))
* update node versions map ([#2484](https://github.com/bazelbuild/rules_nodejs/issues/2484)) ([9506fe0](https://github.com/bazelbuild/rules_nodejs/commit/9506fe0))
* **esbuild:** add --preserve-symlinks flag by default ([eb71285](https://github.com/bazelbuild/rules_nodejs/commit/eb71285))
* **esbuild:** add link_workspace_root for workspace absolute imports ([#2476](https://github.com/bazelbuild/rules_nodejs/issues/2476)) ([ba7e48e](https://github.com/bazelbuild/rules_nodejs/commit/ba7e48e)), closes [#2474](https://github.com/bazelbuild/rules_nodejs/issues/2474)
* use ':' instead of '=' for esbuild 'define' argument ([#2469](https://github.com/bazelbuild/rules_nodejs/issues/2469)) ([b0fddae](https://github.com/bazelbuild/rules_nodejs/commit/b0fddae))
* use ':' instead of '=' for esbuild 'external' argument ([#2475](https://github.com/bazelbuild/rules_nodejs/issues/2475)) ([bc7dc82](https://github.com/bazelbuild/rules_nodejs/commit/bc7dc82))


### Features

* add generate_local_modules_build_files flag to yarn_install and npm_install rules ([#2449](https://github.com/bazelbuild/rules_nodejs/issues/2449)) ([a6449b7](https://github.com/bazelbuild/rules_nodejs/commit/a6449b7))
* **typescript:** add `data` attribute ([ac2097c](https://github.com/bazelbuild/rules_nodejs/commit/ac2097c))



# [3.2.0](https://github.com/bazelbuild/rules_nodejs/compare/3.1.0...3.2.0) (2021-02-13)


### Bug Fixes

* multi-linker linking when only output files in sandbox ([ebb9481](https://github.com/bazelbuild/rules_nodejs/commit/ebb9481))
* **builtin:** fix coverage source file paths ([ae4ec78](https://github.com/bazelbuild/rules_nodejs/commit/ae4ec78))
* **docs:** fix formatting of nodejs_binary#chdir ([1caced8](https://github.com/bazelbuild/rules_nodejs/commit/1caced8))
* **docs:** fix regex that replaces //packages with [@bazel](https://github.com/bazel) ([c31c0b6](https://github.com/bazelbuild/rules_nodejs/commit/c31c0b6))
* **docs:** version selector shows 3.x ([38f4f78](https://github.com/bazelbuild/rules_nodejs/commit/38f4f78))
* **typescript:** allow up to typescript 4.2, add tests for 3.7-4.1 ([ea168a7](https://github.com/bazelbuild/rules_nodejs/commit/ea168a7))
* **typescript:** fixed build for external ts_project targets ([c89e0aa](https://github.com/bazelbuild/rules_nodejs/commit/c89e0aa))
* version number not edited after release candidate ([ac2bb62](https://github.com/bazelbuild/rules_nodejs/commit/ac2bb62))


### Features

* add esbuild package ([e7e5286](https://github.com/bazelbuild/rules_nodejs/commit/e7e5286))
* **builtin:** add coverage all: true support ([8386b97](https://github.com/bazelbuild/rules_nodejs/commit/8386b97))
* support for nested node_modules in linker ([2c2cc6e](https://github.com/bazelbuild/rules_nodejs/commit/2c2cc6e))



# [3.1.0](https://github.com/bazelbuild/rules_nodejs/compare/3.0.0...3.1.0) (2021-02-02)


### Bug Fixes

* forward srcs, deps and visibility of dummy_bzl_library to the filegroup when publishing ([0466084](https://github.com/bazelbuild/rules_nodejs/commit/0466084))
* linker fix for invalid symlink creation path in createSymlinkAndPreserveContents ([14086a8](https://github.com/bazelbuild/rules_nodejs/commit/14086a8))
* relative data paths in yarn_install & npm_install when symlink_node_modules=False and package.json is not at root ([3c12dfe](https://github.com/bazelbuild/rules_nodejs/commit/3c12dfe))
* **builtin:** only generate a .tar pkg_npm output when requested ([#2428](https://github.com/bazelbuild/rules_nodejs/issues/2428)) ([4d8f15c](https://github.com/bazelbuild/rules_nodejs/commit/4d8f15c))
* **builtin:** pass quiet attr though to build file generation on npm / yarn install ([#2400](https://github.com/bazelbuild/rules_nodejs/issues/2400)) ([ceb76d6](https://github.com/bazelbuild/rules_nodejs/commit/ceb76d6))
* **builtin:** when using chdir attribute, don't write to source dir ([3eb4260](https://github.com/bazelbuild/rules_nodejs/commit/3eb4260))
* **typescript:** capture js files in outputs of ts_project if allow_js ([9d7827b](https://github.com/bazelbuild/rules_nodejs/commit/9d7827b))
* remove mirror.bazel.build from list of node_urls used to fetch NodeJS by default ([#2408](https://github.com/bazelbuild/rules_nodejs/issues/2408)) ([67b494b](https://github.com/bazelbuild/rules_nodejs/commit/67b494b))
* skip update NodeJS versions action on forks ([#2396](https://github.com/bazelbuild/rules_nodejs/issues/2396)) ([4e40d25](https://github.com/bazelbuild/rules_nodejs/commit/4e40d25))
* **examples:** angualr universal_server ([d5e8413](https://github.com/bazelbuild/rules_nodejs/commit/d5e8413))
* **update-nodejs-versions:** Fix NodeJS version for running GitHub Action ([4ab8252](https://github.com/bazelbuild/rules_nodejs/commit/4ab8252))


### Features

* **builtin:** add a chdir attribute to nodejs_test and npm_package_bin ([0fde42b](https://github.com/bazelbuild/rules_nodejs/commit/0fde42b)), closes [#2323](https://github.com/bazelbuild/rules_nodejs/issues/2323)
* **typescript:** create a better ts_project worker ([#2416](https://github.com/bazelbuild/rules_nodejs/issues/2416)) ([99bfe5f](https://github.com/bazelbuild/rules_nodejs/commit/99bfe5f))
* add support for NodeJS 15.x ([#2366](https://github.com/bazelbuild/rules_nodejs/issues/2366)) ([924fa41](https://github.com/bazelbuild/rules_nodejs/commit/924fa41))



# [3.0.0](https://github.com/bazelbuild/rules_nodejs/compare/3.0.0-rc.1...3.0.0) (2020-12-22)

> ### For a full list for the breaking changes in 3.0.0 and other notes on migrating, see the [Migrating to 3.0.0 wiki](https://github.com/bazelbuild/rules_nodejs/wiki#migrating-to-30) page.

### Bug Fixes

* **builtin:** only pass kwargs to the test, not the .update binary ([#2361](https://github.com/bazelbuild/rules_nodejs/issues/2361)) ([afa095b](https://github.com/bazelbuild/rules_nodejs/commit/afa095b))


### Code Refactoring

* **builtin:** remove node_modules attribute from nodejs_binary, nodejs_test & ts_library ([c2927af](https://github.com/bazelbuild/rules_nodejs/commit/c2927af))


### BREAKING CHANGES

* **builtin:** We removed the node_modules attribute from `nodejs_binary`, `nodejs_test`, `jasmine_node_test` & `ts_library`.

If you are using the `node_modules` attribute, you can simply add the target specified there to the `data` or `deps` attribute of the rule instead.

For example,

```
nodejs_test(
    name = "test",
    data = [
        "test.js",
        "@npm//:node_modules",
    ],
    entry_point = "test.js",
)
```

or

```
ts_library(
    name = "lib",
    srcs = glob(["*.ts"]),
    tsconfig = ":tsconfig.json",
    deps = ["@npm//:node_modules"],
)
```

We also dropped support for filegroup based node_modules target and removed `node_modules_filegroup` from `index.bzl`.

If you are using this feature for user-managed deps, you must now a `js_library` target
with `external_npm_package` set to `True` instead.

For example,

```
js_library(
    name = "node_modules",
    srcs = glob(
        include = [
            "node_modules/**/*.js",
            "node_modules/**/*.d.ts",
            "node_modules/**/*.json",
            "node_modules/.bin/*",
        ],
        exclude = [
            # Files under test & docs may contain file names that
            # are not legal Bazel labels (e.g.,
            # node_modules/ecstatic/test/public/中文/檔案.html)
            "node_modules/**/test/**",
            "node_modules/**/docs/**",
            # Files with spaces in the name are not legal Bazel labels
            "node_modules/**/* */**",
            "node_modules/**/* *",
        ],
    ),
    # Provide ExternalNpmPackageInfo which is used by downstream rules
    # that use these npm dependencies
    external_npm_package = True,
)

nodejs_test(
    name = "test",
    data = [
        "test.js",
        ":node_modules",
    ],
    entry_point = "test.js",
)
```

See `examples/user_managed_deps` for a working example of user-managed npm dependencies.



# [3.0.0-rc.1](https://github.com/bazelbuild/rules_nodejs/compare/3.0.0-rc.0...3.0.0-rc.1) (2020-12-18)


### Bug Fixes

* **builtin:** add DeclarationInfo sources from dependencies as inputs to npm_package_bin driven actions ([#2353](https://github.com/bazelbuild/rules_nodejs/issues/2353)) ([a549411](https://github.com/bazelbuild/rules_nodejs/commit/a549411))


### Features

* **builtin:** use npm ci as default behaviour for installing node_modules ([#2328](https://github.com/bazelbuild/rules_nodejs/issues/2328)) ([1d650fb](https://github.com/bazelbuild/rules_nodejs/commit/1d650fb)), closes [#159](https://github.com/bazelbuild/rules_nodejs/issues/159)
* allow running NPM tools from execroot ([#2297](https://github.com/bazelbuild/rules_nodejs/issues/2297)) ([2a4ba8f](https://github.com/bazelbuild/rules_nodejs/commit/2a4ba8f))
* create symlink for build files present on node modules installed with relative paths ([#2330](https://github.com/bazelbuild/rules_nodejs/issues/2330)) ([6f4fc17](https://github.com/bazelbuild/rules_nodejs/commit/6f4fc17))
* **builtin:** yarn install use --frozen-lockfile as default ([b6a8cbb](https://github.com/bazelbuild/rules_nodejs/commit/b6a8cbb)), closes [#941](https://github.com/bazelbuild/rules_nodejs/issues/941)



# [3.0.0-rc.0](https://github.com/bazelbuild/rules_nodejs/compare/2.2.2...3.0.0-rc.0) (2020-12-11)


### Bug Fixes

* **builtin:** --nobazel_run_linker implies --bazel_patch_module_resolver ([7100277](https://github.com/bazelbuild/rules_nodejs/commit/7100277))
* remove jasmine-core as a peer dep ([#2336](https://github.com/bazelbuild/rules_nodejs/issues/2336)) ([bb2a302](https://github.com/bazelbuild/rules_nodejs/commit/bb2a302))
* **builtin:** give a longer timeout for _create_build_files ([5d405a7](https://github.com/bazelbuild/rules_nodejs/commit/5d405a7)), closes [#2231](https://github.com/bazelbuild/rules_nodejs/issues/2231)
* **builtin:** give better error when linker runs on Node <10 ([b9dc2c1](https://github.com/bazelbuild/rules_nodejs/commit/b9dc2c1)), closes [#2304](https://github.com/bazelbuild/rules_nodejs/issues/2304)
* **builtin:** make linker deterministic when resolving from manifest & fix link_workspace_root with no runfiles ([f7c342f](https://github.com/bazelbuild/rules_nodejs/commit/f7c342f))
* **examples:** fix jest example on windows ([3ffefa1](https://github.com/bazelbuild/rules_nodejs/commit/3ffefa1)), closes [#1454](https://github.com/bazelbuild/rules_nodejs/issues/1454)
* **exmaples/nestjs:** add module_name field in ts_library ([3a4155c](https://github.com/bazelbuild/rules_nodejs/commit/3a4155c))
* **typescript:** don't depend on protobufjs, it's transitive ([1b344db](https://github.com/bazelbuild/rules_nodejs/commit/1b344db))
* **typescript:** fail the build when ts_project produces zero outputs ([3ca6cac](https://github.com/bazelbuild/rules_nodejs/commit/3ca6cac)), closes [#2301](https://github.com/bazelbuild/rules_nodejs/issues/2301)
* npm_package.pack on Windows should not generate undefined.tgz ([715ad22](https://github.com/bazelbuild/rules_nodejs/commit/715ad22))
* **typescript:** specify rootDir as absolute path ([535fa51](https://github.com/bazelbuild/rules_nodejs/commit/535fa51))
* npm_package.pack should work in windows os ([503d6fb](https://github.com/bazelbuild/rules_nodejs/commit/503d6fb))
* **typescript:** don't include _valid_options marker file in outs ([570e34d](https://github.com/bazelbuild/rules_nodejs/commit/570e34d)), closes [#2078](https://github.com/bazelbuild/rules_nodejs/issues/2078)


### chore

* move karma_web_test to concatjs ([#2313](https://github.com/bazelbuild/rules_nodejs/issues/2313)) ([252b8e5](https://github.com/bazelbuild/rules_nodejs/commit/252b8e5))
* remove old stamping ([68b18d8](https://github.com/bazelbuild/rules_nodejs/commit/68b18d8)), closes [#2158](https://github.com/bazelbuild/rules_nodejs/issues/2158)


### Code Refactoring

* bazel_patch_module_resolver default to false ([fdde32f](https://github.com/bazelbuild/rules_nodejs/commit/fdde32f)), closes [#1440](https://github.com/bazelbuild/rules_nodejs/issues/1440) [#2125](https://github.com/bazelbuild/rules_nodejs/issues/2125)
* make pkg_web#move_files private ([815a3ca](https://github.com/bazelbuild/rules_nodejs/commit/815a3ca))


### Features

* **builtin:** flip the default of the strict_visibility flag on the npm and yarn install rules to True ([2c34857](https://github.com/bazelbuild/rules_nodejs/commit/2c34857))
* **concatjs:** ts_devserver -> concatjs_devserver; move to @bazel/concatjs ([baeae89](https://github.com/bazelbuild/rules_nodejs/commit/baeae89)), closes [#1082](https://github.com/bazelbuild/rules_nodejs/issues/1082)
* **cypress:** remove browiserify preprocessor ([98ee87d](https://github.com/bazelbuild/rules_nodejs/commit/98ee87d))
* **examples:** adds example for running jest with typescript ([#2245](https://github.com/bazelbuild/rules_nodejs/issues/2245)) ([d977c73](https://github.com/bazelbuild/rules_nodejs/commit/d977c73))
* **node_repositories:** Added auth option for downloading nodejs and yarn ([c89ff38](https://github.com/bazelbuild/rules_nodejs/commit/c89ff38))
* **typescript:** add allow_js support to ts_project ([91a95b8](https://github.com/bazelbuild/rules_nodejs/commit/91a95b8))
* **typescript:** worker mode for ts_project ([#2136](https://github.com/bazelbuild/rules_nodejs/issues/2136)) ([5d70997](https://github.com/bazelbuild/rules_nodejs/commit/5d70997))


### Performance Improvements

* **cypress:** pack cypress runfiles into a single tar ([e8484a0](https://github.com/bazelbuild/rules_nodejs/commit/e8484a0))


### BREAKING CHANGES

* By default, we no longer patch the require() function, instead you should rely on the linker to make node modules resolvable at the standard location
if this breaks you, the quickest fix is to flip the flag back on a nodejs_binary/nodejs_test/npm_package_bin with `templated_args = ["--bazel_patch_module_resolver"]`, see https://github.com/bazelbuild/rules_nodejs/pull/2344 as an example.
Another fix is to explicitly use our runfiles helper library, see https://github.com/bazelbuild/rules_nodejs/pull/2341 as an example.
* `packages/karma:package.bzl` is gone, in your WORKSPACE replace

```
load("//packages/karma:package.bzl", "npm_bazel_karma_dependencies")

npm_bazel_karma_dependencies()
```

with the equivalent

```
http_archive(
    name = "io_bazel_rules_webtesting",
    sha256 = "9bb461d5ef08e850025480bab185fd269242d4e533bca75bfb748001ceb343c3",
    urls = ["https://github.com/bazelbuild/rules_webtesting/releases/download/0.3.3/rules_webtesting.tar.gz"],
)
```

Then in BUILD files replace
`load("@npm//@bazel/karma:index.bzl", "karma_web_test_suite")`
with
`load("@npm//@bazel/concatjs:index.bzl", "concatjs_web_test_suite")`

finally drop npm dependencies on `@bazel/karma` and depend on `@bazel/concatjs` instead

* concatjs_web back to karma_web
* **typescript:** any ts_project rule that produces no outputs must be fixed or removed
* pkg_web#move_files helper is now a private API
* - rollup_bundle config_file no longer has substitutions from a "bazel_stamp_file" - use bazel_version_file instead
- pkg_npm no longer has replace_with_version attribute, use substitutions instead
* **concatjs:** users need to change their load statements for ts_devserver
* Users will need to rename `build_bazel_rules_typescript` to `npm_bazel_typescript` and `build_bazel_rules_karma` to `npm_bazel_karma` in their projects
* If you use the internal API of tsc_wrapped you need to update the CompilerHost constructor calls.



## [2.2.2](https://github.com/bazelbuild/rules_nodejs/compare/2.2.1...2.2.2) (2020-10-17)


### Bug Fixes

* **builtin:** js_library supports --output_groups=types ([c060a22](https://github.com/bazelbuild/rules_nodejs/commit/c060a22))
* **example:** remove compression dependencies ([75bf720](https://github.com/bazelbuild/rules_nodejs/commit/75bf720))
* **example:** remove index.html from prodapp srcs ([c7be89b](https://github.com/bazelbuild/rules_nodejs/commit/c7be89b))
* **example:** remove server side compression ([6d5aafb](https://github.com/bazelbuild/rules_nodejs/commit/6d5aafb))
* **exmaple:** add docstring to ngsw_config rule ([481fa21](https://github.com/bazelbuild/rules_nodejs/commit/481fa21))


### Features

* **example:** add full pwa support ([4d5b9c7](https://github.com/bazelbuild/rules_nodejs/commit/4d5b9c7))
* **example:** service worker update handling ([bb66235](https://github.com/bazelbuild/rules_nodejs/commit/bb66235))
* **karma:** use Trusted Types policy when loading scripts for Karma ([af9feb4](https://github.com/bazelbuild/rules_nodejs/commit/af9feb4))



## [2.2.1](https://github.com/bazelbuild/rules_nodejs/compare/2.2.0...2.2.1) (2020-10-07)


### Bug Fixes

* **builtin:** js_library: correctly propagate DeclarationInfos ([41f8719](https://github.com/bazelbuild/rules_nodejs/commit/41f8719))
* **examples:** prevent ibazel EOF ([96aea69](https://github.com/bazelbuild/rules_nodejs/commit/96aea69)), closes [#2143](https://github.com/bazelbuild/rules_nodejs/issues/2143)
* **karma:** allow custom browsers to specify args (fixes [#595](https://github.com/bazelbuild/rules_nodejs/issues/595)) ([5a58030](https://github.com/bazelbuild/rules_nodejs/commit/5a58030))
* don't glob yarn or node files when using vendored_node or vendored_yarn ([f5ef64f](https://github.com/bazelbuild/rules_nodejs/commit/f5ef64f))


### Features

* add strict_visibility to npm_install / yarn_install rules ([#2193](https://github.com/bazelbuild/rules_nodejs/issues/2193)) ([18c6e80](https://github.com/bazelbuild/rules_nodejs/commit/18c6e80)), closes [#2110](https://github.com/bazelbuild/rules_nodejs/issues/2110)
* update nodejs versions ([#2207](https://github.com/bazelbuild/rules_nodejs/issues/2207)) ([5478dab](https://github.com/bazelbuild/rules_nodejs/commit/5478dab))



# [2.2.0](https://github.com/bazelbuild/rules_nodejs/compare/2.1.0...2.2.0) (2020-09-10)


### Bug Fixes

* **builtin:** don't set --preserve-symlinks-main by default ([#2176](https://github.com/bazelbuild/rules_nodejs/issues/2176)) ([df18c61](https://github.com/bazelbuild/rules_nodejs/commit/df18c61))
* **builtin:** fix bazel coverage masking test failures ([3d0d1f7](https://github.com/bazelbuild/rules_nodejs/commit/3d0d1f7))
* **rollup:** allow config files to override default onwarn method ([0b80f6a](https://github.com/bazelbuild/rules_nodejs/commit/0b80f6a)), closes [#2084](https://github.com/bazelbuild/rules_nodejs/issues/2084)


### Features

* add link_workspace_root to nodejs_binary, npm_package_bin, rollup_bundle, terser_minified, ts_project ([4dcb37f](https://github.com/bazelbuild/rules_nodejs/commit/4dcb37f))
* link_workspace_root not needed in terser_minified ([c80b816](https://github.com/bazelbuild/rules_nodejs/commit/c80b816))
* promote js_library to public API ([1e357fd](https://github.com/bazelbuild/rules_nodejs/commit/1e357fd)), closes [#149](https://github.com/bazelbuild/rules_nodejs/issues/149) [#1771](https://github.com/bazelbuild/rules_nodejs/issues/1771)



# [2.1.0](https://github.com/bazelbuild/rules_nodejs/compare/2.0.3...2.1.0) (2020-09-07)


### Bug Fixes

* use golden_file_test instead ([1ef6704](https://github.com/bazelbuild/rules_nodejs/commit/1ef6704))
* **typescript:** add the tsBuildInfoFile option to ts_project ([#2138](https://github.com/bazelbuild/rules_nodejs/issues/2138)) ([16def64](https://github.com/bazelbuild/rules_nodejs/commit/16def64)), closes [#2137](https://github.com/bazelbuild/rules_nodejs/issues/2137)


### Features

* **builtin:** accept any stamp vars in pkg_npm ([01bfe4d](https://github.com/bazelbuild/rules_nodejs/commit/01bfe4d)), closes [#1694](https://github.com/bazelbuild/rules_nodejs/issues/1694)
* **builtin:** support for substitutions ([8a3f9b0](https://github.com/bazelbuild/rules_nodejs/commit/8a3f9b0))
* **typescript:** generate tsconfig.json for ts_project ([#2130](https://github.com/bazelbuild/rules_nodejs/issues/2130)) ([09ec233](https://github.com/bazelbuild/rules_nodejs/commit/09ec233)), closes [#2058](https://github.com/bazelbuild/rules_nodejs/issues/2058)



## [2.0.3](https://github.com/bazelbuild/rules_nodejs/compare/2.0.2...2.0.3) (2020-08-18)


### Bug Fixes

* **examples:** use ./ prefix on babel config file ([374f56f](https://github.com/bazelbuild/rules_nodejs/commit/374f56f))
* **typescript:** only expect .js outs for .tsx? srcs ([#2118](https://github.com/bazelbuild/rules_nodejs/issues/2118)) ([83688a1](https://github.com/bazelbuild/rules_nodejs/commit/83688a1)), closes [#2115](https://github.com/bazelbuild/rules_nodejs/issues/2115)
* **typescript:** produce .d.ts as default output rather than empty ([#2117](https://github.com/bazelbuild/rules_nodejs/issues/2117)) ([3d885e8](https://github.com/bazelbuild/rules_nodejs/commit/3d885e8)), closes [#2116](https://github.com/bazelbuild/rules_nodejs/issues/2116)


### Features

* **builtin:** new js_library rule ([#2109](https://github.com/bazelbuild/rules_nodejs/issues/2109)) ([4fe1a17](https://github.com/bazelbuild/rules_nodejs/commit/4fe1a17))
* **example:** add targets in angular_bazel_architect for production serve and build ([746a6f8](https://github.com/bazelbuild/rules_nodejs/commit/746a6f8))



## [2.0.2](https://github.com/bazelbuild/rules_nodejs/compare/2.0.1...2.0.2) (2020-08-10)


### Bug Fixes

* **cypress:** allow for async cypress plugins ([4fd4653](https://github.com/bazelbuild/rules_nodejs/commit/4fd4653))
* coverage ([#2100](https://github.com/bazelbuild/rules_nodejs/issues/2100)) ([e5fc274](https://github.com/bazelbuild/rules_nodejs/commit/e5fc274))
* remove duplicate Importing ([23f80cf](https://github.com/bazelbuild/rules_nodejs/commit/23f80cf))
* test file pattern ([#2089](https://github.com/bazelbuild/rules_nodejs/issues/2089)) ([857471e](https://github.com/bazelbuild/rules_nodejs/commit/857471e))



## [2.0.1](https://github.com/bazelbuild/rules_nodejs/compare/2.0.0...2.0.1) (2020-07-24)


### Bug Fixes

* **typescript:** ts_library should accept .tsx inputs ([065922b](https://github.com/bazelbuild/rules_nodejs/commit/065922b))



# [2.0.0](https://github.com/bazelbuild/rules_nodejs/compare/2.0.0-rc.3...2.0.0) (2020-07-20)


### Bug Fixes

* **typescript:** exclude package.json from tsconfig#files ([16cbc6f](https://github.com/bazelbuild/rules_nodejs/commit/16cbc6f))
* **typescript:** include package.json in third-party DeclarationInfo ([1c70656](https://github.com/bazelbuild/rules_nodejs/commit/1c70656)), closes [#2044](https://github.com/bazelbuild/rules_nodejs/issues/2044)


### Features

* **typescript:** support for declarationdir on ts_project  ([#2048](https://github.com/bazelbuild/rules_nodejs/issues/2048)) ([981e7c1](https://github.com/bazelbuild/rules_nodejs/commit/981e7c1))



# [2.0.0-rc.3](https://github.com/bazelbuild/rules_nodejs/compare/2.0.0-rc.2...2.0.0-rc.3) (2020-07-17)


### Bug Fixes

* **builtin:** linker fix for when not running in execroot ([b187d50](https://github.com/bazelbuild/rules_nodejs/commit/b187d50))
* **builtin:** perform the ts-to-js entry_point rewrite ([8cc044f](https://github.com/bazelbuild/rules_nodejs/commit/8cc044f))


### chore

* remove ts_setup_workspace ([07d9bb8](https://github.com/bazelbuild/rules_nodejs/commit/07d9bb8)), closes [/github.com/bazelbuild/rules_nodejs/pull/1159/files#diff-fe375cd73fb89504b9b9a9a751518849](https://github.com//github.com/bazelbuild/rules_nodejs/pull/1159/files/issues/diff-fe375cd73fb89504b9b9a9a751518849) [#2033](https://github.com/bazelbuild/rules_nodejs/issues/2033)


### Features

* **examples:** add a vanilla cra app ([b7bdab7](https://github.com/bazelbuild/rules_nodejs/commit/b7bdab7))
* **examples:** convert create-react-app example to bazel run ([a8ff872](https://github.com/bazelbuild/rules_nodejs/commit/a8ff872))
* **examples:** convert create-react-app example to bazel test ([146e522](https://github.com/bazelbuild/rules_nodejs/commit/146e522))
* **examples:** show the create-react-app converted to bazel build ([52455e0](https://github.com/bazelbuild/rules_nodejs/commit/52455e0))
* **typescript:** support for rootdir on ts_project ([bc88536](https://github.com/bazelbuild/rules_nodejs/commit/bc88536))
* add depset support to run_node inputs, matching ctx.action.run ([ee584f8](https://github.com/bazelbuild/rules_nodejs/commit/ee584f8))


### BREAKING CHANGES

* ts_setup_workspace was a no-op and has been removed. Simply remove it from your WORKSPACE file.



# [2.0.0-rc.2](https://github.com/bazelbuild/rules_nodejs/compare/2.0.0-rc.1...2.0.0-rc.2) (2020-07-10)


### Bug Fixes

* **builtin:** fix node patches subprocess sandbox propogation ([#2017](https://github.com/bazelbuild/rules_nodejs/issues/2017)) ([0bd9b7e](https://github.com/bazelbuild/rules_nodejs/commit/0bd9b7e))



# [2.0.0-rc.1](https://github.com/bazelbuild/rules_nodejs/compare/2.0.0-rc.0...2.0.0-rc.1) (2020-07-06)


### Bug Fixes

* **builtin:** fix linker bug when there are no third-party modules ([becd9bc](https://github.com/bazelbuild/rules_nodejs/commit/becd9bc))
* **builtin:** fixes nodejs_binary to collect JSNamedModuleInfo ([4f95cc4](https://github.com/bazelbuild/rules_nodejs/commit/4f95cc4)), closes [#1998](https://github.com/bazelbuild/rules_nodejs/issues/1998)
* **builtin:** linker silently not generating expected links in windows ([2979fad](https://github.com/bazelbuild/rules_nodejs/commit/2979fad))
* **typescript:** add .proto files from npm deps to inputs of ts_library ([#1991](https://github.com/bazelbuild/rules_nodejs/issues/1991)) ([c1d4885](https://github.com/bazelbuild/rules_nodejs/commit/c1d4885))
* **typescript:** add json to ts_project DefaultInfo, fix [#1988](https://github.com/bazelbuild/rules_nodejs/issues/1988) ([f6fa264](https://github.com/bazelbuild/rules_nodejs/commit/f6fa264))
* **typescript:** Exclude .json from _out_paths ([91d81b3](https://github.com/bazelbuild/rules_nodejs/commit/91d81b3))
* allow multiple run_node calls to be made from the same rule context ([48bb9cc](https://github.com/bazelbuild/rules_nodejs/commit/48bb9cc))


### Features

* add support for capturing and overriding the exit code within run_node ([#1990](https://github.com/bazelbuild/rules_nodejs/issues/1990)) ([cbdd3b0](https://github.com/bazelbuild/rules_nodejs/commit/cbdd3b0))
* **cypress:** add cypress_web_test rule and @bazel/cypress package ([3bac870](https://github.com/bazelbuild/rules_nodejs/commit/3bac870))
* **typescript:** add OutputGroupInfo to ts_project with type definitions ([d660ca1](https://github.com/bazelbuild/rules_nodejs/commit/d660ca1)), closes [#1978](https://github.com/bazelbuild/rules_nodejs/issues/1978)



# [2.0.0-rc.0](https://github.com/bazelbuild/rules_nodejs/compare/1.6.0...2.0.0-rc.0) (2020-06-23)


### Bug Fixes

* **builtin:** fix linker common path reduction bug where reduced path conflicts with node_modules ([65d6029](https://github.com/bazelbuild/rules_nodejs/commit/65d6029))
* **builtin:** fix linker issue when running test with "local" tag on osx & linux ([#1835](https://github.com/bazelbuild/rules_nodejs/issues/1835)) ([98d3321](https://github.com/bazelbuild/rules_nodejs/commit/98d3321))
* **builtin:** fix regression in 1.6.0 in linker linking root package when under runfiles ([b4149d8](https://github.com/bazelbuild/rules_nodejs/commit/b4149d8)), closes [#1823](https://github.com/bazelbuild/rules_nodejs/issues/1823) [#1850](https://github.com/bazelbuild/rules_nodejs/issues/1850)
* **builtin:** linker no longer makes node_modules symlink to the root of the workspace output tree ([044495c](https://github.com/bazelbuild/rules_nodejs/commit/044495c))
* **builtin:** rerun yarn_install and npm_install when node version changes ([8c1e035](https://github.com/bazelbuild/rules_nodejs/commit/8c1e035))
* **builtin:** scrub node-patches VERBOSE_LOGS when asserting on stderr ([45f9443](https://github.com/bazelbuild/rules_nodejs/commit/45f9443))
* **labs:** handle const/let syntax in generated protoc js ([96a0690](https://github.com/bazelbuild/rules_nodejs/commit/96a0690))
* **labs:** make grpc service files tree shakable ([a3bd81b](https://github.com/bazelbuild/rules_nodejs/commit/a3bd81b))
* don't expose an npm dependency from builtin ([7b2b4cf](https://github.com/bazelbuild/rules_nodejs/commit/7b2b4cf))
* **terser:** allow fallback binary resolution ([3ffb3b1](https://github.com/bazelbuild/rules_nodejs/commit/3ffb3b1))


### chore

* remove hide-build-files package ([5d1d006](https://github.com/bazelbuild/rules_nodejs/commit/5d1d006)), closes [#1613](https://github.com/bazelbuild/rules_nodejs/issues/1613)


### Code Refactoring

* remove install_source_map_support from nodejs_binary since it is vendored in ([72f19e7](https://github.com/bazelbuild/rules_nodejs/commit/72f19e7))


### Features

* add JSModuleInfo provider ([d3fcf85](https://github.com/bazelbuild/rules_nodejs/commit/d3fcf85))
* **angular:** introduce an Angular CLI builder ([c87c83f](https://github.com/bazelbuild/rules_nodejs/commit/c87c83f))
* **jasmine:** make jasmine a peerDep ([e6890fc](https://github.com/bazelbuild/rules_nodejs/commit/e6890fc))
* add stdout capture to npm_package_bin ([3f182f0](https://github.com/bazelbuild/rules_nodejs/commit/3f182f0))
* **builtin:** add DeclarationInfo to js_library ([2b89f32](https://github.com/bazelbuild/rules_nodejs/commit/2b89f32))
* introduce generated_file_test ([3fbf2c0](https://github.com/bazelbuild/rules_nodejs/commit/3fbf2c0)), closes [#1893](https://github.com/bazelbuild/rules_nodejs/issues/1893)
* **builtin:** enable coverage on nodejs_test ([2059ea9](https://github.com/bazelbuild/rules_nodejs/commit/2059ea9))
* **builtin:** use linker for all generated :bin targets ([007a8f6](https://github.com/bazelbuild/rules_nodejs/commit/007a8f6))
* **examples:** show how to use ts_library(use_angular_plugin) with worker mode ([#1839](https://github.com/bazelbuild/rules_nodejs/issues/1839)) ([a167311](https://github.com/bazelbuild/rules_nodejs/commit/a167311))
* **examples:** upgrade rules_docker to 0.14.1 ([ad2eba1](https://github.com/bazelbuild/rules_nodejs/commit/ad2eba1))
* **rollup:** update the peerDependencies version range to >=2.3.0 <3.0.0 ([e05f5be](https://github.com/bazelbuild/rules_nodejs/commit/e05f5be))
* **typescript:** add outdir to ts_project ([3942fd9](https://github.com/bazelbuild/rules_nodejs/commit/3942fd9))
* **typescript:** include label in the ts_project progress message ([#1944](https://github.com/bazelbuild/rules_nodejs/issues/1944)) ([76e8bd1](https://github.com/bazelbuild/rules_nodejs/commit/76e8bd1)), closes [#1927](https://github.com/bazelbuild/rules_nodejs/issues/1927)
* support bazel+js packages that install into regular @npm//package:index.bzl location ([4f508b1](https://github.com/bazelbuild/rules_nodejs/commit/4f508b1))


### BREAKING CHANGES

* Adds JSModuleInfo provider as the common provider for passing & consuming javascript sources and related files such as .js.map, .json, etc.

For 1.0 we added JSNamedModuleInfo and JSEcmaScriptModuleInfo which were provided by ts_library and consumed by rules that needed to differentiate between the two default flavors of ts_library outputs (named-UMD & esm). We left out JSModuleInfo as its use case was unclear at the time.

For 2.0 we're adding JSModuleInfo as generic javascript provided for the rules_nodejs ecosystem. It is not currently opinionated about the module format of the sources or the language level. Consumers of JSModuleInfo should be aware of what module format & language level is being produced if necessary.

The following rules provide JSModuleInfo:

* ts_library (devmode named-UMD .js output flavor)
* ts_proto_library (devmode named-UMD .js output flavor)
* node_module_library (this is a behind the scenes rule used by yarn_install & npm_install)
* js_library (.js, .js.map & . json files)
* rollup_bundle
* terser_minfied
* ts_project

The following rules consume JSModuleInfo:

* nodejs_binary & nodejs_test (along with derivate macros such as jasmine_node_test); these rules no longer consume JSNamedModuleInfo
* npm_package_bin
* pkg_npm; no longer consumes JSNamedModuleInfo
* karma_web_test (for config file instead of JSNamedModuleInfo; JSNamedModuleInfo still used for test files)
* protractor_web_test (for config & on_prepare files instead of JSModuleInfo; JSNamedModuleInfo still used for test files)
* rollup_bundle (if JSEcmaScriptModuleInfo not provided)
* terser_minified
* **builtin:** Any nodejs_binary/nodejs_test processes with the linker enabled (--nobazel_patch_module_resolver is set) that were relying on standard node_module resolution to resolve manfest file paths such as `my_workspace/path/to/output/file.js` must now use the runfiles helper such as.

Previously:
```
const absPath = require.resolve('my_workspace/path/to/output/file.js');
```
With runfiles helper:
```
const runfiles = require(process.env['BAZEL_NODE_RUNFILES_HELPER']);
const absPath = runfiles.resolve('my_workspace/path/to/output/file.js');
```
* **builtin:** Removed provide_declarations() factory function for DeclarationInfo. Use declaration_info() factory function instead.
* `install_source_map_support` attribute removed from `nodejs_binary`. `source-map-support` is vendored in at `/third_party/github.com/source-map-support` so it can always be installed.
* **builtin:** jasmine_node_test not longer has the `coverage`
attribute
* rules_nodejs now requires Bazel 2.1 or greater.
Also the hide_build_files attribute was removed from pkg_npm, and always_hide_bazel_files was removed from yarn_install and npm_install. These are no longer needed since 1.3.0
* **builtin:** If you use the generated nodejs_binary or nodejs_test rules in the npm
workspace, for example @npm//typescript/bin:tsc, your custom rule must now link the
node_modules directory into that process. A typical way to do this is
with the run_node helper. See updates to examples in this commit.



# [1.6.0](https://github.com/bazelbuild/rules_nodejs/compare/1.5.0...1.6.0) (2020-04-11)


### Features

* **builtin:** export version to npm/yarn install ([011278e](https://github.com/bazelbuild/rules_nodejs/commit/011278e))
* **jasmine:** check pkg version to rules_nodejs ([22bebbc](https://github.com/bazelbuild/rules_nodejs/commit/22bebbc))
* **typescript:** wire up use_angular_plugin attribute ([520493d](https://github.com/bazelbuild/rules_nodejs/commit/520493d))


### Bug Fixes

* **builtin:** always symlink node_modules at `execroot/my_wksp/node_modules` even when running in runfiles ([#1805](https://github.com/bazelbuild/rules_nodejs/issues/1805)) ([5c2f6c1](https://github.com/bazelbuild/rules_nodejs/commit/5c2f6c1))
* **builtin:** don't allow symlinks to escape or enter bazel managed node_module folders ([#1800](https://github.com/bazelbuild/rules_nodejs/issues/1800)) ([4554ce7](https://github.com/bazelbuild/rules_nodejs/commit/4554ce7))
* **builtin:** fix for pkg_npm single directory artifact dep case ([5a7c1a7](https://github.com/bazelbuild/rules_nodejs/commit/5a7c1a7))
* **builtin:** fix node patches lstat short-circuit logic ([#1818](https://github.com/bazelbuild/rules_nodejs/issues/1818)) ([b0627be](https://github.com/bazelbuild/rules_nodejs/commit/b0627be))
* **builtin:** fix npm_version_check.js when running outside of bazel ([#1802](https://github.com/bazelbuild/rules_nodejs/issues/1802)) ([afabe89](https://github.com/bazelbuild/rules_nodejs/commit/afabe89))
* **builtin:** look in the execroot for nodejs_binary source entry_points ([#1816](https://github.com/bazelbuild/rules_nodejs/issues/1816)) ([b84d65e](https://github.com/bazelbuild/rules_nodejs/commit/b84d65e)), closes [#1787](https://github.com/bazelbuild/rules_nodejs/issues/1787) [#1787](https://github.com/bazelbuild/rules_nodejs/issues/1787)
* **builtin:** preserve lone $ in templated_args for legacy support ([#1772](https://github.com/bazelbuild/rules_nodejs/issues/1772)) ([72c14d8](https://github.com/bazelbuild/rules_nodejs/commit/72c14d8))
* **builtin:** under runfiles linker should link node_modules folder at root of runfiles tree ([13510ad](https://github.com/bazelbuild/rules_nodejs/commit/13510ad))
* **rollup:** fix worker not picking up config file changes ([a19eb2b](https://github.com/bazelbuild/rules_nodejs/commit/a19eb2b)), closes [#1790](https://github.com/bazelbuild/rules_nodejs/issues/1790)
* **typescript:** don't mix worker mode and linker ([55c6c4a](https://github.com/bazelbuild/rules_nodejs/commit/55c6c4a)), closes [#1803](https://github.com/bazelbuild/rules_nodejs/issues/1803) [#1803](https://github.com/bazelbuild/rules_nodejs/issues/1803)
* **typescript:** include extended tsconfigs in _TsConfigInfo ([cd8520d](https://github.com/bazelbuild/rules_nodejs/commit/cd8520d)), closes [#1754](https://github.com/bazelbuild/rules_nodejs/issues/1754)


### Examples

* **examples:** add support for server side rendering with universal ([c09ca89](https://github.com/bazelbuild/rules_nodejs/commit/c09ca89))
* **examples:** build and consume an Angular workspace library  ([#1633](https://github.com/bazelbuild/rules_nodejs/issues/1633)) ([b459d6d](https://github.com/bazelbuild/rules_nodejs/commit/b459d6d))


### Documentation

* **docs:** `yarn_urls` should be `string_list`, not `string` ([3357c08](https://github.com/bazelbuild/rules_nodejs/commit/3357c08))


# [1.5.0](https://github.com/bazelbuild/rules_nodejs/compare/1.4.1...1.5.0) (2020-03-28)


### Bug Fixes

* **builtin:** entry point of a .tsx file is .js ([#1732](https://github.com/bazelbuild/rules_nodejs/issues/1732)) ([24607ed](https://github.com/bazelbuild/rules_nodejs/commit/24607ed)), closes [#1730](https://github.com/bazelbuild/rules_nodejs/issues/1730)
* **builtin:** fix for nodejs_binary entry point in bazel-out logic ([#1739](https://github.com/bazelbuild/rules_nodejs/issues/1739)) ([a6e29c2](https://github.com/bazelbuild/rules_nodejs/commit/a6e29c2)) ([863c7de](https://github.com/bazelbuild/rules_nodejs/commit/863c7de))
closes [#1606](https://github.com/bazelbuild/rules_nodejs/issues/1606)
* **jasmine:** user templated_args should be passed to jasmine after 3 internal templated_args ([#1743](https://github.com/bazelbuild/rules_nodejs/issues/1743)) ([baa68c1](https://github.com/bazelbuild/rules_nodejs/commit/baa68c1))
* **typescript:** fix ts_library to allow deps with module_name but no module_root attrs ([#1738](https://github.com/bazelbuild/rules_nodejs/issues/1738)) ([0b5ad2a](https://github.com/bazelbuild/rules_nodejs/commit/0b5ad2a))
* **typescript:** pass rootDir to ts_project tsc actions ([#1748](https://github.com/bazelbuild/rules_nodejs/issues/1748)) ([13caf8b](https://github.com/bazelbuild/rules_nodejs/commit/13caf8b))


### Features

* **builtin:** add LinkablePackageInfo to pkg_npm, js_library & ts_library ([1023852](https://github.com/bazelbuild/rules_nodejs/commit/1023852))
* **builtin:** add support for predefined variables and custom variable to params_file ([34b8cf4](https://github.com/bazelbuild/rules_nodejs/commit/34b8cf4))
* **builtin:** support $(rootpath), $(execpath), predefined & custom variables in templated_args ([5358d56](https://github.com/bazelbuild/rules_nodejs/commit/5358d56))
* **labs:** introduce a new ts_proto_library with grpc support ([8b43896](https://github.com/bazelbuild/rules_nodejs/commit/8b43896))
* **rollup:** add worker support to rollup_bundle ([66db579](https://github.com/bazelbuild/rules_nodejs/commit/66db579))
* **typescript:** add devmode_target, devmode_module, prodmode_target & prodmode_module attributes ([#1687](https://github.com/bazelbuild/rules_nodejs/issues/1687)) ([1a83a7f](https://github.com/bazelbuild/rules_nodejs/commit/1a83a7f))
* **typescript:** add ts_project rule ([#1710](https://github.com/bazelbuild/rules_nodejs/issues/1710)) ([26f6698](https://github.com/bazelbuild/rules_nodejs/commit/26f6698))


### Examples

* **examples:** fix angular examples prod serve doesn't work on windows ([#1699](https://github.com/bazelbuild/rules_nodejs/issues/1699)) ([063fb13](https://github.com/bazelbuild/rules_nodejs/commit/063fb13)), 


### Documentation

* **docs:** invalid link of examples ([#1728](https://github.com/bazelbuild/rules_nodejs/issues/1728)) ([7afaa48](https://github.com/bazelbuild/rules_nodejs/commit/7afaa48))
* **docs:** syntax error in example code ([#1731](https://github.com/bazelbuild/rules_nodejs/issues/1731)) ([51785e5](https://github.com/bazelbuild/rules_nodejs/commit/51785e5))
* **docs:** invalid link in index ([b47cc74](https://github.com/bazelbuild/rules_nodejs/commit/b47cc74))

## [1.4.1](https://github.com/bazelbuild/rules_nodejs/compare/1.4.0...1.4.1) (2020-03-06)

### Bug Fixes

* **builtin:** Bazel build failing when project is not on the system drive on Windows (C:) ([#1641](https://github.com/bazelbuild/rules_nodejs/issues/1641)) ([d9cbb99f](https://github.com/bazelbuild/rules_nodejs/commit/d9cbb99f)
* **windows_utils:** Escaping \ and " before passing args to bash scrip… ([#1685](https://github.com/bazelbuild/rules_nodejs/pull/1685)) ([f9be953d](https://github.com/bazelbuild/rules_nodejs/commit/f9be953d)


# [1.4.0](https://github.com/bazelbuild/rules_nodejs/compare/1.3.0...1.4.0) (2020-03-02)


### Bug Fixes

* **builtin:** don't include external files when pkg_npm is in root package ([#1677](https://github.com/bazelbuild/rules_nodejs/issues/1677)) ([8089999](https://github.com/bazelbuild/rules_nodejs/commit/8089999)), closes [#1499](https://github.com/bazelbuild/rules_nodejs/issues/1499)
* **examples:** change build target label to //src:prodapp ([a7f07d1](https://github.com/bazelbuild/rules_nodejs/commit/a7f07d1))
* **examples:** fix angular examples to use bazelisk ([02e6462](https://github.com/bazelbuild/rules_nodejs/commit/02e6462))
* ensure BAZEL_NODE_RUNFILES_HELPER & BAZEL_NODE_PATCH_REQUIRE are absolute ([#1634](https://github.com/bazelbuild/rules_nodejs/issues/1634)) ([25600ea](https://github.com/bazelbuild/rules_nodejs/commit/25600ea))
* expand_variables helper should handle external labels ([3af3a0d](https://github.com/bazelbuild/rules_nodejs/commit/3af3a0d))
* logic error in expand_variables ([#1631](https://github.com/bazelbuild/rules_nodejs/issues/1631)) ([32c003f](https://github.com/bazelbuild/rules_nodejs/commit/32c003f))
* yarn cache path should be a string ([#1679](https://github.com/bazelbuild/rules_nodejs/issues/1679)) ([a43809b](https://github.com/bazelbuild/rules_nodejs/commit/a43809b))
* **builtin:** use posix paths in assembler ([d635dca](https://github.com/bazelbuild/rules_nodejs/commit/d635dca)), closes [#1635](https://github.com/bazelbuild/rules_nodejs/issues/1635)
* **create:** use latest typescript ([a8ba18e](https://github.com/bazelbuild/rules_nodejs/commit/a8ba18e)), closes [#1602](https://github.com/bazelbuild/rules_nodejs/issues/1602)
* **examples:** add fixes to angular architect ([f6f40c3](https://github.com/bazelbuild/rules_nodejs/commit/f6f40c3))
* remove empty arguments from launcher ([#1650](https://github.com/bazelbuild/rules_nodejs/issues/1650)) ([aa3cd6c](https://github.com/bazelbuild/rules_nodejs/commit/aa3cd6c))


### Features

* **@bazel/jasmine:** update dependencies to jasmine v3.5.0 ([98fab93](https://github.com/bazelbuild/rules_nodejs/commit/98fab93))
* **docs:** add authroing instructions ([4dde728](https://github.com/bazelbuild/rules_nodejs/commit/4dde728))
* **docs:** add header anchor links ([2002046](https://github.com/bazelbuild/rules_nodejs/commit/2002046))
* **docs:** add vscode debugging section ([78d308f](https://github.com/bazelbuild/rules_nodejs/commit/78d308f))
* **examples:** add serve to angular architect ([1569f4b](https://github.com/bazelbuild/rules_nodejs/commit/1569f4b))
* **jasmine:** configure XML reporter to capture detailed testlogs ([8abd20d](https://github.com/bazelbuild/rules_nodejs/commit/8abd20d))
* **rollup:** add `args` attribute to rollup_bundle rule ([#1681](https://github.com/bazelbuild/rules_nodejs/issues/1681)) ([94c6182](https://github.com/bazelbuild/rules_nodejs/commit/94c6182))
* **rollup:** add silent attr to rollup_bundle to support --silent flag ([#1680](https://github.com/bazelbuild/rules_nodejs/issues/1680)) ([18e8001](https://github.com/bazelbuild/rules_nodejs/commit/18e8001))
* **typescript:** use run_node helper to execute tsc ([066a52c](https://github.com/bazelbuild/rules_nodejs/commit/066a52c))



# [1.3.0](https://github.com/bazelbuild/rules_nodejs/compare/1.2.4...1.3.0) (2020-02-07)


### Bug Fixes

* **builtin:** strip leading v prefix from stamp ([#1591](https://github.com/bazelbuild/rules_nodejs/issues/1591)) ([39bb821](https://github.com/bazelbuild/rules_nodejs/commit/39bb821))
* angular example ts_scripts path in Windows ([30d0f37](https://github.com/bazelbuild/rules_nodejs/commit/30d0f37)), closes [#1604](https://github.com/bazelbuild/rules_nodejs/issues/1604)
* html script injection is broken on windows ([7f7a45b](https://github.com/bazelbuild/rules_nodejs/commit/7f7a45b)), closes [#1604](https://github.com/bazelbuild/rules_nodejs/issues/1604)
* unset YARN_IGNORE_PATH before calling yarn in [@nodejs](https://github.com/nodejs) targets ([aee3003](https://github.com/bazelbuild/rules_nodejs/commit/aee3003)), closes [#1588](https://github.com/bazelbuild/rules_nodejs/issues/1588)


### Features

* **builtin:** add environment attribute to yarn_install & npm_install ([#1596](https://github.com/bazelbuild/rules_nodejs/issues/1596)) ([87b2a64](https://github.com/bazelbuild/rules_nodejs/commit/87b2a64))
* **builtin:** expose `@npm//foo__all_files` filegroup that includes all files in the npm package ([#1600](https://github.com/bazelbuild/rules_nodejs/issues/1600)) ([8d77827](https://github.com/bazelbuild/rules_nodejs/commit/8d77827))
* **examples:** add protractor angular architect ([#1594](https://github.com/bazelbuild/rules_nodejs/issues/1594)) ([d420019](https://github.com/bazelbuild/rules_nodejs/commit/d420019))



## [1.2.4](https://github.com/bazelbuild/rules_nodejs/compare/1.2.2...1.2.4) (2020-01-31)


### Bug Fixes

* **builtin:** fix logic error in linker conflict resolution ([#1597](https://github.com/bazelbuild/rules_nodejs/issues/1597)) ([b864223](https://github.com/bazelbuild/rules_nodejs/commit/b864223))



## [1.2.2](https://github.com/bazelbuild/rules_nodejs/compare/1.2.1...1.2.2) (2020-01-31)


### Bug Fixes

* unset YARN_IGNORE_PATH in yarn_install before calling yarn ([5a2af71](https://github.com/bazelbuild/rules_nodejs/commit/5a2af71))
* fixes bazelbuild/rules_nodejs#1567 Recursively copy files from subdirectories into mirrored structure in the npm archive ([c83b026](https://github.com/bazelbuild/rules_nodejs/commit/c83b026))


### Code Refactoring

* Replace grep with bash's regex operator ([9fb080b](https://github.com/bazelbuild/rules_nodejs/commit/9fb080b))


### Examples

* enable test file crawling for jest example ([8854bfd](https://github.com/bazelbuild/rules_nodejs/commit/8854bfd))
* add angular bazel architect ([6dc919d](https://github.com/bazelbuild/rules_nodejs/commit/6dc919d))



## [1.2.1](https://github.com/bazelbuild/rules_nodejs/compare/1.2.0...1.2.1) (2020-01-30)


### Bug Fixes

* allow "src" and "bin" module mappings to win over "runfiles" ([110e00e](https://github.com/bazelbuild/rules_nodejs/commit/110e00e))
* also link "runfiles" mappings from *_test rules ([79bedc5](https://github.com/bazelbuild/rules_nodejs/commit/79bedc5))
* osx hide-bazel-files issue with fsevents ([#1578](https://github.com/bazelbuild/rules_nodejs/issues/1578)) ([64a31ab](https://github.com/bazelbuild/rules_nodejs/commit/64a31ab))
* yarn_install failure if yarn is a dependency ([#1581](https://github.com/bazelbuild/rules_nodejs/issues/1581)) ([f712377](https://github.com/bazelbuild/rules_nodejs/commit/f712377))



# [1.2.0](https://github.com/bazelbuild/rules_nodejs/compare/1.1.0...1.2.0) (2020-01-24)


### Bug Fixes

* **builtin:** legacy module_mappings_runtime_aspect handles dep with module_name but no module_root ([9ac0534](https://github.com/bazelbuild/rules_nodejs/commit/9ac0534))
* **builtin:** nodejs_binary collects module_mappings for linker ([4419f95](https://github.com/bazelbuild/rules_nodejs/commit/4419f95))
* **builtin:** set cwd before running yarn for yarn_install ([#1569](https://github.com/bazelbuild/rules_nodejs/issues/1569)) ([d7083ac](https://github.com/bazelbuild/rules_nodejs/commit/d7083ac))


### Features

* **builtin:** add configuration_env_vars to npm_package_bin ([07d9f5d](https://github.com/bazelbuild/rules_nodejs/commit/07d9f5d))



# [1.1.0](https://github.com/bazelbuild/rules_nodejs/compare/1.0.1...1.1.0) (2020-01-12)


### Bug Fixes

* separate nodejs require patches from loader and —require them first ([b10d230](https://github.com/bazelbuild/rules_nodejs/commit/b10d230))
* **karma:** pass --node_options to karma ([d48f237](https://github.com/bazelbuild/rules_nodejs/commit/d48f237))
* **protractor:** pass --node_options to protractor ([a3b39ab](https://github.com/bazelbuild/rules_nodejs/commit/a3b39ab))


### Features

* **builtin:** add support for Predefined variables and Custom variable to npm_package_bin ([34176e5](https://github.com/bazelbuild/rules_nodejs/commit/34176e5))
* **examples:** add nestjs test ([f448931](https://github.com/bazelbuild/rules_nodejs/commit/f448931))
* **examples:** add nodejs_binary cluster example ([#1515](https://github.com/bazelbuild/rules_nodejs/issues/1515)) ([f217519](https://github.com/bazelbuild/rules_nodejs/commit/f217519))



## [1.0.1](https://github.com/bazelbuild/rules_nodejs/compare/1.0.0...1.0.1) (2020-01-03)


### Bug Fixes

* don't bake COMPILATION_MODE into launcher as exported environment var ([8a931d8](https://github.com/bazelbuild/rules_nodejs/commit/8a931d8))
* **builtin:** make .pack and .publish targets work again ([43716d3](https://github.com/bazelbuild/rules_nodejs/commit/43716d3)), closes [#1493](https://github.com/bazelbuild/rules_nodejs/issues/1493)
* **create:** @bazel/create should verbose log based on VERBOSE_LOGS instead of COMPILATION_MODE ([c1b97d6](https://github.com/bazelbuild/rules_nodejs/commit/c1b97d6))


### Features

* **builtin:** allow patching require in bootstrap scripts ([842dfb4](https://github.com/bazelbuild/rules_nodejs/commit/842dfb4))



# [1.0.0](https://github.com/bazelbuild/rules_nodejs/compare/0.42.3...1.0.0) (2019-12-20)


### Bug Fixes

* **builtin:** bin folder was included in runfiles path for tests when link type was 'bin' ([f938ab7](https://github.com/bazelbuild/rules_nodejs/commit/f938ab7))
* **builtin:** link module_name to directories recursively to avoid directory clashes ([#1432](https://github.com/bazelbuild/rules_nodejs/issues/1432)) ([0217724](https://github.com/bazelbuild/rules_nodejs/commit/0217724)), closes [#1411](https://github.com/bazelbuild/rules_nodejs/issues/1411)
* **builtin:** strip BOM when parsing package.json ([#1453](https://github.com/bazelbuild/rules_nodejs/issues/1453)) ([c65d9b7](https://github.com/bazelbuild/rules_nodejs/commit/c65d9b7)), closes [#1448](https://github.com/bazelbuild/rules_nodejs/issues/1448)
* **typescript:** remove stray references to ts_auto_deps ([#1449](https://github.com/bazelbuild/rules_nodejs/issues/1449)) ([aacd924](https://github.com/bazelbuild/rules_nodejs/commit/aacd924))


### chore

* make defs.bzl error ([3339d46](https://github.com/bazelbuild/rules_nodejs/commit/3339d46)), closes [#1068](https://github.com/bazelbuild/rules_nodejs/issues/1068)


### Code Refactoring

* pkg_npm attributes renames packages=>nested_packages & replacements=>substitutions ([7e1b7df](https://github.com/bazelbuild/rules_nodejs/commit/7e1b7df))
* remove `bootstrap` attribute & fix $(location) expansions in nodejs_binary templated_args ([1860a6a](https://github.com/bazelbuild/rules_nodejs/commit/1860a6a))
* remove templated_args_file from nodejs_binary & nodejs_test ([799acb4](https://github.com/bazelbuild/rules_nodejs/commit/799acb4))
* **builtin:** add `args` to yarn_install & npm_install ([#1462](https://github.com/bazelbuild/rules_nodejs/issues/1462)) ([d245d09](https://github.com/bazelbuild/rules_nodejs/commit/d245d09))
* **builtin:** remove legacy jasmine_node_test ([6d731cf](https://github.com/bazelbuild/rules_nodejs/commit/6d731cf))
* **builtin:** renamed npm_package to pkg_npm to match naming convention ([7df4109](https://github.com/bazelbuild/rules_nodejs/commit/7df4109))
* pre-1.0 release breaking changes ([cc64818](https://github.com/bazelbuild/rules_nodejs/commit/cc64818))
* remove unused exclude_packages from npm_install & yarn_install ([f50dea3](https://github.com/bazelbuild/rules_nodejs/commit/f50dea3))


### Features

* **builtin:** introduce copy_to_bin rule ([#1450](https://github.com/bazelbuild/rules_nodejs/issues/1450)) ([f19245b](https://github.com/bazelbuild/rules_nodejs/commit/f19245b))


### Performance Improvements

* avoid unnecessary nested depset() ([#1435](https://github.com/bazelbuild/rules_nodejs/issues/1435)) ([f386322](https://github.com/bazelbuild/rules_nodejs/commit/f386322))


### BREAKING CHANGES

* `templated_args_file` removed from nodejs_binary, nodejs_test & jasmine_node_test. This was a separation of concerns and complicated node.bzl more than necessary while also being rigid in how the params file is formatted. It is more flexible to expose this functionality as another simple rule named params_file.

To match standard $(location) and $(locations) expansion, params_file args location expansions are also in the standard short_path form (this differs from the old templated_args behavior which was not Bazel idiomatic)
Usage example:

```
load("@build_bazel_rules_nodejs//:index.bzl", "params_file", "nodejs_binary")

params_file(
    name = "params_file",
    args = [
        "--some_param",
        "$(location //path/to/some:file)",
        "--some_other_param",
        "$(location //path/to/some/other:file)",
    ],
    data = [
        "//path/to/some:file",
        "//path/to/some/other:file",
    ],
)

nodejs_binary(
    name = "my_binary",
    data = [":params_file"],
    entry_point = ":my_binary.js",
    templated_args = ["$(location :params_file)"],
)
```
* bootstrap attribute in nodejs_binary, nodejs_test & jasmine_node_test removed

This can be replaced with the `--node_options=--require=$(location label)` argument such as,

```
nodejs_test(
name = "bootstrap_test",
templated_args = ["--node_options=--require=$(rlocation $(location :bootstrap.js))"],
entry_point = ":bootstrap.spec.js",
data = ["bootstrap.js"],
)
```
or
```
jasmine_node_test(
name = "bootstrap_test",
srcs = ["bootstrap.spec.js"],
templated_args = ["--node_options=--require=$(rlocation $(location :bootstrap.js))"],
data = ["bootstrap.js"],
)
```

`templated_args` `$(location)` and `$(locations)` are now correctly expanded when there is no space before ` $(location`
such as `templated_args = ["--node_options=--require=$(rlocation $(location :bootstrap.js))"]`.

Path is returned in runfiles manifest path format such as `repo/path/to/file`. This differs from how $(location)
and $(locations) expansion behaves in expansion the `args` attribute of a *_binary or *_test which returns
the runfiles short path of the format `./path/to/file` for user repo and `../external_repo/path/to/file` for external
repositories. We may change this behavior in the future with $(mlocation) and $(mlocations) used to expand
to the runfiles manifest path.
See https://docs.bazel.build/versions/master/be/common-definitions.html#common-attributes-binaries.
* * pkg_npm attribute packages renamed to nested_packages
* pkg_npm attribute replacements renamed to substitutions
* **builtin:** legacy @build_bazel_rules_nodejs//internal/jasmine_node_test removed; use jasmine_node_test from @bazel/jasmine npm package instead
* **builtin:** `args` in yarn_install and npm_install can be used to pass arbitrary arguments so we removed the following attributes:
* prod_only from yarn_install and npm_install; should be replaced by args = ["--prod"] and args = ["--production"] respectively
* frozen_lockfile from yarn_install; should be replaced by args = ["--frozen-lockfile"]
* network_timeout from yanr_install; should be replaced by args = ["--network_timeout", "<time in ms>"]
* **builtin:** `npm_package` renamed to `pkg_npm`. This is to match the naming convention for package rules https://docs.bazel.build/versions/master/be/pkg.html.
* Users must now switch to loading from index.bzl
* Removed unused exclude_packages from npm_install & yarn_install
* //:declaration_provider.bzl deleted; load from //:providers.bzl instead
//internal/common:npm_pacakge_info.bzl removed; load from //:providers.bzl instead
transitive_js_ecma_script_module_info macro removed; use js_ecma_script_module_info instead
@npm_bazel_karma//:browser_repositories.bzl removed; use @io_bazel_rules_webtesting//web/versioned:browsers-0.3.2.bzl instead
@npm_bazel_protractor//:browser_repositories.bzl removed; use @io_bazel_rules_webtesting//web/versioned:browsers-0.3.2.bzl instead
ts_web_test & ts_web_test_suite marcos removed; use karma_web_test & karma_web_test_suite instead



## [0.42.3](https://github.com/bazelbuild/rules_nodejs/compare/0.42.2...0.42.3) (2019-12-10)

To upgrade:

```python
http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "a54b2511d6dae42c1f7cdaeb08144ee2808193a088004fc3b464a04583d5aa2e",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/0.42.3/rules_nodejs-0.42.3.tar.gz"],
)
```

and run `yarn upgrade --scope @bazel` to update all your `@bazel`-scoped npm packages to the latest versions.
(or manually do the npm equivalent - they don't have a way to update a scope)

### Bug Fixes

* **builtin:** handle scoped packages in generated npm_umd_bundle targets ([#1425](https://github.com/bazelbuild/rules_nodejs/issues/1425)) ([e9e2e8e](https://github.com/bazelbuild/rules_nodejs/commit/e9e2e8e)), closes [#1095](https://github.com/bazelbuild/rules_nodejs/issues/1095)
* **builtin:** only stamp artifacts when --stamp is passed to bazel ([#1441](https://github.com/bazelbuild/rules_nodejs/issues/1441)) ([cbaab60](https://github.com/bazelbuild/rules_nodejs/commit/cbaab60))
* **docs** default values are now documented for rule attributes

### Features

* **builtin:** wire linker/node-patches to npm-generated index.bzl rules ([3321ed5](https://github.com/bazelbuild/rules_nodejs/commit/3321ed5)), closes [#1382](https://github.com/bazelbuild/rules_nodejs/issues/1382)



## [0.42.2](https://github.com/bazelbuild/rules_nodejs/compare/0.42.1...0.42.2) (2019-12-04)


### Bug Fixes

* **builtin:** additional_root_paths in pkg_web should also include paths in genfiles and bin dirs ([#1402](https://github.com/bazelbuild/rules_nodejs/issues/1402)) ([9ce8c85](https://github.com/bazelbuild/rules_nodejs/commit/9ce8c85))
* **typescript:** fix for cross platform ts_devserver issue [#1409](https://github.com/bazelbuild/rules_nodejs/issues/1409) ([#1413](https://github.com/bazelbuild/rules_nodejs/issues/1413)) ([172caff](https://github.com/bazelbuild/rules_nodejs/commit/172caff)), closes [#1415](https://github.com/bazelbuild/rules_nodejs/issues/1415)
* support realpath.native and fix crash in mkdirp ([b9282b9](https://github.com/bazelbuild/rules_nodejs/commit/b9282b9))



## [0.42.1](https://github.com/bazelbuild/rules_nodejs/compare/0.41.0...0.42.1) (2019-11-27)

To upgrade:

```python
http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "c612d6b76eaa17540e8b8c806e02701ed38891460f9ba3303f4424615437887a",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/0.42.1/rules_nodejs-0.42.1.tar.gz"],
)
```

and run `yarn upgrade --scope @bazel` to update all your `@bazel`-scoped npm packages to the latest versions.
(or manually do the npm equivalent - they don't have a way to update a scope)

### New stuff

In 0.41.0 we noted that a feature for inserting `<script>` and `<link>` tags was dropped from `ts_devserver` and `pkg_web` but the replacement wasn't available. Now it is thanks to @jbedard who published a standalone npm package `html-insert-assets`. You can see how it's wired in the examples.

If you waited to upgrade before, now you should.

### Bug Fixes

* @npm//foobar:foobar__files target no longer includes nested node_modules ([#1390](https://github.com/bazelbuild/rules_nodejs/issues/1390)) ([a13f2b6](https://github.com/bazelbuild/rules_nodejs/commit/a13f2b6))
* allow files in protractor data attribute ([3feb13c](https://github.com/bazelbuild/rules_nodejs/commit/3feb13c))
* **builtin:** $(RULEDIR) npm_package_bin expansion should always be the root output directory ([b494974](https://github.com/bazelbuild/rules_nodejs/commit/b494974))
* **builtin:** locations arg of npm_package_bin should result in separate argv ([242379f](https://github.com/bazelbuild/rules_nodejs/commit/242379f))
* **builtin:** use correct genrule-style make vars ([77039b1](https://github.com/bazelbuild/rules_nodejs/commit/77039b1))
* **examples:** kotlin example server working ([adf6934](https://github.com/bazelbuild/rules_nodejs/commit/adf6934))


### BREAKING CHANGES

* **builtin:** We fixed `npm_package_bin` and all rules generated by it, to match genrule behavior as documented at https://docs.bazel.build/versions/master/be/make-variables.html#predefined_genrule_variables
This means that usage of the `$@` shortcut to refer to the output directory should now be `$(@D)` when `output_dir=True`
and you can now use `$@` to refer to the location of a single output



# [0.41.0](https://github.com/bazelbuild/rules_nodejs/compare/0.40.0...0.41.0) (2019-11-22)

To upgrade:

```
http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "8dc1466f8563f3aa4ac7ab7aa3c96651eb7764108219f40b2d1c918e1a81c601",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/0.41.0/rules_nodejs-0.41.0.tar.gz"],
)
```

and run `yarn upgrade --scope @bazel` to update all your `@bazel`-scoped npm packages to the latest versions.
(or manually do the npm equivalent - they don't have a way to update a scope)

### BREAKING CHANGES

As mentioned before, we are close to a 1.0 release, so we are making all our breaking changes now to prepare for a period of stability. Sorry for the long list this time!

* `web_package` rule has been renamed to `pkg_web` and is now a public API

Update your load statements from

```python
load("@build_bazel_rules_nodejs//internal/web_package:web_package.bzl", "web_package")
```

to

```python
load("@build_bazel_rules_nodejs//:index.bzl", "pkg_web")
```

* `ts_devserver` and `pkg_web` (previously `web_package`) no longer have an `index_html` attribute.

They expect an index.html file to be among the assets, and to already
have the script and link tags needed for the app to work.

The feature where those tags were injected into the html dynamically has
been moved to its own rule, inject_html.

We are in a transition state where the `inject_html` rule is not published, because we want this to be a plain npm package and not Bazel-specific. We will publish this functionality soon. If you depend on it, you may want to delay this upgrade.

* internal/rollup_bundle rule is removed. see https://github.com/bazelbuild/rules_nodejs/wiki for migration instructions

* Removed the expand_location_into_runfiles helper from //internal:node.bzl
Load it from //internal/common:expand_into_runfiles instead

* npm karma deps for karma_web_test and karma_web_suite are now peer deps so that the versions used can be chosen by the user.

This PR also removes the built-in  `@io_bazel_rules_webtesting//browsers/sauce:chrome-win10` saucelabs support. It is not very useful as it only tests a single browser and it difficult to use. In the angular repo, saucelabs support was implemented with a custom karma config using karma_web_test. This is the recommended approach.

* `--define=DEBUG=1` is no longer functional to request debugging outputs. Use `-c dbg` instead (this matches Bazel's behavior for C++).

* We renamed some of the generated targets in the `@nodejs//` workspace:

`bazel run @nodejs//:npm` is replaced with `bazel run @nodejs//:npm_node_repositories` and `bazel run @nodejs//:yarn` is replaced with `bazel run @nodejs//:yarn_node_repositories`. `@nodejs//:yarn` and `@nodejs//:npm` now run yarn & npm in the current working directory instead of on all of the `package.json` files in `node_repositories()`.

`@nodejs//:bin/node` & `@nodejs//:bin/node.cmd` (on Windows) are no longer valid targets. Use `@nodejs//:node` instead on all platforms. You can still call the old targets in their platform specific node repositories such as `@nodejs_darwin_amd64//:bin/node`.

`@nodejs//:bin/yarn` & `@nodejs//:bin/yarn.cmd` (on Windows) are no longer valid targets. Use `@nodejs//:yarn` instead on all platforms. You can still call the old targets in their platform specific node repositories such as `@nodejs_darwin_amd64//:bin/yarn`.

`@nodejs//:bin/npm` & `@nodejs//:bin/npm.cmd` (on Windows) are no longer valid targets. Use `@nodejs//:npm` instead on all platforms. You can still call the old targets in their platform specific node repositories such as `@nodejs_darwin_amd64//:bin/npm`.


### Bug Fixes

* **builtin:** allow .tsx entry_point in node binary/test ([313d484](https://github.com/bazelbuild/rules_nodejs/commit/313d484)), closes [#1351](https://github.com/bazelbuild/rules_nodejs/issues/1351)
* **terser:** call terser binary instead of uglifyjs ([#1360](https://github.com/bazelbuild/rules_nodejs/issues/1360)) ([a100420](https://github.com/bazelbuild/rules_nodejs/commit/a100420))
* **terser:** remove ngDevMode & ngI18nClosureMode global_defs from default terser config ([98c8dbc](https://github.com/bazelbuild/rules_nodejs/commit/98c8dbc))


### chore

* remove deprecated re-export file ([148bf8a](https://github.com/bazelbuild/rules_nodejs/commit/148bf8a))
* remove old rollup_bundle ([9a824ac](https://github.com/bazelbuild/rules_nodejs/commit/9a824ac)), closes [#740](https://github.com/bazelbuild/rules_nodejs/issues/740)


### Code Refactoring

* move injector feature to own rule ([be06d23](https://github.com/bazelbuild/rules_nodejs/commit/be06d23))


### Features

* node-patches\filesystem patcher. ([#1332](https://github.com/bazelbuild/rules_nodejs/issues/1332)) ([0b2f675](https://github.com/bazelbuild/rules_nodejs/commit/0b2f675))
* support --compilation_mode flag ([9fa4343](https://github.com/bazelbuild/rules_nodejs/commit/9fa4343))
* **builtin:** rename @nodejs//:npm and @nodejs//:yarn to @nodejs//:[yarn/npm]_node_repositories ([#1369](https://github.com/bazelbuild/rules_nodejs/issues/1369)) ([01079a3](https://github.com/bazelbuild/rules_nodejs/commit/01079a3))
* **karma:** npm peer deps & remove [@rules](https://github.com/rules)_webtesting//browsers/sauce:chrome-win10 support ([318bbf3](https://github.com/bazelbuild/rules_nodejs/commit/318bbf3))
* **protractor:** protractor npm package is now a peer deps ([#1352](https://github.com/bazelbuild/rules_nodejs/issues/1352)) ([5db7c8e](https://github.com/bazelbuild/rules_nodejs/commit/5db7c8e))


# [0.40.0](https://github.com/bazelbuild/rules_nodejs/compare/0.39.1...0.40.0) (2019-11-13)


### Bug Fixes

* fix nodejs_binary cross-platform RBE issue [#1305](https://github.com/bazelbuild/rules_nodejs/issues/1305) ([38d0b3d](https://github.com/bazelbuild/rules_nodejs/commit/38d0b3d))
* prevent dpulicate entries in owners files for global owners ([afea290](https://github.com/bazelbuild/rules_nodejs/commit/afea290))


### Features

* **karma:** remove ts_web_test and ts_web_test_suite rules ([8384562](https://github.com/bazelbuild/rules_nodejs/commit/8384562))
* **terser:** add `args` attribute to support additional command line arguments ([563bad7](https://github.com/bazelbuild/rules_nodejs/commit/563bad7))



## [0.39.1](https://github.com/bazelbuild/rules_nodejs/compare/0.39.0...0.39.1) (2019-10-29)


### Bug Fixes

* fix for https://github.com/bazelbuild/rules_nodejs/issues/1307 ([7163571](https://github.com/bazelbuild/rules_nodejs/commit/7163571))
* **karma:** load scripts in strict mode ([5498f93](https://github.com/bazelbuild/rules_nodejs/commit/5498f93)), closes [#922](https://github.com/bazelbuild/rules_nodejs/issues/922)


### Features

* **examples:** demonstrate using Webpack to build and serve a React app ([c5d0909](https://github.com/bazelbuild/rules_nodejs/commit/c5d0909))



# [0.39.0](https://github.com/bazelbuild/rules_nodejs/compare/0.38.3...0.39.0) (2019-10-23)


### Bug Fixes

* bundle names in angular examples ([b4f01e2](https://github.com/bazelbuild/rules_nodejs/commit/b4f01e2))
* **builtin:** allow more than 2 segments in linker module names ([7e98089](https://github.com/bazelbuild/rules_nodejs/commit/7e98089))
* webpack should be a peerDep of @bazel/labs ([312aa4d](https://github.com/bazelbuild/rules_nodejs/commit/312aa4d))


### Code Refactoring

* remove dynamic_deps feature ([#1276](https://github.com/bazelbuild/rules_nodejs/issues/1276)) ([b916d61](https://github.com/bazelbuild/rules_nodejs/commit/b916d61))


### Features

* **builtin:** turn off a strict requirement for peer dependencies ([#1163](https://github.com/bazelbuild/rules_nodejs/issues/1163)) ([bd2f108](https://github.com/bazelbuild/rules_nodejs/commit/bd2f108))
* **examples:** add Jest example ([#1274](https://github.com/bazelbuild/rules_nodejs/issues/1274)) ([f864462](https://github.com/bazelbuild/rules_nodejs/commit/f864462)), closes [/github.com/ecosia/bazel_rules_nodejs_contrib/issues/4#issuecomment-475291612](https://github.com//github.com/ecosia/bazel_rules_nodejs_contrib/issues/4/issues/issuecomment-475291612)


### BREAKING CHANGES

* The dynamic_deps attribute of yarn_install and npm_install is removed,
in favor of declaring needed packages in the deps/data of the rule that
invokes the tool.



## [0.38.3](https://github.com/bazelbuild/rules_nodejs/compare/0.38.2...0.38.3) (2019-10-11)


### Bug Fixes

* **terser:** terser_minified should support .mjs files when running on directory ([#1264](https://github.com/bazelbuild/rules_nodejs/issues/1264)) ([6b09b51](https://github.com/bazelbuild/rules_nodejs/commit/6b09b51))


### Features

* **examples:** angular view engine example ([#1252](https://github.com/bazelbuild/rules_nodejs/issues/1252)) ([c10272a](https://github.com/bazelbuild/rules_nodejs/commit/c10272a))
* **terser:** support .map files in directory inputs ([#1250](https://github.com/bazelbuild/rules_nodejs/issues/1250)) ([dfefc11](https://github.com/bazelbuild/rules_nodejs/commit/dfefc11))



## [0.38.2](https://github.com/bazelbuild/rules_nodejs/compare/0.38.1...0.38.2) (2019-10-09)


### Bug Fixes

* clean_nested_workspaces.sh ([acaa5fb](https://github.com/bazelbuild/rules_nodejs/commit/acaa5fb))
* **rollup:** handle transitive npm deps in rollup_bundle ([77289e0](https://github.com/bazelbuild/rules_nodejs/commit/77289e0))
* dont generate build files in symlinked node_modules ([#1111](https://github.com/bazelbuild/rules_nodejs/issues/1111)) ([2e7de34](https://github.com/bazelbuild/rules_nodejs/commit/2e7de34)), closes [#871](https://github.com/bazelbuild/rules_nodejs/issues/871)
* linker can't assume that transitive module_mappings are in the sandbox ([a67a844](https://github.com/bazelbuild/rules_nodejs/commit/a67a844))


### Features

* **examples:** add closure compiler example ([79b0927](https://github.com/bazelbuild/rules_nodejs/commit/79b0927))
* document the escape hatch from ts_library ([#1247](https://github.com/bazelbuild/rules_nodejs/issues/1247)) ([baa9aa8](https://github.com/bazelbuild/rules_nodejs/commit/baa9aa8))
* **examples:** illustrate how to run a mocha test ([#1216](https://github.com/bazelbuild/rules_nodejs/issues/1216)) ([5485a8a](https://github.com/bazelbuild/rules_nodejs/commit/5485a8a))
* **examples:** update examples/angular to new rollup_bundle ([#1238](https://github.com/bazelbuild/rules_nodejs/issues/1238)) ([54f5d8c](https://github.com/bazelbuild/rules_nodejs/commit/54f5d8c))
* **terser:** add source map links ([32eb7ca](https://github.com/bazelbuild/rules_nodejs/commit/32eb7ca))
* **typescript:** add a transitive_js_ecma_script_module_info alias to js_ecma_script_module_info ([#1243](https://github.com/bazelbuild/rules_nodejs/issues/1243)) ([77e2d4a](https://github.com/bazelbuild/rules_nodejs/commit/77e2d4a))
* **typescript:** add direct_sources field to JSEcmaScriptModuleInfo ([1ee00e6](https://github.com/bazelbuild/rules_nodejs/commit/1ee00e6))
* **typescript:** add JSNamedModuleInfo provider to ts_library outputs ([#1215](https://github.com/bazelbuild/rules_nodejs/issues/1215)) ([bb1f9b4](https://github.com/bazelbuild/rules_nodejs/commit/bb1f9b4))



## [0.38.1](https://github.com/bazelbuild/rules_nodejs/compare/0.38.0...0.38.1) (2019-10-03)


### Bug Fixes

* **builtin:** bugs in 0.38 found while rolling out to angular repo ([d2262c8](https://github.com/bazelbuild/rules_nodejs/commit/d2262c8))
* **README:** update "sections below" reference ([#1210](https://github.com/bazelbuild/rules_nodejs/issues/1210)) ([a59203c](https://github.com/bazelbuild/rules_nodejs/commit/a59203c))
* invalidate installed npm repositories correctly ([#1200](https://github.com/bazelbuild/rules_nodejs/issues/1200)) ([#1205](https://github.com/bazelbuild/rules_nodejs/issues/1205)) ([0312800](https://github.com/bazelbuild/rules_nodejs/commit/0312800))
* **docs:** fix typo in TypeScript.md ([#1211](https://github.com/bazelbuild/rules_nodejs/issues/1211)) ([893f61e](https://github.com/bazelbuild/rules_nodejs/commit/893f61e))
* pin @bazel/karma karma dep to ~4.1.0 as 4.2.0 breaks stack traces in karma output ([4e86283](https://github.com/bazelbuild/rules_nodejs/commit/4e86283))


### Features

* **examples:** updated to angular 8.2.8 in examples/angular ([#1226](https://github.com/bazelbuild/rules_nodejs/issues/1226)) ([697bd22](https://github.com/bazelbuild/rules_nodejs/commit/697bd22))
* **examples:** upgrade to v9 and enable ivy ([#1227](https://github.com/bazelbuild/rules_nodejs/issues/1227)) ([1c7426f](https://github.com/bazelbuild/rules_nodejs/commit/1c7426f))



# [0.38.0](https://github.com/bazelbuild/rules_nodejs/compare/0.37.0...0.38.0) (2019-09-26)


### Bug Fixes

* **builtin:** linker test should run program as an action ([#1113](https://github.com/bazelbuild/rules_nodejs/issues/1113)) ([7f0102e](https://github.com/bazelbuild/rules_nodejs/commit/7f0102e))
* add golden file ([9a02ee0](https://github.com/bazelbuild/rules_nodejs/commit/9a02ee0))
* add missing async test fixes ([12f711a](https://github.com/bazelbuild/rules_nodejs/commit/12f711a))
* **builtin:** support for scoped modules in linker ([#1199](https://github.com/bazelbuild/rules_nodejs/issues/1199)) ([94abf68](https://github.com/bazelbuild/rules_nodejs/commit/94abf68))
* **protractor:** update rules_webtesting patch to include additional windows fixes ([#1140](https://github.com/bazelbuild/rules_nodejs/issues/1140)) ([f76e97b](https://github.com/bazelbuild/rules_nodejs/commit/f76e97b))
* **rollup:** npm requires an index.js file ([2ababdf](https://github.com/bazelbuild/rules_nodejs/commit/2ababdf))


### chore

* cleanup some deprecated APIs ([#1160](https://github.com/bazelbuild/rules_nodejs/issues/1160)) ([cefc2ae](https://github.com/bazelbuild/rules_nodejs/commit/cefc2ae)), closes [#1144](https://github.com/bazelbuild/rules_nodejs/issues/1144)


### Code Refactoring

* remove http_server and history_server rules ([#1158](https://github.com/bazelbuild/rules_nodejs/issues/1158)) ([01fdeec](https://github.com/bazelbuild/rules_nodejs/commit/01fdeec))


### Features

* **builtin:** detect APF node module format if ANGULAR_PACKAGE file found ([#1112](https://github.com/bazelbuild/rules_nodejs/issues/1112)) ([162e436](https://github.com/bazelbuild/rules_nodejs/commit/162e436))
* **builtin:** expose the new linker to node programs ([65d8a36](https://github.com/bazelbuild/rules_nodejs/commit/65d8a36))
* **builtin:** introduce npm_package_bin ([#1139](https://github.com/bazelbuild/rules_nodejs/issues/1139)) ([2fd80cf](https://github.com/bazelbuild/rules_nodejs/commit/2fd80cf))
* **builtin:** linker should resolve workspace-absolute paths ([307a796](https://github.com/bazelbuild/rules_nodejs/commit/307a796))
* **builtin:** npm_package_bin can produce directory output ([#1164](https://github.com/bazelbuild/rules_nodejs/issues/1164)) ([6d8c625](https://github.com/bazelbuild/rules_nodejs/commit/6d8c625))
* **examples:** demonstrate that a macro assembles a workflow ([7231aaa](https://github.com/bazelbuild/rules_nodejs/commit/7231aaa))
* **examples:** replace examples/webapp with new rollup_bundle ([c6cd91c](https://github.com/bazelbuild/rules_nodejs/commit/c6cd91c))
* **examples:** the Angular example now lives in rules_nodejs ([9072ddb](https://github.com/bazelbuild/rules_nodejs/commit/9072ddb))
* **rollup:** ensure that sourcemaps work end-to-end ([f340589](https://github.com/bazelbuild/rules_nodejs/commit/f340589))
* **rollup:** new implementation of rollup_bundle in @bazel/rollup package ([3873715](https://github.com/bazelbuild/rules_nodejs/commit/3873715)), closes [#532](https://github.com/bazelbuild/rules_nodejs/issues/532) [#724](https://github.com/bazelbuild/rules_nodejs/issues/724)
* **rollup:** support multiple entry points ([f660d39](https://github.com/bazelbuild/rules_nodejs/commit/f660d39))
* **rollup:** tests and docs for new rollup_bundle ([cfef773](https://github.com/bazelbuild/rules_nodejs/commit/cfef773))
* **terser:** support directory inputs ([21b5142](https://github.com/bazelbuild/rules_nodejs/commit/21b5142))
* add angular example ([#1124](https://github.com/bazelbuild/rules_nodejs/issues/1124)) ([c376355](https://github.com/bazelbuild/rules_nodejs/commit/c376355))
* **terser:** support source map files ([#1195](https://github.com/bazelbuild/rules_nodejs/issues/1195)) ([d5bac48](https://github.com/bazelbuild/rules_nodejs/commit/d5bac48))
* **typescript:** add JSEcmaScriptModuleInfo provider to ts_library outputs ([1433eb9](https://github.com/bazelbuild/rules_nodejs/commit/1433eb9))


### BREAKING CHANGES

* @bazel/typescript and @bazel/karma no longer have a defs.bzl file. Use
index.bzl instead.

The @yarn workspace is no longer created. Use @nodejs//:yarn instead.
* history_server and http_server rules are no longer built-in.

To use them, first install the http-server and/or history-server packages
Then load("@npm//http-server:index.bzl", "http_server")
(or replace with history-server, noting that the rule has underscore where the package has hyphen)



## [0.37.1](https://github.com/bazelbuild/rules_nodejs/compare/0.37.0...0.37.1) (2019-09-16)


### Bug Fixes

* **protractor:** update rules_webtesting patch to include additional windows fixes ([#1140](https://github.com/bazelbuild/rules_nodejs/issues/1140)) ([f76e97b](https://github.com/bazelbuild/rules_nodejs/commit/f76e97b))
* **rollup:** npm requires an index.js file ([2ababdf](https://github.com/bazelbuild/rules_nodejs/commit/2ababdf))
* add golden file ([9a02ee0](https://github.com/bazelbuild/rules_nodejs/commit/9a02ee0))
* add missing async test fixes ([12f711a](https://github.com/bazelbuild/rules_nodejs/commit/12f711a))
* **builtin:** linker test should run program as an action ([#1113](https://github.com/bazelbuild/rules_nodejs/issues/1113)) ([7f0102e](https://github.com/bazelbuild/rules_nodejs/commit/7f0102e))


### Features

* **examples:** the Angular example now lives in rules_nodejs ([9072ddb](https://github.com/bazelbuild/rules_nodejs/commit/9072ddb))
* add angular example ([#1124](https://github.com/bazelbuild/rules_nodejs/issues/1124)) ([c376355](https://github.com/bazelbuild/rules_nodejs/commit/c376355))
* **builtin:** detect APF node module format if ANGULAR_PACKAGE file found ([#1112](https://github.com/bazelbuild/rules_nodejs/issues/1112)) ([162e436](https://github.com/bazelbuild/rules_nodejs/commit/162e436))
* **builtin:** expose the new linker to node programs ([65d8a36](https://github.com/bazelbuild/rules_nodejs/commit/65d8a36))
* **rollup:** new implementation of rollup_bundle in @bazel/rollup package ([3873715](https://github.com/bazelbuild/rules_nodejs/commit/3873715)), closes [#532](https://github.com/bazelbuild/rules_nodejs/issues/532) [#724](https://github.com/bazelbuild/rules_nodejs/issues/724)
* **rollup:** support multiple entry points ([f660d39](https://github.com/bazelbuild/rules_nodejs/commit/f660d39))
* **rollup:** tests and docs for new rollup_bundle ([cfef773](https://github.com/bazelbuild/rules_nodejs/commit/cfef773))
* **terser:** support directory inputs ([21b5142](https://github.com/bazelbuild/rules_nodejs/commit/21b5142))



# [0.37.0](https://github.com/bazelbuild/rules_nodejs/compare/0.36.2...0.37.0) (2019-09-06)


### Bug Fixes

* **builtin:** --nolegacy_external_runfiles on build ([38814aa](https://github.com/bazelbuild/rules_nodejs/commit/38814aa))
* **builtin:** fix localWorkspacePath logic ([0a7fb01](https://github.com/bazelbuild/rules_nodejs/commit/0a7fb01)), closes [#1087](https://github.com/bazelbuild/rules_nodejs/issues/1087)
* **npm_install:** dynamic_deps attribute not working for scoped packages ([bf68577](https://github.com/bazelbuild/rules_nodejs/commit/bf68577))
* node executables not running on windows if bash toolchain path ([#1104](https://github.com/bazelbuild/rules_nodejs/issues/1104)) ([c82b43d](https://github.com/bazelbuild/rules_nodejs/commit/c82b43d))
* node_loader windows fix for RUNFILES_MANIFEST_FILE slashes ([d3886ce](https://github.com/bazelbuild/rules_nodejs/commit/d3886ce))


### chore

* remove tsc_wrapped_deps compatibility ([#1100](https://github.com/bazelbuild/rules_nodejs/issues/1100)) ([5e98bda](https://github.com/bazelbuild/rules_nodejs/commit/5e98bda)), closes [#1086](https://github.com/bazelbuild/rules_nodejs/issues/1086)


### Features

* add default DEBUG and VERBOSE_LOGS configuration_env_vars to nodejs_binary ([#1080](https://github.com/bazelbuild/rules_nodejs/issues/1080)) ([df37fca](https://github.com/bazelbuild/rules_nodejs/commit/df37fca))
* **builtin:** add Kotlin example ([0912014](https://github.com/bazelbuild/rules_nodejs/commit/0912014))
* **builtin:** introduce a linker ([62037c9](https://github.com/bazelbuild/rules_nodejs/commit/62037c9))


### BREAKING CHANGES

* A compatibility layer was removed. See discussion in https://github.com/bazelbuild/rules_nodejs/issues/1086



## [0.36.2](https://github.com/bazelbuild/rules_nodejs/compare/0.36.1...0.36.2) (2019-08-30)


### Bug Fixes

* account for breaking path change in new chromedriver distro ([d8a0ccb](https://github.com/bazelbuild/rules_nodejs/commit/d8a0ccb)), closes [/github.com/bazelbuild/rules_webtesting/commit/62062b4bd111acc8598bfc816e87cda012bdaae6#diff-bb710201187c4ad0a3fbbe941ffc4b0](https://github.com//github.com/bazelbuild/rules_webtesting/commit/62062b4bd111acc8598bfc816e87cda012bdaae6/issues/diff-bb710201187c4ad0a3fbbe941ffc4b0)
* patching rules_webtesting to fix chrome path ([97933d8](https://github.com/bazelbuild/rules_nodejs/commit/97933d8))
* **builtin:** reformat the error message for Node loader.js ([67bca8f](https://github.com/bazelbuild/rules_nodejs/commit/67bca8f)), closes [/github.com/nodejs/node/blob/a49b20d3245dd2a4d890e28582f3c013c07c3136/lib/internal/modules/cjs/loader.js#L264](https://github.com//github.com/nodejs/node/blob/a49b20d3245dd2a4d890e28582f3c013c07c3136/lib/internal/modules/cjs/loader.js/issues/L264)
* **karma:** error messages truncated due to custom formatter ([f871be6](https://github.com/bazelbuild/rules_nodejs/commit/f871be6))
* **typescript:** add peerDependency on typescript ([48c5088](https://github.com/bazelbuild/rules_nodejs/commit/48c5088))


### Features

* **builtin:** add a DeclarationInfo provider ([3d7eb13](https://github.com/bazelbuild/rules_nodejs/commit/3d7eb13))
* add templated_args_file to allow long agrs to be written to a file ([b34d7bb](https://github.com/bazelbuild/rules_nodejs/commit/b34d7bb))
* **builtin:** support yarn --frozen_lockfile ([426861f](https://github.com/bazelbuild/rules_nodejs/commit/426861f))
* **terser:** introduce @bazel/terser package ([232acfe](https://github.com/bazelbuild/rules_nodejs/commit/232acfe))



## [0.36.1](https://github.com/bazelbuild/rules_nodejs/compare/0.36.0...0.36.1) (2019-08-20)


### Features

* **builtin:** add browser to rollup mainFields ([e488cb6](https://github.com/bazelbuild/rules_nodejs/commit/e488cb6))
* **builtin:** introduce dynamic dependencies concept ([a47410e](https://github.com/bazelbuild/rules_nodejs/commit/a47410e))
* **less:** add less link to the docs's drawer ([ec6e0d1](https://github.com/bazelbuild/rules_nodejs/commit/ec6e0d1))
* **less:** new less package ([462f6e9](https://github.com/bazelbuild/rules_nodejs/commit/462f6e9))
* **less:** updated default compiler to @bazel/less as mentioned in code review ([fd71f26](https://github.com/bazelbuild/rules_nodejs/commit/fd71f26))
* **less:** updated package.json in e2e/less to pull latest ([6027aa3](https://github.com/bazelbuild/rules_nodejs/commit/6027aa3))



# [0.36.0](https://github.com/bazelbuild/rules_nodejs/compare/0.35.0...0.36.0) (2019-08-15)


### Bug Fixes

* **jasmine:** correct comment about behavior of config_file attr ([59a7239](https://github.com/bazelbuild/rules_nodejs/commit/59a7239))
* fix yarn_install yarn cache mutex bug ([31aa1a6](https://github.com/bazelbuild/rules_nodejs/commit/31aa1a6))
* get rules_go dependency from build_bazel_rules_typescript ([ea6ee0b](https://github.com/bazelbuild/rules_nodejs/commit/ea6ee0b))
* npm_package issue with external files on windows ([8679b9e](https://github.com/bazelbuild/rules_nodejs/commit/8679b9e))
* sconfig deps sandbox bug ([161693c](https://github.com/bazelbuild/rules_nodejs/commit/161693c))


### Features

* **jasmine:** introduce config_file attribute ([b0b2648](https://github.com/bazelbuild/rules_nodejs/commit/b0b2648))
* **jasmine_node_test:** add attr `jasmine_config` ([715ffc6](https://github.com/bazelbuild/rules_nodejs/commit/715ffc6))
* **worker:** new worker package ([9e26856](https://github.com/bazelbuild/rules_nodejs/commit/9e26856))
* add browser module main priority to generated umd bundles ([17cfac9](https://github.com/bazelbuild/rules_nodejs/commit/17cfac9))



# [0.35.0](https://github.com/bazelbuild/rules_nodejs/compare/0.34.0...0.35.0) (2019-08-02)


### Bug Fixes

* **jasmine:** enforce that jasmine_node_test is loaded from new location ([7708858](https://github.com/bazelbuild/rules_nodejs/commit/7708858)), closes [#838](https://github.com/bazelbuild/rules_nodejs/issues/838)
* fencing for npm packages ([#946](https://github.com/bazelbuild/rules_nodejs/issues/946)) ([780dfb4](https://github.com/bazelbuild/rules_nodejs/commit/780dfb4))


### Features

* **builtin:** do code splitting even if only one entry point ([f51c129](https://github.com/bazelbuild/rules_nodejs/commit/f51c129))
* **stylus:** add initial stylus rule ([804a788](https://github.com/bazelbuild/rules_nodejs/commit/804a788))
* **stylus:** output sourcemap ([dac014a](https://github.com/bazelbuild/rules_nodejs/commit/dac014a))
* **stylus:** support import by allowing files in deps ([3987070](https://github.com/bazelbuild/rules_nodejs/commit/3987070))


### BREAKING CHANGES

* **jasmine:** You can no longer get jasmine_node_test from @build_bazel_rules_nodejs.
- Use `load("@npm_bazel_jasmine//:index.bzl", "jasmine_node_test")`
instead
- You need to remove `@npm//jasmine` from the deps of the
jasmine_node_test
- If you use user-managed dependencies, see the commit for examples of
the change needed

Also makes the repo bazel-lint-clean, so running yarn bazel:lint-fix no
longer makes edits.



# [0.34.0](https://github.com/bazelbuild/rules_nodejs/compare/0.33.1...0.34.0) (2019-07-23)


### Bug Fixes

* **builtin:** process/browser should resolve from browserify ([a98eda7](https://github.com/bazelbuild/rules_nodejs/commit/a98eda7))
* fix for node windows cross-compile ([001d945](https://github.com/bazelbuild/rules_nodejs/commit/001d945)), closes [#909](https://github.com/bazelbuild/rules_nodejs/issues/909)
* node runfiles resolution from external workspaces ([82500de](https://github.com/bazelbuild/rules_nodejs/commit/82500de))


### Features

* **protractor:** add protractor rule ([35a344c](https://github.com/bazelbuild/rules_nodejs/commit/35a344c))



## [0.33.1](https://github.com/bazelbuild/rules_nodejs/compare/0.33.0...0.33.1) (2019-07-12)


### Bug Fixes

* **builtin:** include package.json files in browserify inputs ([13c09e6](https://github.com/bazelbuild/rules_nodejs/commit/13c09e6))



# [0.33.0](https://github.com/bazelbuild/rules_nodejs/compare/0.32.2...0.33.0) (2019-07-12)


### Bug Fixes

* **builtin:** update to latest ncc ([c1e3f4d](https://github.com/bazelbuild/rules_nodejs/commit/c1e3f4d)), closes [#771](https://github.com/bazelbuild/rules_nodejs/issues/771)
* **builtin:** use a local mod to revert a browserify change ([253e9cb](https://github.com/bazelbuild/rules_nodejs/commit/253e9cb))


### Features

* **builtin:** add nodejs toolchain support ([9afb8db](https://github.com/bazelbuild/rules_nodejs/commit/9afb8db))



## [0.32.2](https://github.com/bazelbuild/rules_nodejs/compare/0.32.1...0.32.2) (2019-06-21)


### Bug Fixes

* **builtin:** add test case for @bazel/hide-bazel-files bug ([2a63ed6](https://github.com/bazelbuild/rules_nodejs/commit/2a63ed6))
* **builtin:** always hide bazel files in yarn_install & npm install--- ([0104be7](https://github.com/bazelbuild/rules_nodejs/commit/0104be7))



## [0.32.1](https://github.com/bazelbuild/rules_nodejs/compare/0.32.0...0.32.1) (2019-06-19)


### Bug Fixes

* **typescript:** exclude typescript lib declarations in ([3d55b41](https://github.com/bazelbuild/rules_nodejs/commit/3d55b41))
* **typescript:** remove override of @bazel/tsetse ([2e128ce](https://github.com/bazelbuild/rules_nodejs/commit/2e128ce))



# [0.32.0](https://github.com/bazelbuild/rules_nodejs/compare/0.31.1...0.32.0) (2019-06-18)


### Bug Fixes

* **builtin:** add @bazel/hide-bazel-files utility ([e7d2fbd](https://github.com/bazelbuild/rules_nodejs/commit/e7d2fbd))
* **builtin:** fix for issue 834 ([#847](https://github.com/bazelbuild/rules_nodejs/issues/847)) ([c0fe512](https://github.com/bazelbuild/rules_nodejs/commit/c0fe512))
* **builtin:** fix for symlinked node_modules issue [#802](https://github.com/bazelbuild/rules_nodejs/issues/802) ([43cebe7](https://github.com/bazelbuild/rules_nodejs/commit/43cebe7))
* **create:** run ts_setup_workspace in TypeScript workspaces ([c8e61c5](https://github.com/bazelbuild/rules_nodejs/commit/c8e61c5))
* **typescript:** fix issue with types[] in non-sandboxed tsc ([08b231a](https://github.com/bazelbuild/rules_nodejs/commit/08b231a))
* **typescript:** include transitive_declarations ([bbcfcdd](https://github.com/bazelbuild/rules_nodejs/commit/bbcfcdd))


### Features

* **builtin:** e2e tests for symlinked node_modules and hide-bazel-files ([8cafe43](https://github.com/bazelbuild/rules_nodejs/commit/8cafe43))
* **create:** add a .gitignore file in new workspaces ([#849](https://github.com/bazelbuild/rules_nodejs/issues/849)) ([3c05167](https://github.com/bazelbuild/rules_nodejs/commit/3c05167))
* **create:** add hide-bazel-files to @bazel/create ([03b7dae](https://github.com/bazelbuild/rules_nodejs/commit/03b7dae))
* implicit hide-bazel-files ([1a8175d](https://github.com/bazelbuild/rules_nodejs/commit/1a8175d))
