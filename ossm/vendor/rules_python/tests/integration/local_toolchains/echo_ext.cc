#include <Python.h>

static PyObject *echoArgs(PyObject *self, PyObject *args) { return args; }

static PyMethodDef echo_methods[] = {
  { "echo", echoArgs, METH_VARARGS, "Returns a tuple of the input args" },
  { NULL, NULL, 0, NULL },
};

extern "C" {

PyMODINIT_FUNC PyInit_echo_ext(void) {
  static struct PyModuleDef echo_module_def = {
    // Module definition
    PyModuleDef_HEAD_INIT, "echo_ext", "'echo_ext' module", -1, echo_methods
  };

  return PyModule_Create(&echo_module_def);
}

}  // extern "C"
