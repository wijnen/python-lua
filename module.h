// lua.h - Use Lua in Python programs, (internal) header file.
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

/* Module documentation {{{

This module provides an interface between Python and Lua.

Objects of the Lua class contain the state of a Lua environment. Lua instances
do not share variables. Interactions of Lua objects from different environments
may not work; Lua's documentation isn't clear on that.

By default, the Lua constructor disables all potentially insecure features. To
enable them, set the corresponding argument to True. The features are:

	debug: debug library
		Not unsafe, but this should be disabled for production code, so
		it should only be enabled explicitly.
		Jailbreak: no.
		System damage: no.
		Privacy issue: no.

	loadlib: package.loadlib function
		It can load shared libraries from the system.
		Jailbreak: yes.
		System damage: yes.
		Privacy issue: yes.

	doloadfile: dofile and loadfile functions
		They access files on the file system.
		Jailbreak: no.
		System damage: no.
		Privacy issue: yes. (Very limited; only lua source can be run.)

	io: file read and write module
		The module accesses files on the file system.
		Jailbreak: no.
		System damage: yes.
		Privacy issue: yes.

	os: the os module, except for os.clock, os.date, os.difftime, os.setlocale and os.time
		It allows access to the os.
		Jailbreak: yes.
		System damage: yes.
		Privacy issue: yes.

lua = Lua()

After creating a Lua instance, it can be used to run a script either from a
string, or from a file. The script may be lua source, or compiled lua code.

Lua().run(source)
Lua().run_file(filename)

A variable in the Lua environment can be given a value using:

Lua().run(var = 'name', value = 'value')

When using run() to both set a variable and run code, the variable is set before
running the code.


While it is possible to access external code from Lua by setting a variable to
a function, the normal way to do it is through a module which is loaded with a
require statement. For this to work, the module must first be made available to
Lua. This is done using:

lua.module(name, object)

}}} */

// Includes. {{{
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <assert.h>
// }}}

typedef int bool;
#define true 1
#define false 0

enum Operator { // {{{
	// Binary operators.
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	POW,
	IDIV,
	AND,
	OR,
	XOR,
	LSHIFT,
	RSHIFT,
	CONCAT,
	EQ,
	LT,
	LE,

	// Unary operators
	CLOSE,
	NEG,
	NOT,
	LEN,
	TOSTRING,

	INDEX,
	NEWINDEX,
	CALL,

	NUM_OPERATORS
}; // }}}

struct OperatorMap { // {{{
	char const *lua_name;
	char const *lua_operator;
	PyObject *(*python_binary)(PyObject *lhs, PyObject *rhs);
	PyObject *(*python_unary)(PyObject *arg);
}; // }}}

extern struct OperatorMap operators[NUM_OPERATORS];

extern PyTypeObject *Lua_type;
extern PyTypeObject *function_type;
extern PyTypeObject *table_type;
extern PyTypeObject *table_iter_type;

typedef struct UserdataData {
	struct UserdataData *prev;
	struct UserdataData *next;
	PyObject *target;
} UserdataData;

typedef struct Lua { // {{{
	PyObject_HEAD
	// Linked list of userdata objects, for garbage collection.
	UserdataData *first_userdata;

	// Context for Lua environment.
	lua_State *state;

	// Doubly linked list of userdatas for garbage collection.
	UserdataData *first_data;

	// Copies of initial values of some globals, to use later regardless of them changing in Lua.
	lua_Integer table_remove;
	lua_Integer table_concat;
	lua_Integer table_insert;
	lua_Integer table_unpack;
	lua_Integer table_move;
	lua_Integer table_sort;
	lua_Integer package_loaded;

	// Stored Lua functions of all operators, to be used from Python calls on Lua-owned objects.
	// Values are Function objects.
	PyObject *lua_operator[NUM_OPERATORS];
} Lua; // }}}

#ifdef __cplusplus
extern "C" {
#endif
void lua_load_module(Lua *self, const char *name, PyObject *dict);
#ifdef __cplusplus
}
#endif

// Load variable from Lua stack into Python.
PyObject *Lua_to_python(Lua *self, int index);

// Load variable from Python onto Lua stack.
void Lua_push(Lua *self, PyObject *obj);

void Lua_dump_stack(Lua *self);


typedef struct Function { // {{{
	PyObject_HEAD

	// Context in which this object is defined.
	Lua *lua;

	// Registry index holding the Lua function.
	lua_Integer id;
} Function; // }}}

// Construct new function from value at top of stack.
PyObject *Function_create(Lua *context);
void Function_dealloc(PyObject *self);
PyObject *Function_call(PyObject *self, PyObject *args, PyObject *keywords);
PyObject *Function_repr(PyObject *self);


typedef struct Table { // {{{
	PyObject_HEAD

	// Registry index holding the Lua table for this object.
	lua_Integer id;

	// Context in which this object is defined.
	Lua *lua;
} Table; // }}}

// Construct new table from value at top of stack.
PyObject *Table_create(Lua *lua);
void Table_dealloc(PyObject *self);
PyObject *Table_repr(PyObject *self);
Py_ssize_t Table_len(PyObject *self);
PyObject *Table_getitem(PyObject *self, PyObject *key);
PyObject *Table_call(PyObject *self, PyObject *args, PyObject *kwargs);
int Table_contains(PyObject *self, PyObject *value);
PyObject *Table_add(PyObject *self, PyObject *args);
PyObject *Table_iadd(PyObject *self, PyObject *args);
PyObject *Table_sub(PyObject *self, PyObject *args);
PyObject *Table_mul(PyObject *self, PyObject *args);
PyObject *Table_div(PyObject *self, PyObject *args);
PyObject *Table_mod(PyObject *self, PyObject *args);
PyObject *Table_power(PyObject *self, PyObject *args, PyObject *mod);
PyObject *Table_idiv(PyObject *self, PyObject *args);
PyObject *Table_and(PyObject *self, PyObject *args);
PyObject *Table_or(PyObject *self, PyObject *args);
PyObject *Table_xor(PyObject *self, PyObject *args);
PyObject *Table_lshift(PyObject *self, PyObject *args);
PyObject *Table_rshift(PyObject *self, PyObject *args);
PyObject *Table_concat(PyObject *self, PyObject *args);
PyObject *Table_richcompare(PyObject *self, PyObject *args, int op);
PyObject *Table_neg(PyObject *self);
PyObject *Table_not(PyObject *self);
int Table_setitem(PyObject *self, PyObject *key, PyObject *value);
PyObject *table_list_method(PyObject *self, PyObject *args);
extern PyMethodDef Table_methods[];

// Class to iterate over table elements.
typedef struct TableIter { // {{{
	PyObject_HEAD

	// Object that we are iterating over, or NULL.
	PyObject *target;

	// Iteration type: pairs or ipairs.
	bool is_ipairs;

	// Current key for pairs iteration.
	PyObject *current;

	// Current key for ipairs iteration.
	int icurrent;
} TableIter; // }}}
PyObject *Table_iter_create(PyObject *target, bool is_ipairs);
void Table_iter_dealloc(PyObject *self);
PyObject *Table_iter_repr(PyObject *self);
PyObject *Table_iter_iter(PyObject *self);
PyObject *Table_iter_iternext(PyObject *self);

// For internal use only.
PyObject *Lua_run_code(Lua *self, int pos, bool keep_single, int nargs);

// Garbage collection helpers.
int Lua_traverse(PyObject *self, visitproc visit, void *arg);
int function_traverse(PyObject *self, visitproc visit, void *arg);
int table_traverse(PyObject *self, visitproc visit, void *arg);
int table_iter_traverse(PyObject *self, visitproc visit, void *arg);
int Lua_clear(PyObject *self);
int function_clear(PyObject *self);
int table_clear(PyObject *self);
int table_iter_clear(PyObject *self);

// vim: set foldmethod=marker :
