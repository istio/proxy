:::{default-domain} bzl
:::

# Patching wheels

Sometimes the wheels have to be patched to:
* Workaround the lack of a standard `site-packages` layout ({gh-issue}`2156`).
* Include certain PRs of your choice on top of wheels and avoid building from sdist.

You can patch the wheels by using the {attr}`pip.override.patches` attribute.
