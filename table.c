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

#if 1
PyObject *Table_create(Lua *context) { // {{{
	Table *self = (Table *)(table_type->tp_alloc(table_type, 0));
	if (!self)
		return NULL;
	self->lua = context;
	Py_INCREF(self->lua);
	self->iter_ref = LUA_NOREF;
	self->id = luaL_ref(self->lua->state, LUA_REGISTRYINDEX);
	/*
	for (int i = 0; i < NUM_OPERATORS; ++i) {
		// Skip functions that are hardcoded into the type.
		if (i == STR || i == CLOSE)
			continue;
		fprintf(stderr, "setting method %s\n", operators[i].python_name);
		PyObject_SetAttrString((PyObject *)self, operators[i].python_name, context->lua_operator[i]);
	}*/
	return (PyObject *)self;
}; // }}}

// Destructor.
void Table_dealloc(Table *self) { // {{{
	luaL_unref(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Py_DECREF(self->lua);
	table_type->tp_free((PyObject *)self);
} // }}}

// Methods.
static PyObject *iadd_method(Table *self, PyObject *args) { // {{{
	PyObject *iterator;
	if (!PyArg_ParseTuple(args, "O", &iterator))	// borrowed reference.
		return NULL;
	if (!PyIter_Check(iterator)) {
		PyErr_SetString(PyExc_TypeError, "argument must be iterable");
		return NULL;
	}
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	lua_len(self->lua->state, -1);	// Length pushed.
	Py_ssize_t length = lua_tointeger(self->lua->state, -1);
	lua_pop(self->lua->state, 1);	// Length popped.
	PyObject *item;
	while ((item = PyIter_Next(iterator))) {
		Lua_push(self->lua, item);
		lua_seti(self->lua->state, -2, length + 1);
		length += 1;
		Py_DECREF(item);
	}
	Py_DECREF(iterator);
	lua_pop(self->lua->state, 1);
	return (PyObject *)self;
} // }}}

static PyObject *contains_method(Table *self, PyObject *args) { // {{{
	// Check if the argument exists in the table as a value (not a key).
	PyObject *value;
	if (!PyArg_ParseTuple(args, "O", &value))	// borrowed reference.
		return NULL;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	lua_pushnil(self->lua->state);
	while (lua_next(self->lua->state, -2) != 0) {
		PyObject *candidate = Lua_to_python(self->lua, -1);
		if (PyObject_RichCompareBool(value, candidate, Py_EQ)) {
			lua_pop(self->lua->state, 3);
			Py_DECREF(candidate);
			Py_RETURN_TRUE;
		}
		Py_DECREF(candidate);
		lua_pop(self->lua->state, 1);
	}
	lua_pop(self->lua->state, 2);
	Py_RETURN_FALSE;
} // }}}

/*static void clear_iter() { // {{{
	if (iter_ref != LUA_REFNIL)
		luaL_unref(lua->state, LUA_REGISTRYINDEX, iter_ref);
	iter_ref = LUA_NOREF;
} // }}}

static PyObject *iter_method(Table *self, PyObject *args) { // {{{
	// Destroy pending iteration, if any.
	if (self->iter_ref != LUA_NOREF)
		self->clear_iter();
	self->iter_ref = LUA_REFNIL;
	// TODO: the iterator should really be a copy of the object, to allow multiple iterations simultaneously.
	Py_INCREF(self);
	return (PyObject *)self;
} // }}}

static PyObject *next_method(Table *self, PyObject *args) { // {{{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	if (self->iter_ref == LUA_NOREF) {
		PyErr_SetString(PyExc_ValueError, "next called on invalid iterator");
		return NULL;
	}
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->iter_ref);	// previous key pushed.
	if (lua_next(self->lua->state, -2) == 0) {
		self->clear_iter();
		lua_pop(self->lua->state, 1);
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
	PyObject *ret = Lua_to_python(self->lua, -2);
	lua_pop(self->lua->state, 1);	// Pop value.
	if (self->iter_ref == LUA_REFNIL) {
		luaL_ref(self->lua->state, LUA_REGISTRYINDEX);	// Pop and store key.
		lua_pop(self->lua->state, 1);	// Pop table.
	}
	else {
		lua_pushinteger(self->lua->state, self->iter_ref);	// Push ref as key.
		lua_pushvalue(self->lua->state, -2);	// Push table key as value. Stack is now [table, key, ref, key]
		lua_settable(self->lua->state, LUA_REGISTRYINDEX);	// Store key in registry, pop ref and key.
		lua_pop(self->lua->state, 2);	// Pop table and key.
	}
	return ret;
} // }}} */

static PyObject *ne_method(Table *self, PyObject *args) { // {{{
	PyObject *other;
	if (!PyArg_ParseTuple(args, "O", &other))	// borrowed reference.
		return NULL;
	Lua_push(self->lua, self->lua->lua_operator[EQ]);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Lua_push(self->lua, other);
	lua_call(self->lua->state, 2, 1);
	PyObject *inverted = Lua_to_python(self->lua, -1);
	lua_pop(self->lua->state, 1);
	PyObject *ret = PyBool_FromLong(PyObject_Not(inverted));
	Py_DECREF(inverted);
	return ret;
} // }}}

static PyObject *gt_method(Table *self, PyObject *args) { // {{{
	PyObject *other;
	if (!PyArg_ParseTuple(args, "O", &other))	// borrowed reference.
		return NULL;
	Lua_push(self->lua, self->lua->lua_operator[LT]);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Lua_push(self->lua, other);
	lua_call(self->lua->state, 2, 1);
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_settop(self->lua->state, -1);
	return ret;
} // }}}

static PyObject *ge_method(Table *self, PyObject *args) { // {{{
	PyObject *other;
	if (!PyArg_ParseTuple(args, "O", &other))	// borrowed reference.
		return NULL;
	Lua_push(self->lua, self->lua->lua_operator[LE]);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Lua_push(self->lua, other);
	lua_call(self->lua->state, 2, 1);
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_settop(self->lua->state, -1);
	return ret;
} // }}}

static PyObject *dict_method(Table *self, PyObject *args) { // {{{
	// Get a copy of the table as a dict
	// This function can be called as a method on self, or standalone.
	// If called standalone, the target object is in args.
	Table *target;
	if (self == NULL) {
		if (!PyArg_ParseTuple(args, "O", (PyObject **)&target))
			return NULL;
		if (!PyObject_IsInstance((PyObject *)target,
					(PyObject *)table_type)) {
			PyErr_Format(PyExc_ValueError,
					"argument is not a Lua Table: %S",
					(PyObject *)target);
			return NULL;
		}
	} else {
		if (!PyArg_ParseTuple(args, "")) {
			PyErr_SetString(PyExc_ValueError,
					"No argument expected");
			return NULL;
		}
		target = self;
	}
	PyObject *ret = PyDict_New();
	lua_rawgeti(target->lua->state, LUA_REGISTRYINDEX, target->id);
	lua_pushnil(target->lua->state);
	while (lua_next(target->lua->state, -2) != 0) {
		PyObject *key = Lua_to_python(target->lua, -2);
		PyObject *value = Lua_to_python(target->lua, -1);
		bool fail = PyDict_SetItem(ret, key, value) < 0;
		Py_DECREF(key);
		Py_DECREF(value);
		if (fail) {
			printf("Fail\n");
			Py_DECREF(ret);
			return NULL;
		}
		lua_pop(target->lua->state, 1);
	}
	lua_pop(target->lua->state, 1);
	return ret;
} // }}}

PyObject *table_list_method(Table *self, PyObject *args) { // {{{
	// Get a copy of (the sequence elements of) the table, as a list. note that table[1] becomes list[0].
	// This function can be called as a method on self, or standalone.
	// If called standalone, the target object is in args.
	Table *target;
	if (self == NULL) {
		if (!PyArg_ParseTuple(args, "O", (PyObject **)&target))
			return NULL;
		if (!PyObject_IsInstance((PyObject *)target,
					(PyObject *)table_type)) {
			PyErr_Format(PyExc_ValueError,
					"argument is not a Lua Table: %S",
					(PyObject *)target);
			return NULL;
		}
	} else {
		if (!PyArg_ParseTuple(args, "")) {
			PyErr_SetString(PyExc_ValueError,
					"No argument expected");
			return NULL;
		}
		target = self;
	}
	lua_rawgeti(target->lua->state, LUA_REGISTRYINDEX, target->id);
	lua_len(target->lua->state, -1);
	Py_ssize_t length = lua_tointeger(target->lua->state, -1);
	lua_pop(target->lua->state, 1);
	PyObject *ret = PyList_New(length);
	for (Py_ssize_t i = 1; i <= length; ++i) {
		lua_rawgeti(target->lua->state, -1, i);
		PyObject *value = Lua_to_python(target->lua, -1);
		PyList_SET_ITEM(ret, i - 1, value);
		Py_DECREF(value);
		lua_pop(target->lua->state, 1);
	}
	lua_pop(target->lua->state, 1);
	return ret;
} // }}}

static PyObject *pop_method(Table *self, PyObject *args) { // {{{
	Py_ssize_t index = -1;
	if (!PyArg_ParseTuple(args, "|n", &index))
		return NULL;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	if (index < 0) {
		lua_len(self->lua->state, -1);
		index += lua_tointeger(self->lua->state, -1);
		lua_pop(self->lua->state, 1);
	}
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_remove);
	lua_pushinteger(self->lua->state, index);
	lua_call(self->lua->state, 1, 1);
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_pop(self->lua->state, 2);
	return ret;
} // }}}

Py_ssize_t Table_len(Table *self) { // {{{
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_len(self->lua->state, -1);
	lua_Number ret = lua_tonumber(self->lua->state, -1);
	lua_pop(self->lua->state, 2);
	return ret;
} // }}}

PyObject *Table_getitem(Table *self, PyObject *key) { // {{{
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	Lua_push(self->lua, key);	// Key pushed.
	lua_gettable(self->lua->state, -2);	// Key replaced by value.
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_pop(self->lua->state, 2);
	if (ret == Py_None) {
		PyErr_Format(PyExc_IndexError, "Key %S does not exist in Lua table", key);
		Py_DECREF(ret);
		return NULL;
	}
	return ret;
} // }}}

int Table_setitem(Table *self, PyObject *key, PyObject *value) { // {{{
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	Lua_push(self->lua, key);
	if (value == NULL)
		lua_pushnil(self->lua->state);
	else
		Lua_push(self->lua, value);
	lua_settable(self->lua->state, -3);	// Pops key and value from stack
	lua_pop(self->lua->state, 1);
	return 0;
} // }}}

PyObject *Table_repr(PyObject *self) { // {{{
	char str[100];
	snprintf(str, sizeof(str), "<lua.Table@%p>", self);
	return PyUnicode_DecodeUTF8(str, strlen(str), NULL);
} // }}}
// }}}

/*
class Table: # Using Lua tables from Python. {{{
	'''Python interface to access a lua table.
	The table is owned by lua; this class only provides a view to it.'''

	def __init__(self, lua, table = None): # {{{
		'''Create Python handle for Lua table.
		If the table is not given as an arugment, it must be pushed to
		the stack before calling this.'''
		_dprint('creating lua table', 1)
		self._lua = lua
		if table is not None:
			self._lua._push_luatable(table)
		self._id = luaL_ref(lua->state, LUA_REGISTRYINDEX)
		for name, impl in self._lua._ops.items():
			setattr(self, '__' + name + '__', impl)
	# }}}

# }}}
*/

// Python-accessible methods.
PyMethodDef Table_methods[] = { // {{{
	{"list", (PyCFunction)table_list_method, METH_VARARGS, "Create list from Lua table"},
	{"dict", (PyCFunction)dict_method, METH_VARARGS, "Create dict from Lua table"},
	//{"__len__", (PyCFunction)len_method, METH_VARARGS, "Call the # operator on the Lua table"},
	{"__iadd__", (PyCFunction)iadd_method, METH_VARARGS, "Call the += operator on the Lua table"},
	//{"__getitem__", (PyCFunction)getitem_method, METH_VARARGS, "Get item from Lua table"},
	//{"__setitem__", (PyCFunction)setitem_method, METH_VARARGS, "Set item in Lua table"},
	//{"__delitem__", (PyCFunction)delitem_method, METH_VARARGS, "Remove item from Lua table"},
	{"__contains__", (PyCFunction)contains_method, METH_VARARGS, "Check if Lua table contains item"},
	//{"__iter__", (PyCFunction)iter_method, METH_VARARGS, "Loop over the Lua table"},
	{"__ne__", (PyCFunction)ne_method, METH_VARARGS, "Call the ~= operator on the Lua table"},
	{"__gt__", (PyCFunction)gt_method, METH_VARARGS, "Call the > operator on the Lua table"},
	{"__ge__", (PyCFunction)ge_method, METH_VARARGS, "Call the >= operator on the Lua table"},
	{"pop", (PyCFunction)pop_method, METH_VARARGS, "Remove item from Lua table"},
	{NULL, NULL, 0, NULL}
}; // }}}

PyTypeObject *table_type;
#endif

// vim: set foldmethod=marker :
