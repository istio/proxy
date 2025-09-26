const {join, dirname} = require('path')
const {runfiles} = require('@bazel/runfiles');

describe('runfile resolution', () => {
  it('should properly resolve the "test_fixture.md" file', () => {
    const testFixturePath =
        runfiles.resolve('rules_nodejs/packages/runfiles/test/test_fixture.md');
    const expectedPath = join(__dirname, 'test_fixture.md');

    expect(normalizePath(testFixturePath))
        .toEqual(
            normalizePath(expectedPath),
            'Expected the test fixture to be resolved next to the spec source file.');
  });

  it('should properly resolve with forward slashes', () => {
    const testFixturePath =
        runfiles.resolve('rules_nodejs\\packages\\runfiles\\test\\test_fixture.md');
    const expectedPath = join(__dirname, 'test_fixture.md');

    expect(normalizePath(testFixturePath))
        .toEqual(
            normalizePath(expectedPath),
            'Expected the test fixture to be resolved next to the spec source file.');
  });

  it('should properly resolve a subdirectory of a runfile', () => {
    const packagePath = runfiles.resolve('rules_nodejs/packages');
    // Alternate with trailing slash
    const packagePath2 = runfiles.resolve('rules_nodejs/packages/');
    const expectedPath = dirname(dirname(dirname(runfiles.resolve(
        'rules_nodejs/packages/runfiles/test/test_fixture.md.generated_file_suffix'))));

    expect(normalizePath(packagePath))
        .toEqual(normalizePath(expectedPath), 'Expected to resolve a subdirectory of a runfile.');
    expect(normalizePath(packagePath2))
        .toEqual(normalizePath(expectedPath), 'Expected to resolve a subdirectory of a runfile.');
  });

  it('should properly resolve the workspace root of a runfile', () => {
    const packagePath = runfiles.resolve('rules_nodejs');
    // Alternate with trailing slash
    const packagePath2 = runfiles.resolve('rules_nodejs/');
    const expectedPath = dirname(dirname(dirname(dirname(runfiles.resolve(
        'rules_nodejs/packages/runfiles/test/test_fixture.md.generated_file_suffix')))));

    expect(normalizePath(packagePath))
        .toEqual(
            normalizePath(expectedPath), 'Expected to resolve the workspace root of a runfile.');
    expect(normalizePath(packagePath2))
        .toEqual(
            normalizePath(expectedPath), 'Expected to resolve the workspace root of a runfile.');
  });
});

/**
 * Normalizes the delimiters within the specified path. This is useful for test assertions
 * where paths might be computed using different path delimiters.
 */
function normalizePath(value) {
  return value.replace(/\\/g, '/');
}
