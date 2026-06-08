#include <Python.h>

#include "increment.h"

static PyObject *do_add(PyObject *self, PyObject *Py_UNUSED(args)) {
  return PyLong_FromLong(increment(1));
}

static PyMethodDef AdderMethods[] = {
    {"do_add", do_add, METH_NOARGS, "Add one"}, {NULL, NULL, 0, NULL}};

static struct PyModuleDef addermodule = {PyModuleDef_HEAD_INIT, "adder", NULL,
                                         -1, AdderMethods};

PyMODINIT_FUNC PyInit_adder(void) { return PyModule_Create(&addermodule); }
