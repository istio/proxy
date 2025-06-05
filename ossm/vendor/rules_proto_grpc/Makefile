# Run the rulegen system
.PHONY: rulegen
rulegen:
	bazel query '//example/routeguide/... - attr(tags, manual, //example/routeguide/...)' > available_tests.txt; \
	bazel run --run_under="cd $$PWD && " //tools/rulegen -- --ref=$$(git describe --abbrev=0 --tags); \
	rm available_tests.txt;


# Build docs locally
.PHONY: docs
docs:
	python3 -m sphinx -c docs -a -E -T -W --keep-going docs docs/build


# Apply buildifier
.PHONY: buildifier
buildifier:
	bazel run //tools:buildifier


# Run crate_universe to update the rust dependencies
.PHONY: rust_crates_vendor
rust_crates_vendor:
	bazel run //rust:crates_vendor -- --repin


# Run yarn to upgrade the js dependencies
.PHONY: yarn_upgrade
yarn_upgrade:
	cd js/requirements; \
	rm yarn.lock; \
	yarn install; \


# Run bundle to upgrade the Ruby dependencies
.PHONY: ruby_bundle_upgrade
ruby_bundle_upgrade:
	cd ruby; \
	rm Gemfile.lock; \
	bundle install --path /tmp/ruby-bundle; \


# Run pip-compile to upgrade python dependencies
.PHONY: pip_compile
pip_compile:
	echo '' > python/requirements.txt
	bazel run //python:requirements.update


# Run C# package regeneration
.PHONY: csharp_regenerate_packages
csharp_regenerate_packages:
	./csharp/nuget/regenerate_packages.sh
	bazel run //tools:buildifier


# Run F# package regeneration
.PHONY: fsharp_regenerate_packages
fsharp_regenerate_packages:
	./fsharp/nuget/regenerate_packages.sh
	bazel run //tools:buildifier


# Run all language specific updates
.PHONY: all_updates
all_updates: rust_crates_vendor yarn_upgrade ruby_bundle_upgrade pip_compile csharp_regenerate_packages fsharp_regenerate_packages


# Pull in auto-generated examples makefile
include example/Makefile.mk

# Pull in auto-generated test workspaces makefile
include test_workspaces/Makefile.mk
