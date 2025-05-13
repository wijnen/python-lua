#include <Python.h>

class Lua {
};
static PyObject *Lua_constructor(PyObject *self, PyObject *args) {
}

class Function {
};
static PyObject *Function_constructor(PyObject *self, PyObject *args) {
}

class Table {
};
static PyObject *Table_constructor(PyObject *self, PyObject *args) {
}

// Module registration. {{{
static PyMethodDef Methods[] = {
	{ "Lua", Lua_constructor, METH_VARARGS, "Main Lua class"},
	{ "Function", Function_constructor, METH_VARARGS, "Wrapped Lua Function"},
	{ "Table", Table_constructor, METH_VARARGS, "Wrapped Lua Table"},
	{ nullptr, nullptr, 0, nullptr }
};

static PyModuleDef Module = {
	PyModuleDef_HEAD_INIT,
	"lua",	// Module name
	nullptr,	// Documentation
	-1,	// State is held in global variables
	Methods	// Methods.
};

extern "C" {
	PyMODINIT_FUNC PyInit_lua() {
		return PyModule_Create(&Module);
	}
}
// }}}

// vim: set foldmethod=marker :
