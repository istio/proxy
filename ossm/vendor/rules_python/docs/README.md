# rules_python Sphinx docs generation

The docs for rules_python are generated using a combination of Sphinx, Bazel,
and Read the Docs. The Markdown files in source control are unlikely to render
properly without the Sphinx processing step because they rely on Sphinx and
MyST-specific Markdown functionality.

The actual sources that Sphinx consumes are in this directory, with Stardoc
generating additional sources for Sphinx.

Manually building the docs isn't necessary -- Read the Docs will
automatically build and deploy them when commits are pushed to the repo.

## Generating docs for development

Generating docs for development is a two-part process: starting a local HTTP
server to serve the generated HTML, and re-generating the HTML when sources
change. The quick start is:

```
bazel run //docs:docs.serve  # Run in separate terminal
ibazel build //docs:docs  # Automatically rebuilds docs
```

This will build the docs and start a local webserver at http://localhost:8000
where you can view the output. As you edit files, ibazel will detect the file
changes and re-run the build process, and you can simply refresh your browser to
see the changes. Using ibazel is not required; you can manually run the
equivalent bazel command if desired.

An alternative to `ibazel` is using `inotify` on Linux systems:

```
inotifywait --event modify --monitor . --recursive --includei '^.*\.md$' |
while read -r dir events filename; do bazel build //docs:docs; done;
```

And lastly, a poor-man's `ibazel` and `inotify` is simply `watch` with
a reasonable interval like 10s:

```
watch --interval 10 bazel build //docs:docs
```

### Installing ibazel

The `ibazel` tool can be used to automatically rebuild the docs as you
develop them. See the [ibazel docs](https://github.com/bazelbuild/bazel-watcher) for
how to install it. The quick start for Linux is:

```
sudo apt install npm
sudo npm install -g @bazel/ibazel
```

## MyST Markdown flavor

Sphinx is configured to parse Markdown files using MyST, which is a more
advanced flavor of Markdown that supports most features of restructured text and
integrates with Sphinx functionality such as automatic cross references,
creating indexes, and using concise markup to generate rich documentation.

MyST features and behaviors are controlled by the Sphinx configuration file,
`docs/conf.py`. For more info, see https://myst-parser.readthedocs.io.

## Sphinx configuration

The Sphinx-specific configuration files and input doc files live in
docs/.

The Sphinx configuration is `docs/conf.py`. See
https://www.sphinx-doc.org/ for details about the configuration file.

## Read the Docs configuration

There's two basic parts to the Read the Docs configuration:

*   `.readthedocs.yaml`: This configuration file controls most settings, such as
    the OS version used to build, Python version, dependencies, what Bazel
    commands to run, etc.
*   https://readthedocs.org/projects/rules-python: This is the project
    administration page. While most settings come from the config file, this
    controls additional settings such as permissions, what versions are
    published, when to publish changes, etc.

For more Read the Docs configuration details, see docs.readthedocs.io.
