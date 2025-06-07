// lua.py - Use Lua in Python programs
/* Copyright 2012-2023 Bas Wijnen <wijnen@debian.org> {{{
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * }}} */

#include "module.h"

// Create new Function from value at top of stack.
PyObject *Function_create(Lua *context) { // {{{
	Function *self = (Function *)(function_type->tp_alloc(function_type, 0));
	if (!self)
		return NULL;
	self->lua = context;
	Py_INCREF((PyObject *)(self->lua));

	// Wrap function at top of lua stack,
	// which must have been pushed before calling this.
	self->id = luaL_ref(self->lua->state, LUA_REGISTRYINDEX);

	return (PyObject *)self;
}; // }}}

// Destructor.
void Function_dealloc(Function *self) { // {{{
	luaL_unref(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Py_DECREF(self->lua);
	function_type->tp_free((PyObject *)self);
} // }}}

PyObject *Function_call(Function *self, PyObject *args,
		PyObject *keywords) { // {{{

	// Parse keep_single argument. {{{
	bool keep_single = false;
	if (keywords) {
		PyObject *kw = PyDict_GetItemString(keywords, "keep_single");	// Returns borrowed reference.
		Py_ssize_t num_keywords = PyDict_Size(keywords);
		if (kw) {
			if (!PyBool_Check(kw)) {
				PyErr_SetString(PyExc_ValueError, "keep_single argument must be of bool type");
				return NULL;
			}
			keep_single = kw == Py_True;
			num_keywords -= 1;
		}
		if (num_keywords > 0) {
			PyErr_SetString(PyExc_ValueError, "only keep_single is supported as a keyword argument");
			return NULL;
		}
	}
	// }}}

	int pos = lua_gettop(self->lua->state);

	// Push target function to stack.
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);

	// Push arguments to stack. {{{
	assert(PyTuple_Check(args));
	Py_ssize_t nargs = PyTuple_Size(args);
	for (Py_ssize_t a = 0; a < nargs; ++a) {
		PyObject *arg = PyTuple_GetItem(args, a);	// Borrowed reference.
		Lua_push(self->lua, arg);
	}
	// }}}

	// Call function.
	return Lua_run_code(self->lua, pos, keep_single, nargs);
} // }}}

PyObject *Function_repr(PyObject *self) { // {{{
	char str[100];
	snprintf(str, sizeof(str), "<lua.Function@%p>", self);
	return PyUnicode_DecodeUTF8(str, strlen(str), NULL);
} // }}}
// }}}

// Python-accessible methods.
//PyMethodDef function_methods[] = { // {{{
//	{"__call__", (PyCFunction)Function_call, METH_VARARGS | METH_KEYWORDS, "Call the Lua function"},
//	{NULL, NULL, 0, NULL}
//}; // }}}

PyTypeObject *function_type;

// vim: set foldmethod=marker :
