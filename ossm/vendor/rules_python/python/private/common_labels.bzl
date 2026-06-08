"""Constants for common labels used in the codebase."""

# NOTE: str() is called because some APIs don't accept Label objects
# (e.g. transition inputs/outputs or the transition settings return dict)

labels = struct(
    # keep sorted
    ADD_SRCS_TO_RUNFILES = str(Label("//python/config_settings:add_srcs_to_runfiles")),
    BOOTSTRAP_IMPL = str(Label("//python/config_settings:bootstrap_impl")),
    BUILD_PYTHON_ZIP = str(Label("//python/config_settings:build_python_zip")),
    # NOTE: Special target; see definition for details.
    BUILD_RUNFILE_LINKS = str(Label("//command_line_option:build_runfile_links")),
    DEBUGGER = str(Label("//python/config_settings:debugger")),
    # NOTE: Special target; see definition for details.
    ENABLE_RUNFILES = str(Label("//command_line_option:enable_runfiles")),
    EXEC_TOOLS_TOOLCHAIN = str(Label("//python/config_settings:exec_tools_toolchain")),
    NONE = str(Label("//python:none")),
    PIP_ENV_MARKER_CONFIG = str(Label("//python/config_settings:pip_env_marker_config")),
    PIP_WHL = str(Label("//python/config_settings:pip_whl")),
    PIP_WHL_GLIBC_VERSION = str(Label("//python/config_settings:pip_whl_glibc_version")),
    PIP_WHL_MUSLC_VERSION = str(Label("//python/config_settings:pip_whl_muslc_version")),
    PIP_WHL_OSX_ARCH = str(Label("//python/config_settings:pip_whl_osx_arch")),
    PIP_WHL_OSX_VERSION = str(Label("//python/config_settings:pip_whl_osx_version")),
    PLATFORMS_OS_WINDOWS = str(Label("@platforms//os:windows")),
    PRECOMPILE = str(Label("//python/config_settings:precompile")),
    PRECOMPILE_SOURCE_RETENTION = str(Label("//python/config_settings:precompile_source_retention")),
    PYC_COLLECTION = str(Label("//python/config_settings:pyc_collection")),
    PYTHON_IMPORT_ALL_REPOSITORIES = str(Label("//python/config_settings:experimental_python_import_all_repositories")),
    PYTHON_SRC = str(Label("//python/bin:python_src")),
    PYTHON_VERSION = str(Label("//python/config_settings:python_version")),
    PYTHON_VERSION_MAJOR_MINOR = str(Label("//python/config_settings:python_version_major_minor")),
    PY_FREETHREADED = str(Label("//python/config_settings:py_freethreaded")),
    PY_LINUX_LIBC = str(Label("//python/config_settings:py_linux_libc")),
    REPL_DEP = str(Label("//python/bin:repl_dep")),
    VENVS_SITE_PACKAGES = str(Label("//python/config_settings:venvs_site_packages")),
    VENVS_USE_DECLARE_SYMLINK = str(Label("//python/config_settings:venvs_use_declare_symlink")),
    VISIBLE_FOR_TESTING = str(Label("//python/private:visible_for_testing")),
)
