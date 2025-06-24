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

#define DEBUG_ITER
#include "module.h"

static Table *check(PyObject *table) // {{{
{
	if (!PyObject_IsInstance(table, (PyObject *)table_type)) {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S", table);
		return NULL;
	}
	return (Table *)table;
} // }}}

static void setup_table(Table *self, Lua *context) // {{{
{
	self->lua = context;
	Py_INCREF(self->lua);
	self->id = luaL_ref(self->lua->state, LUA_REGISTRYINDEX);
} // }}}

PyObject *Table_create(Lua *context) // {{{
{
	Table *self = PyObject_GC_New(Table, table_type);
	setup_table(self, context);
	return (PyObject *)self;
} // }}}

// Destructor.
void Table_dealloc(PyObject *self_obj) { // {{{
	//fprintf(stderr, "dealloc %p refcnt %ld\n", self, Py_REFCNT((PyObject *)self));
	Table *self = check(self_obj);
	if (!self)
		return;
	luaL_unref(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Py_DECREF(self->lua);
	table_type->tp_free(self_obj);
} // }}}

// Methods.
PyObject *Table_repr(PyObject *self_obj) { // {{{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	char str[100];
	snprintf(str, sizeof(str), "<lua.Table@%p>", self);
	return PyUnicode_DecodeUTF8(str, strlen(str), NULL);
} // }}}

Py_ssize_t Table_len(PyObject *self_obj) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return -1;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_len(self->lua->state, -1);
	lua_Number ret = lua_tonumber(self->lua->state, -1);
	lua_pop(self->lua->state, 2);
	return ret;
} // }}}

PyObject *Table_getitem(PyObject *self_obj, PyObject *key) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
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

int Table_setitem(PyObject *self_obj, PyObject *key, PyObject *value) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return -1;
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

PyObject *Table_call(PyObject *self_obj, PyObject *args,
		PyObject *kwargs) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	if (kwargs != NULL) {
		PyErr_SetString(PyExc_ValueError,
				"Lua does not support keyword arguments.");
		return NULL;
	}
	if (!PySequence_Check(args)) {
		PyErr_SetString(PyExc_ValueError,
				"Non-sequence arguments to Table.call.");
		return NULL;
	}
	int pos = lua_gettop(self->lua->state);
	Lua_push(self->lua, (PyObject *)self);
	Py_ssize_t n = PySequence_Length(args);
	for (Py_ssize_t i = 0; i < n; ++i) {
		PyObject *item = PySequence_GetItem(args, i);
		if (item == NULL) {
			lua_pop(self->lua->state, i + 1);
			return NULL;
		}
		Lua_push(self->lua, item);
		Py_DECREF(item);
	}
	return Lua_run_code(self->lua, pos, false, n);
} // }}}

int Table_contains(PyObject *self_obj, PyObject *value) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return -1;
	// Check if the argument exists in the table as a value (not a key).

	// Push Table.
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);

	lua_pushnil(self->lua->state);
	while (lua_next(self->lua->state, -2) != 0) {
		PyObject *candidate = Lua_to_python(self->lua, -1);
		int cmp = PyObject_RichCompareBool(value, candidate, Py_EQ);
		Py_DECREF(candidate);
		if (cmp == 1) {
			lua_pop(self->lua->state, 3);
			return 1;
		} else if (cmp == -1) {
			lua_pop(self->lua->state, 3);
			return -1;
		}
		lua_pop(self->lua->state, 1);
	}
	lua_pop(self->lua->state, 1);
	return 0;
} // }}}

static PyObject *Table_binary_operator(PyObject *self_obj, PyObject *value,
		enum Operator operator) // {{{
{
	bool reverse;
	Table *self;
	PyObject *other;
	if (PyObject_IsInstance(self_obj, (PyObject *)table_type)) {
		self = (Table *)self_obj;
		other = value;
		reverse = false;
	} else if (PyObject_IsInstance(value, (PyObject *)table_type)) {
		self = (Table *)value;
		other = self_obj;
		reverse = true;
	} else {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S", self_obj);
		return NULL;
	}
	int pos = lua_gettop(self->lua->state);
	// Push function.
	Lua_push(self->lua, self->lua->lua_operator[operator]);
	// Push arguments.
	if (!reverse) {
		lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
		Lua_push(self->lua, other);
	} else {
		Lua_push(self->lua, other);
		lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	}
	return Lua_run_code(self->lua, pos, false, 2);
} // }}}

static PyObject *Table_unary_operator(PyObject *self_obj,
		enum Operator operator) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	int pos = lua_gettop(self->lua->state);
	// Push function.
	Lua_push(self->lua, self->lua->lua_operator[operator]);
	// Push argument.
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	return Lua_run_code(self->lua, pos, false, 1);
} // }}}

PyObject *Table_add(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, ADD); }
PyObject *Table_sub(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, SUB); }
PyObject *Table_mul(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, MUL); }
PyObject *Table_mod(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, MOD); }
PyObject *Table_div(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, DIV); }
PyObject *Table_power(PyObject *self_obj, PyObject *args, PyObject * /*mod*/)
	{ return Table_binary_operator(self_obj, args, POW); }
PyObject *Table_idiv(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, IDIV); }
PyObject *Table_and(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, AND); }
PyObject *Table_or(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, OR); }
PyObject *Table_xor(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, XOR); }
PyObject *Table_lshift(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, LSHIFT); }
PyObject *Table_rshift(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, RSHIFT); }
PyObject *Table_concat(PyObject *self_obj, PyObject *args)
	{ return Table_binary_operator(self_obj, args, CONCAT); }

PyObject *Table_neg(PyObject *self_obj)
	{ return Table_unary_operator(self_obj, NEG); }
PyObject *Table_not(PyObject *self_obj)
	{ return Table_unary_operator(self_obj, NOT); }

PyObject *Table_iadd(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	PyObject *value;
	if (!PyArg_ParseTuple(args, "O", &value))	// borrowed reference.
		return NULL;
	PyObject *iterator = PyObject_GetIter(value);
	if (!iterator)
		return NULL;
	// Push Table.
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
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

PyObject *Table_richcompare(PyObject *lhs, PyObject *rhs, int op) // {{{
{
	bool reverse;
	Table *self;
	PyObject *other;
	if (PyObject_IsInstance(lhs, (PyObject *)table_type)) {
		self = (Table *)lhs;
		other = rhs;
		reverse = false;
	} else if (PyObject_IsInstance(rhs, (PyObject *)table_type)) {
		self = (Table *)rhs;
		other = lhs;
		reverse = true;
	} else {
		PyErr_Format(PyExc_ValueError,
				"argument is not a Lua Table: %S; %S",
				lhs, rhs);
		return NULL;
	}
	if (reverse) {
		switch(op) {
		case Py_LT:
			op = Py_GT;
			break;
		case Py_LE:
			op = Py_GE;
			break;
		case Py_GT:
			op = Py_LT;
			break;
		case Py_GE:
			op = Py_LE;
			break;
		case Py_EQ:
		case Py_NE:
		default:
			break;
		}
	}
	bool invert;
	enum Operator lua_op;
	switch(op) {
	case Py_LT:
		lua_op = LT;
		invert = false;
		break;
	case Py_LE:
		lua_op = LE;
		invert = false;
		break;
	case Py_GT:
		lua_op = LE;
		invert = true;
		break;
	case Py_GE:
		lua_op = LT;
		invert = true;
		break;
	case Py_EQ:
		lua_op = EQ;
		invert = false;
		break;
	case Py_NE:
		lua_op = EQ;
		invert = true;
		break;
	default:
		PyErr_SetString(PyExc_ValueError, "Invalid operator.");
		return NULL;
	}
	int pos = lua_gettop(self->lua->state);
	// Push function.
	Lua_push(self->lua, self->lua->lua_operator[lua_op]);
	// Push arguments.
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Lua_push(self->lua, other);
	PyObject *result = Lua_run_code(self->lua, pos, false, 2);
	if (!result)
		return NULL;
	if (!invert)
		return result;
	int ret = PyObject_Not(result);
	Py_DECREF(result);
	if (ret < 0)
		return NULL;
	if (ret)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
} // }}}

static PyObject *dict_method(PyObject *self, PyObject *args) // {{{
{
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
		target = check(self);
		if (!target)
			return NULL;
	}
	PyObject *ret = PyDict_New();
	lua_rawgeti(target->lua->state, LUA_REGISTRYINDEX, target->id);
	lua_pushnil(target->lua->state);
	while (lua_next(target->lua->state, -2) != 0) {
		Lua_dump_stack(target->lua);
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

PyObject *table_list_method(PyObject *self, PyObject *args) // {{{
{
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
		target = check(self);
		if (!target)
			return NULL;
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

static PyObject *remove_method(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	Py_ssize_t index = Table_len(self_obj);
	if (!PyArg_ParseTuple(args, "|n", &index))
		return NULL;
	int pos = lua_gettop(self->lua->state);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_remove);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, index);
	return Lua_run_code(self->lua, pos, false, 2);
} // }}}

static PyObject *insert_method(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	Py_ssize_t length = PySequence_Length(args);
	Py_ssize_t pos;
	PyObject *value;
	switch (length) {
	case 1:
		if (!PyArg_ParseTuple(args, "O", &value))
			return NULL;
		pos = Table_len(self_obj) + 1;
		break;
	case 2:
		if (!PyArg_ParseTuple(args, "nO", &pos, &value))
			return NULL;
		break;
	default:
		PyErr_SetString(PyExc_ValueError, "Invalid argument for lua.Table.insert");
		return NULL;
	}
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_insert);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, pos);
	Lua_push(self->lua, value);
	if (lua_pcall(self->lua->state, 3, 0, 0) != LUA_OK) {
		PyErr_Format(PyExc_RuntimeError,
				"Error running Table.insert: %s",
				lua_tostring(self->lua->state, -1));
		lua_pop(self->lua->state, 1);
		return NULL;
	}
	Py_RETURN_NONE;
} // }}}

static PyObject *unpack_method(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	Py_ssize_t i = 1, j = Table_len(self_obj);
	if (!PyArg_ParseTuple(args, "|nn", &i, &j))
		return NULL;
	int oldtop = lua_gettop(self->lua->state);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_unpack);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, i);
	lua_pushinteger(self->lua->state, j);
	if (lua_pcall(self->lua->state, 3, LUA_MULTRET, 0) != LUA_OK) {
		PyErr_Format(PyExc_RuntimeError,
				"Error running Table.unpack: %s",
				lua_tostring(self->lua->state, -1));
		lua_pop(self->lua->state, 1);
		return NULL;
	}
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

static PyObject *move_method(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	PyObject *other = NULL;
	Py_ssize_t f, e, t;
	if (!PyArg_ParseTuple(args, "nnn|O!", &f, &e, &t,
				(PyObject *)table_type, &other))
		return NULL;
	int pos = lua_gettop(self->lua->state);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_move);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushinteger(self->lua->state, f);
	lua_pushinteger(self->lua->state, e);
	lua_pushinteger(self->lua->state, t);
	if (other != NULL) {
		Lua_push(self->lua, other);
		return Lua_run_code(self->lua, pos, false, 5);
	} else {
		return Lua_run_code(self->lua, pos, false, 4);
	}
} // }}}

static PyObject *sort_method(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	PyObject *comp = NULL;
	if (!PyArg_ParseTuple(args, "|O", &comp))
		return NULL;
	int pos = lua_gettop(self->lua->state);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->lua->table_sort);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	if (comp != NULL) {
		Lua_push(self->lua, comp);
		return Lua_run_code(self->lua, pos, false, 2);
	} else {
		return Lua_run_code(self->lua, pos, false, 1);
	}
} // }}}

static PyObject *pairs_method(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	return Table_iter_create((PyObject *)self, false);
} // }}}

static PyObject *ipairs_method(PyObject *self_obj, PyObject *args) // {{{
{
	Table *self = check(self_obj);
	if (!self)
		return NULL;
	//fprintf(stderr, "ipairs\n");
	return Table_iter_create((PyObject *)self, true);
} // }}}

// Iterator methods. {{{
PyObject *Table_iter_create(PyObject *target, bool is_ipairs) { // {{{
	TableIter *self = PyObject_GC_New(TableIter, table_iter_type);
	if (!self)
		return NULL;
	self->target = target;
	Py_INCREF(self);
	Py_INCREF(target);
#ifdef DEBUG_ITER
	fprintf(stderr, "new iter %p %p refcnt %ld, %ld\n", target, self,
			Py_REFCNT((PyObject *)self), Py_REFCNT(target));
#endif
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

void Table_iter_dealloc(PyObject *py_self) { // {{{
	TableIter *self = (TableIter *)py_self;
#ifdef DEBUG_ITER
	fprintf(stderr, "dealloc iter %p / %p\n", self, self->target);
#endif
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
#ifdef DEBUG_ITER
	fprintf(stderr, "iter %p\n", self);
#endif
	return self;
} // }}}

PyObject *Table_iter_iternext(PyObject *py_self) { // {{{
#ifdef DEBUG_ITER
	fprintf(stderr, "next iter %p: %ld\n", py_self, Py_REFCNT(py_self));
#endif
	if (!PyObject_IsInstance(py_self, (PyObject *)table_iter_type)) {
		PyErr_SetString(PyExc_ValueError,
				"self must be a table iterator");
		return NULL;
	}
	TableIter *self = (TableIter *)py_self;
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

// Garbage collection helpers.
int table_traverse(PyObject *self, visitproc visit, void *arg) // {{{
{
	PyObject_VisitManagedDict((PyObject*)self, visit, arg);
	Table *table = (Table *)self;
	Py_VISIT(table->lua);
	// Visit type.
	Py_VISIT(Py_TYPE(self));
	return 0;
} // }}}

int table_iter_traverse(PyObject *self, visitproc visit, void *arg) // {{{
{
	PyObject_VisitManagedDict((PyObject*)self, visit, arg);
	TableIter *iter = (TableIter *)self;
	Py_VISIT(iter->target);
	Py_VISIT(iter->current);
	// Visit type.
	Py_VISIT(Py_TYPE(self));
	return 0;
} // }}}

int table_clear(PyObject *self) // {{{
{
	PyObject_ClearManagedDict(self);
	Table *table = (Table *)self;
	Py_CLEAR(table->lua);
	return 0;
} // }}}

int table_iter_clear(PyObject *self) // {{{
{
	PyObject_ClearManagedDict(self);
	TableIter *iter = (TableIter *)self;
	Py_CLEAR(iter->target);
	Py_CLEAR(iter->current);
	return 0;
} // }}}


// Python-accessible methods.
PyMethodDef Table_methods[] = { // {{{
	{"list", (PyCFunction)table_list_method, METH_VARARGS, "Create list from Lua table"},
	{"dict", (PyCFunction)dict_method, METH_VARARGS, "Create dict from Lua table"},
	{"remove", (PyCFunction)remove_method, METH_VARARGS, "Remove item from Lua table"},
	{"concat", (PyCFunction)Table_concat, METH_VARARGS, "Join items from Lua table into string"},
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
