# Bazel Website Builder - Testing and Generator Agnostic Design

## Overview

The website builder in `bazel/website/` provides Bazel rules for building static websites. The design intends to be generator-agnostic, allowing different static site generators (not just Pelican) to be used.

## Current State of Generator-Agnostic Design

### What Works Well (Already Generator-Agnostic)

1. **Generator Parameter**: The `static_website()` macro accepts a `generator` parameter that can point to any Bazel target that acts as a site generator.
   ```starlark
   generator = "@envoy_toolshed//website/tools/pelican"  # Default, but customizable
   ```

2. **Simple Generator Interface**: The generator is invoked with just the content path as an argument:
   ```bash
   $GENERATOR "$CONTENT"
   ```
   This simple interface makes it easy for different generators to work.

3. **Customizable Parameters**: Key parameters can be customized per-site:
   - `exclude`: List of patterns to exclude from final tarball
   - `mappings`: Dictionary of source to destination path mappings
   - `output_path`: Where the generator outputs files
   - `content_path`: Where content files are located

### Areas That Need Improvement for Full Generator Agnostic Support

1. **Default Exclude Patterns**: The default `exclude` list contains Pelican-specific patterns:
   ```starlark
   exclude = [
       "archives.html",      # Pelican-specific
       "authors.html",       # Pelican-specific
       "categories.html",    # Pelican-specific
       "external",
       "tags.html",          # Pelican-specific
       "pages",              # Pelican-specific
       "theme/.webassets-cache",
       "theme/css/_sass",
       "theme/css/main.scss",
   ]
   ```
   **Recommendation**: These should either be:
   - Moved to a Pelican-specific wrapper function
   - Made conditional based on the generator
   - Or just documented that users should override them when using other generators

2. **Default Mappings**: The default `mappings` assume Pelican's theme structure:
   ```starlark
   mappings = {
       "theme/css": "theme/static/css",
       "theme/js": "theme/static/js",
       "theme/images": "theme/static/images",
       "theme/templates/extra": "theme/templates",
   }
   ```
   **Recommendation**: Same as above - either make generator-specific wrappers or document override expectations.

## Testing

Tests are located in `bazel/website/tests/` and verify:

1. **Basic Website Generation**: Tests that the macro can build a complete website tarball
2. **Custom Exclude Patterns**: Tests that custom exclude patterns work
3. **Custom Mappings**: Tests that custom (or empty) mappings work
4. **Generator Flexibility**: Tests verify the macro compiles with different parameter combinations

### Running Tests

```bash
cd bazel
bazel test //website/tests/...
```

### Test Structure

- `tests/test_content/`: Minimal markdown content for test websites
- `tests/test_theme/`: Minimal Pelican theme templates
- `tests/test_config.py`: Minimal Pelican configuration
- `tests/BUILD`: Test targets using the website macros
- `tests/run_website_tests.sh`: Basic functionality tests
- `tests/run_parameterized_tests.sh`: Parameter flexibility tests

## Recommendations for Future Iterations

1. **Create Generator-Specific Wrappers**:
   ```starlark
   def pelican_website(**kwargs):
       # Set Pelican-specific defaults
       static_website(
           exclude = ["archives.html", "authors.html", ...],
           mappings = {"theme/css": "theme/static/css", ...},
           **kwargs
       )
   ```

2. **Document Generator Requirements**: Create clear documentation about what a generator must do:
   - Accept content path as first argument
   - Output to the specified output path
   - Handle config files passed via the sources tarball

3. **Add More Generator Examples**: Once the interface is stable, add examples with other generators (Hugo, Jekyll, etc.) to verify true generator-agnostic design.

4. **Consider Environment Variables**: Some generators may need environment variables (like SITEURL in Pelican). The current design handles this, but it could be more explicit in documentation.
