# __path__ manipulation added by bazel-contrib/rules_python to support namespace pkgs.
__path__ = __import__("pkgutil").extend_path(__path__, __name__)
