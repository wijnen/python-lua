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

PyObject *Table_create(Lua *context) { // {{{
	Table *self = (Table *)(table_type->tp_alloc(table_type, 0));
	if (!self)
		return NULL;
	self->lua = context;
	Py_INCREF(self->lua);
	self->id = luaL_ref(self->lua->state, LUA_REGISTRYINDEX);
	//fprintf(stderr, "alloc table %p\n", self);
	return (PyObject *)self;
}; // }}}

// Destructor.
void Table_dealloc(Table *self) { // {{{
	//fprintf(stderr, "dealloc %p refcnt %ld\n", self, Py_REFCNT((PyObject *)self));
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
		if (!PyArg_ParseTuple(args, ""))
			return NULL;
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
			//fprintf(stderr, "Fail\n");
			//fprintf(stderr, "decref %p\n", ret);
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
		if (!PyArg_ParseTuple(args, ""))
			return NULL;
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
		lua_pop(target->lua->state, 1);
	}
	lua_pop(target->lua->state, 1);
	return ret;
} // }}}

static PyObject *remove_method(Table *self, PyObject *args) { // {{{
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	Py_ssize_t index = Table_len(self);
	if (!PyArg_ParseTuple(args, "|n", &index))
		return NULL;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_remove);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, index);
	lua_call(self->lua->state, 2, 1);
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_pop(self->lua->state, 1);
	return ret;
} // }}}

static PyObject *concat_method(Table *self, PyObject *args) { // {{{
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	const char *sep = "";
	Py_ssize_t seplen = 0, i = 1, j = Table_len(self);
	if (!PyArg_ParseTuple(args, "|s#nn", &sep, &seplen, &i, &j))
		return NULL;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_concat);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushlstring(self->lua->state, sep, seplen);
	lua_pushinteger(self->lua->state, i);
	lua_pushinteger(self->lua->state, j);
	lua_call(self->lua->state, 4, 1);
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_pop(self->lua->state, 1);
	return ret;
} // }}}

static PyObject *insert_method(Table *self, PyObject *args) { // {{{
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	Py_ssize_t length = PySequence_Length(args);
	Py_ssize_t pos;
	PyObject *value;
	switch (length) {
	case 1:
		if (!PyArg_ParseTuple(args, "O", &value))
			return NULL;
		pos = Table_len(self) + 1;
		break;
	case 2:
		if (!PyArg_ParseTuple(args, "nO", &pos, &value))
			return NULL;
		break;
	default:
		PyErr_SetString(PyExc_ValueError, "invalid argument for lua.Table.insert");
		return NULL;
	}
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_insert);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, pos);
	Lua_push(self->lua, value);
	lua_call(self->lua->state, 3, 0);
	lua_pop(self->lua->state, 1);
	Py_RETURN_NONE;
} // }}}

static PyObject *unpack_method(Table *self, PyObject *args) { // {{{
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	Py_ssize_t i = 1, j = Table_len(self);
	if (!PyArg_ParseTuple(args, "|nn", &i, &j))
		return NULL;
	int oldtop = lua_gettop(self->lua->state);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_unpack);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, i);
	lua_pushinteger(self->lua->state, j);
	lua_call(self->lua->state, 3, LUA_MULTRET);
	int newtop = lua_gettop(self->lua->state);
	PyObject *ret = PyTuple_New(newtop - oldtop);
	if (ret == NULL) {
		lua_settop(self->lua->state, oldtop);
		return NULL;
	}
	int p;
	for (p = oldtop + 1; p <= newtop; ++p)
		PyTuple_SET_ITEM(ret, p - oldtop - 1, Lua_to_python(self->lua, p));
	lua_settop(self->lua->state, oldtop);
	return ret;
} // }}}

static PyObject *move_method(Table *self, PyObject *args) { // {{{
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	PyObject *other = NULL;
	Py_ssize_t f, e, t;
	if (!PyArg_ParseTuple(args, "nnn|O!", &f, &e, &t,
				(PyObject *)table_type, &other))
		return NULL;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_move);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, f);
	lua_pushinteger(self->lua->state, e);
	lua_pushinteger(self->lua->state, t);
	if (other != NULL) {
		Lua_push(self->lua, other);
		lua_call(self->lua->state, 5, 1);
	} else {
		lua_call(self->lua->state, 4, 1);
	}
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_pop(self->lua->state, 1);
	return ret;
} // }}}

static PyObject *sort_method(Table *self, PyObject *args) { // {{{
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	PyObject *comp = NULL;
	if (!PyArg_ParseTuple(args, "|O", &comp))
		return NULL;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_sort);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	if (comp != NULL) {
		Lua_push(self->lua, comp);
		lua_call(self->lua->state, 2, 1);
	} else {
		lua_call(self->lua->state, 1, 1);
	}
	PyObject *ret = Lua_to_python(self->lua, -1);
	lua_pop(self->lua->state, 1);
	return ret;
} // }}}

static PyObject *pairs_method(Table *self, PyObject *args) { // {{{
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	return Table_iter_create((PyObject *)self, false);
} // }}}

static PyObject *ipairs_method(Table *self, PyObject *args) { // {{{
	//fprintf(stderr, "ipairs\n");
	if (!PyObject_IsInstance((PyObject *)self,
				(PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S",
				(PyObject *)self);
		return NULL;
	}
	return Table_iter_create((PyObject *)self, true);
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

// Iterator methods. {{{
PyObject *Table_iter_create(PyObject *target, bool is_ipairs) { // {{{
	TableIter *self = (TableIter *)
		(table_iter_type->tp_alloc(table_iter_type, 0));
	if (!self)
		return NULL;
	self->target = target;
	Py_INCREF(self);
	Py_INCREF(target);
	//fprintf(stderr, "new iter %p %p refcnt %ld, %ld\n", target, self,
	//		Py_REFCNT((PyObject *)self), Py_REFCNT(target));
	self->is_ipairs = is_ipairs;
	if (is_ipairs) {
		self->current = NULL;
		self->icurrent = 0;
	} else {
		self->current = Py_None;
		self->icurrent = -1;
	}
	return (PyObject *)self;
} // }}}

void Table_iter_dealloc(TableIter *self) { // {{{
	//fprintf(stderr, "dealloc iter %p / %p\n", self, self->target);
	if (self->target != NULL)
		Py_DECREF(self->target);
	if (self->current != NULL)
		Py_DECREF(self->current);
	table_iter_type->tp_free((PyObject *)self);
} // }}}

PyObject *Table_iter_repr(PyObject *self) { // {{{
	char str[100];
	snprintf(str, sizeof(str), "<lua.Table.iterator@%p>", self);
	return PyUnicode_DecodeUTF8(str, strlen(str), NULL);
} // }}}

PyObject *Table_iter_iter(PyObject *self) { // {{{
	//fprintf(stderr, "iter %p\n", self);
	return self;
} // }}}

PyObject *Table_iter_iternext(TableIter *self) { // {{{
	//fprintf(stderr, "next iter %p: %ld\n", self, Py_REFCNT(self));
	//if (!PyObject_IsInstance((PyObject *)self,
	//			(PyObject *)&table_iter_type)) {
	//	PyErr_SetString(PyExc_ValueError,
	//			"self must be a table iterator");
	//	return NULL;
	//}
	Lua *lua = ((Table *)(self->target))->lua;
	if (self->is_ipairs) {
		if (self->icurrent < 0)
			return NULL;
		self->icurrent += 1;
		Lua_push(lua, self->target);
		lua_geti(lua->state, -1, self->icurrent);
		if (lua_isnil(lua->state, -1)) {
			self->icurrent = -1;
			lua_pop(lua->state, 2);
			return NULL;
		}
		PyObject *ret = Lua_to_python(lua, -1);
		lua_pop(lua->state, 2);
		return Py_BuildValue("iN", self->icurrent, ret);
	}
	// pairs iterator.
	if (self->current == NULL)
		return NULL;
	Lua_push(lua, self->target);
	Lua_push(lua, self->current);
	if (lua_next(lua->state, -2) == 0) {
		lua_pop(lua->state, 1);
		Py_DECREF(self->current);
		self->current = NULL;
		return NULL;
	}
	PyObject *value = Lua_to_python(lua, -1);
	Py_DECREF(self->current);
	self->current = Lua_to_python(lua, -2);
	lua_pop(lua->state, 3);
	return Py_BuildValue("ON", self->current, value);
} // }}}
// }}}

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
	{"remove", (PyCFunction)remove_method, METH_VARARGS, "Remove item from Lua table"},
	{"concat", (PyCFunction)concat_method, METH_VARARGS, "Join items from Lua table into string"},
	{"insert", (PyCFunction)insert_method, METH_VARARGS, "Insert item into Lua table"},
	{"unpack", (PyCFunction)unpack_method, METH_VARARGS, "Extract items from Lua table into tuple"},
	{"move", (PyCFunction)move_method, METH_VARARGS, "Move item from Lua table to other index or table"},
	{"sort", (PyCFunction)sort_method, METH_VARARGS, "Sort item in Lua table"},
	{"pairs", (PyCFunction)pairs_method, METH_VARARGS, "Iterate over key-value pairs"},
	{"ipairs", (PyCFunction)ipairs_method, METH_VARARGS, "Iterate over index-value pairs"},
	{NULL, NULL, 0, NULL}
}; // }}}

PyTypeObject *table_type;
PyTypeObject *table_iter_type;

// vim: set foldmethod=marker :
