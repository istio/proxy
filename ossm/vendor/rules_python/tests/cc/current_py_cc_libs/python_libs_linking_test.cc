#include <Python.h>

int main(int argc, char** argv) {
  // Early return to prevent the broken code below from running.
  if (argc >= 1) {
    return 0;
  }

  // The below code won't actually run. We just reference some Python
  // symbols so the compiler and linker do some work to verify they are
  // able to resolve the symbols.
  // To make it actually run, more custom initialization is necessary.
  // See https://docs.python.org/3/c-api/intro.html#embedding-python
  Py_Initialize();
  Py_BytesMain(argc, argv);
  Py_Finalize();
  return 0;
}
