#include <Python.h>

int SomeFunction() {
  // Early return to prevent the broken code below from running.
  if (true) {
    return 0;
  }

  // The below code won't actually run. We just reference some Python
  // symbols so the compiler and linker do some work to verify they are
  // able to resolve the symbols.
  // To make it actually run, more custom initialization is necessary.
  // See https://docs.python.org/3/c-api/intro.html#embedding-python
  Py_Initialize();
  Py_Finalize();
  return 0;
}
