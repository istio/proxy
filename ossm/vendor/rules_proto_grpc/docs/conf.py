import datetime

# -- Project information -----------------------------------------------------

project = 'rules_proto_grpc'
copyright = '{}, rules_proto_grpc authors - Apache 2.0 License'.format(
    datetime.date.today().year
)
author = 'rules_proto_grpc authors'
release = ''
version = release


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'notfound.extension',
    'sphinx_sitemap',
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['.ipynb_checkpoints', '**/.ipynb_checkpoints']

# Code highlighting
pygments_style = 'monokai'


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'furo'

# Title
html_title = 'Protobuf and gRPC rules for Bazel - rules_proto_grpc'

# Logo and favicon
html_logo = '_static/logo.svg'
html_favicon = '_static/logo.png'

# Extra vars to provide to templating
html_context = {
    'absolute_icon_png': 'https://rules-proto-grpc.com/en/latest/_static/logo.png'  # Used by meta tags
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

# Theme options
html_theme_options = {
    'sidebar_hide_name': True,
    'light_css_variables': {
        'color-brand-primary': '#38a3a5',
        'color-brand-content': '#38a3a5',
    },
    'dark_css_variables': {
        'color-brand-primary': '#38a3a5',
        'color-brand-content': '#38a3a5',
    },
}

# Disable footer
html_show_sphinx = False

# Add CSS files
html_css_files = []

# Extra files to include
html_extra_path = [
    'robots.txt',
]

# Sitemap options
sitemap_filename = "sitemap-override.xml"  # RTD generates sitemap with not much in it...
sitemap_url_scheme = "{link}"
