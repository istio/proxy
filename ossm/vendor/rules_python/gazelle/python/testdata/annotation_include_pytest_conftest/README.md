# Annotation: Include Pytest Conftest

Validate that the `# gazelle:include_pytest_conftest` annotation follows
this logic:

+ When a `conftest.py` file does not exist:
  + all values have no affect
+ When a `conftest.py` file does exist:
  + Truthy values add `:conftest` to `deps`.
  + Falsey values do not add `:conftest` to `deps`.
  + Unset (no annotation) performs the default action.

Additionally, we test that:

+ invalid values (eg `foo`) print a warning and then act as if
  the annotation was not present.
+ last annotation (highest line number) wins.
+ the annotation has no effect on non-test files/targets.
+ the `include_dep` can still inject `:conftest` even when `include_pytest_conftest`
  is false.
+ `import conftest` will still add the dep even when `include_pytest_conftest` is
  false.

An annotation without a value is not tested, as that's part of the core
annotation framework and not specific to this annotation.
