// lua.c - Use Lua in Python programs
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

struct OperatorMap operators[NUM_OPERATORS] = { // {{{
	{ "__add__", "__add", "+" },
	{ "__sub__", "__sub", "-" },
	{ "__mul__", "__mul", "*" },
	{ "__truediv__", "__div", "/" },
	{ "__mod__", "__mod", "%" },
	{ "__pow__", "__pow", "^" },
	{ "__floordiv__", "__idiv", "//" },
	{ "__and__", "__band", "&" },
	{ "__or__", "__bor", "|" },
	{ "__xor__", "__bxor", "~" },
	{ "__lshift__", "__shl", "<<" },
	{ "__rshift__", "__shr", ">>" },
	{ "__matmul__", "__concat", ".." },
	{ "__eq__", "__eq", "==" },
	{ "__lt__", "__lt", "<" },
	{ "__le__", "__le", "<=" },
	{ "__close__", "__close", NULL },
	{ "__neg__", "__unm", NULL },
	{ "__invert__", "__bnot", NULL },
	{ "__len__", "__len", NULL },
	{ "__getitem__", "__index", NULL },
	{ "__setitem__", "__newindex", NULL },
	//{ "__call__", "__call", NULL },
	{ "__repr__", "__totring", NULL }
}; // }}}

// Lua callback for all userdata metamethods. (Method selection is done via operator name stored in upvalue.)
// Userdata metamethods are called when Lua code uses an operator on a Python-owned object.
static int metamethod(lua_State *state) { // {{{
	// This function is called from Lua through a metatable.
	// Get self pointer.
	lua_getfield(state, LUA_REGISTRYINDEX, "self");
	Lua *lua = (Lua *)lua_touserdata(state, -1);
	lua_pop(state, 1);

	// 1 upvalue: python method index.
	int method_index = lua_tointeger(state, lua_upvalueindex(1));
	fprintf(stderr, "calling method %s\n", operators[method_index].python_name);

	PyObject *target = Lua_to_python(lua, 1);
	PyObject *method = PyMapping_GetItemString(target, operators[method_index].python_name);

	if (!method) {
		PyErr_Clear();
		lua_pop(state, 1);
		return 0;
	}

	// Create arguments tuple
	int nargs = lua_gettop(state);
	PyObject *args = PyTuple_New(nargs - 1);
	for (int i = 2; i <= nargs; ++i) {
		PyObject *arg = Lua_to_python(lua, i);	// New reference.
		// fill tuple
		PyTuple_SET_ITEM(args, i - 2, arg);	// Steals reference to arg.
	}

	// Call function.
	PyObject *ret = PyObject_CallObject(method, args);
	Lua_push(lua, ret);	// Unpack returned sequence?
	Py_DECREF(ret);
	Py_DECREF(args);
	return 1;
} // }}}

static int gc(lua_State *state) { // {{{
	lua_getfield(state, LUA_REGISTRYINDEX, "self");
	PyObject *lua = (PyObject *)lua_touserdata(state, -1);
	lua_pop(state, 1);
	
	// Drop (this reference to) the object.
	Py_DECREF(lua);

	return 0;
} // }}}

static lua_Integer build_wrapper(Lua *self, char const *code, char const *desc, bool make_ref) { // {{{
	if (luaL_loadbuffer(self->state, code, strlen(code), desc ? desc : code) != LUA_OK) {
		lua_close(self->state);
		PyErr_SetString(PyExc_ValueError, lua_tostring(self->state, -1));
		return LUA_NOREF;
	}
	lua_call(self->state, 0, 1);
	if (!make_ref)
		return LUA_REFNIL;
	return luaL_ref(self->state, LUA_REGISTRYINDEX);
} // }}}

// Create (native) Lua table on Lua stack from items in Python list or dict.
static void push_luatable_dict(Lua *self, PyObject *obj) { // {{{
	Py_ssize_t ppos = 0;
	PyObject *key;
	PyObject *value;
	while (PyDict_Next(obj, &ppos, &key, &value)) {	// Key and value become borrowed references.
		Lua_push(self, key);
		Lua_push(self, value);
		lua_rawset(self->state, -3); // Pops key and value from the stack.
	}
} // }}}

static void push_luatable_list(Lua *self, PyObject *obj) { // {{{
	Py_ssize_t size = PySequence_Size(obj);
	for (Py_ssize_t i = 0; i < size; ++i) {
		PyObject *value = PySequence_GetItem(obj, i);	// New reference.
		Lua_push(self, value);
		Py_DECREF(value);
		lua_rawseti(self->state, -2, i + 1);
	}
} // }}}

static PyObject *construct_bytes_impl(PyObject * /*self*/, PyObject *args) // {{{
{
	// Implementation of python.bytes()
	PyObject *arg;
	if (!PyArg_ParseTuple(args, "O", &arg))
		return NULL;
	if (PyObject_IsInstance(arg, (PyObject*)table_type)) {
		PyObject *list = table_list_method((Table *)arg, NULL);
		return PyBytes_FromObject(list);
	}
	if (PyUnicode_Check(arg))
		return PyUnicode_AsUTF8String(arg);
	return PyBytes_FromObject(arg);
}
static PyMethodDef construct_bytes_method = {
	"constuct_bytes",
	(PyCFunction)construct_bytes_impl,
	METH_VARARGS,
	"Convert argument to bytes"
}; // }}}

static int call_method(lua_State *state) // {{{
{
	lua_getfield(state, LUA_REGISTRYINDEX, "self");
	Lua *lua = (Lua *)lua_touserdata(state, -1);
	lua_pop(state, 1);
	PyObject *target = Lua_to_python(lua, 1);
	if (!PyCallable_Check(target)) {
		fprintf(stderr, "Called object is not callable: ");
		PyObject_Print(target, stderr, 0);
		fprintf(stderr, "\n");
		lua_settop(state, 0);
		return 0;
	}
	// Create arguments tuple
	int nargs = lua_gettop(state);
	PyObject *args = PyTuple_New(nargs - 1);
	for (int i = 2; i <= nargs; ++i) {
		PyObject *arg = Lua_to_python(lua, i);	// New reference.
		// fill tuple
		PyTuple_SET_ITEM(args, i - 2, arg);	// Steals reference to arg.
	}

	// Call function.
	PyObject *ret = PyObject_CallObject(target, args);
	Py_DECREF(args);
	if (ret == NULL)
		return 0;
	Lua_push(lua, ret);	// Unpack returned sequence?
	Py_DECREF(ret);
	return 1;
} // }}}

// Python class Lua __new__ function.
static PyObject *Lua_new(PyTypeObject *type, PyObject *args, PyObject *keywords) { // {{{
	Lua *self = (Lua *)type->tp_alloc(type, 0);
	if (!self)
		return NULL;
	// Create a new lua object.
	// This object provides the interface into the lua library.
	// It also provides access to all the symbols that lua owns.

	// Parse arguments. {{{
	bool debug = false;
	bool loadlib = false;
	bool searchers = false;
	bool doloadfile = false;
	bool io = false;
	bool os = false;
	bool python_module = true;
	char const *keywordnames[] = {"debug", "loadlib", "searchers", "doloadfile", "io", "os", "python_module", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, keywords, "|ppppppp", (char **)keywordnames, &debug, &loadlib, &searchers, &doloadfile, &io, &os, &python_module))
		return NULL;
	// }}}

	// Create new state and store back pointer to self. {{{
	self->state = luaL_newstate();
	lua_pushlightuserdata(self->state, self);
	lua_setfield(self->state, LUA_REGISTRYINDEX, "self");
	// }}}

	// Open standard libraries. Many of them are closed again below.
	luaL_openlibs(self->state);

	// Create metatable and store operators (because the API does not define them). {{{
	lua_createtable(self->state, 0, NUM_OPERATORS + 1);
	for (int i = 0; i < NUM_OPERATORS; ++i) {
		// Create metatable for table wrappers.
		lua_pushinteger(self->state, i);
		lua_pushcclosure(self->state, metamethod, 1);
		lua_setfield(self->state, -2, operators[i].lua_name);

		// Store binary operators. (Others are stored below this loop.)
		if (!operators[i].lua_operator)
			continue;
		char const *template = "return function(a, b) return a %s b end";
		char buffer[strlen(template) + 2];
		sprintf(buffer, template, operators[i].lua_operator);
		lua_Integer function_id = build_wrapper(self, buffer, NULL, true);
		if (function_id == LUA_NOREF)
			return NULL;
		lua_pushinteger(self->state, function_id);
		self->lua_operator[i] = Function_create(self);
	}

	//Lua_dump_stack(self);

	// Unary operators.
	lua_Integer index = build_wrapper(self, "return function(a) return -a end", NULL, true);
	if (index == LUA_NOREF)
		return NULL;
	lua_pushinteger(self->state, index);
	self->lua_operator[NEG] = Function_create(self);

	index = build_wrapper(self, "return function(a) return ~a end", NULL, true);
	if (index == LUA_NOREF)
		return NULL;
	lua_pushinteger(self->state, index);
	self->lua_operator[NOT] = Function_create(self);

	index = build_wrapper(self, "return function(a) return tostring(a) end", NULL, true);
	if (index == LUA_NOREF)
		return NULL;
	lua_pushinteger(self->state, index);
	self->lua_operator[STR] = Function_create(self);

	// Skipped: len, getitem, setitem, delitem, because they have API calls which are used.

	// Set call metamethod.
	lua_pushcclosure(self->state, call_method, 0);
	lua_setfield(self->state, -2, "__call");

	// Set gc metamethod. This is not passed through to Python, but instead cleans up the PyObject * reference.
	lua_pushcclosure(self->state, gc, 0);
	lua_setfield(self->state, -2, "__gc");

	// Set the metatable in the registry, for use by new userdata objects.
	lua_setfield(self->state, LUA_REGISTRYINDEX, "metatable");
	// }}}

	// Store a copy of some initial values, so they still work if the original value is replaced.
	self->table_remove = build_wrapper(self, "return table.remove", "get table.remove", true);
	if (self->table_remove == LUA_NOREF)
		return NULL;
	self->table_concat = build_wrapper(self, "return table.concat", "get table.concat", true);
	if (self->table_concat == LUA_NOREF)
		return NULL;
	self->table_insert = build_wrapper(self, "return table.insert", "get table.insert", true);
	if (self->table_insert == LUA_NOREF)
		return NULL;
	self->table_unpack = build_wrapper(self, "return table.unpack", "get table.unpack", true);
	if (self->table_unpack == LUA_NOREF)
		return NULL;
	self->table_move = build_wrapper(self, "return table.move", "get table.move", true);
	if (self->table_move == LUA_NOREF)
		return NULL;
	self->table_sort = build_wrapper(self, "return table.sort", "get table.sort", true);
	if (self->table_sort == LUA_NOREF)
		return NULL;
	self->package_loaded = build_wrapper(self, "return package.loaded", "get package.loaded", true);
	if (self->package_loaded == LUA_NOREF)
		return NULL;

	// Disable optional features that have not been requested. {{{
	if (!debug) {
		if (build_wrapper(self, "debug = nil package.loaded.debug = nil", "disabling debug", false) == LUA_NOREF)
			return NULL;
	}
	if (!loadlib) {
		if (build_wrapper(self, "package.loadlib = nil", "disabling loadlib", false) == LUA_NOREF)
			return NULL;
	}
	if (!searchers) {
		if (build_wrapper(self, "package.searchers = {}", "disabling searchers", false) == LUA_NOREF)
			return NULL;
	}
	if (!doloadfile) {
		if (build_wrapper(self, "loadfile = nil dofile = nil", "disabling loadfile and dofile", false) == LUA_NOREF)
			return NULL;
	}
	if (!os) {
		if (build_wrapper(self, "os = {clock = os.clock, date = os.date, difftime = os.difftime, setlocale = os.setlocale, time = os.time} package.loaded.os = os", "disabling some of os", false) == LUA_NOREF)
			return NULL;
	}
	if (!io) {
		if (build_wrapper(self, "io = nil package.loaded.io = nil", "disabling io", false) == LUA_NOREF)
			return NULL;
	}
	// }}}

	/* Add access to Python object constructors from Lua (unless disabled).
	   TODO when Table interface has been implemented.*/
	if (python_module) {
		// Lua module for accessing some Python parts from Lua. This is prepared as a "python" module unless disabled. {{{
		PyObject *table_list = PyCMethod_New(&Table_methods[0],
				NULL, NULL, NULL);
		PyObject *table_dict = PyCMethod_New(&Table_methods[1],
				NULL, NULL, NULL);
		PyObject *construct_bytes
			= PyCMethod_New(&construct_bytes_method,
					NULL, NULL, NULL);

		lua_load_module(self, "python", Py_BuildValue("{sO sO sO}",
				"list", table_list,
				"dict", table_dict,
				"bytes", construct_bytes));
		// }}}
	}

	return (PyObject *)self;
} // }}}

// Destructor.
static void Lua_dealloc(Lua *self) { // {{{
	lua_close(self->state);
	Lua_type->tp_free((PyObject *)self);
} // }}}

// set variable in Lua.
static void set(Lua *self, char const *name, PyObject *value) { // {{{
	lua_rawgeti(self->state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	Lua_push(self, value);
	lua_setfield(self->state, -2, name);
	lua_settop(self->state, -2);
} // }}}

static PyObject *return_stack(Lua *self, int pos, bool keep_single) { // {{{
	int size = lua_gettop(self->state) - pos;
	PyObject *ret;
	if (keep_single || size > 1) {
		ret = PyTuple_New(size);
		if (ret == NULL)
			return NULL;
		for (int i = 0; i < size; ++i)
			PyTuple_SET_ITEM(ret, i, Lua_to_python(self, -size + i));
		lua_settop(self->state, pos);
		return ret;
	} else if (size == 1) {
		ret = Lua_to_python(self, -1);
		lua_settop(self->state, pos);
		return ret;
	} else {
		lua_settop(self->state, pos);
		Py_RETURN_NONE;
	}
} // }}}

// run code after having loaded the buffer (internal use only).
static PyObject *run_code(Lua *self, int pos, bool keep_single) { // {{{
	lua_call(self->state, 0, LUA_MULTRET);
	if (PyErr_Occurred()) {
		lua_settop(self->state, 0);
		return NULL;
	}
	return return_stack(self, pos, keep_single);
} // }}}

// run string in lua.
static PyObject *run(Lua *self, char const *cmd, char const *description, bool keep_single) { // {{{
	int pos = lua_gettop(self->state);
	if (luaL_loadbuffer(self->state, cmd, strlen(cmd), description) != LUA_OK) {
		PyErr_SetString(PyExc_ValueError, lua_tostring(self->state, -1));
		return NULL;
	}
	return run_code(self, pos, keep_single);
} // }}}

// run file in lua.
static PyObject *do_run_file(Lua *self, char const *filename, bool keep_single) { // {{{
	int pos = lua_gettop(self->state);
	if (luaL_loadfilex(self->state, filename, NULL) != LUA_OK) {
		PyErr_SetString(PyExc_ValueError, lua_tostring(self->state, -1));
		return NULL;
	}
	return run_code(self, pos, keep_single);
} // }}}

// load module into lua.
void lua_load_module(Lua *self, char const *name, PyObject *dict) { // {{{
	if (!PyDict_Check(dict)) {
		PyObject *new_dict = PyDict_New();	// new reference
		PyObject *dir = PyObject_Dir(dict);	// new reference
		Py_ssize_t size = PyList_Size(dir);
		for (Py_ssize_t i = 0; i < size; ++i) {
			PyObject *key = PyList_GetItem(dir, i);	// borrowed reference
			Py_ssize_t len;
			char const *byteskey = PyUnicode_AsUTF8AndSize(key, &len);
			if (len > 0 && byteskey[0] == '_') {
				if (strncmp(byteskey, "_lua_", 5) != 0)
					continue;
				byteskey = &byteskey[4];
				len -= 4;
			}
			PyObject *value = PyObject_GetAttr(dict, key);	// new reference
			PyDict_SetItemString(new_dict, byteskey, value);	// no stealing
			Py_DECREF(value);
		}
		Py_DECREF(dir);
		Py_DECREF(dict);
		dict = new_dict;
	}
	lua_geti(self->state, LUA_REGISTRYINDEX, self->package_loaded);
	lua_createtable(self->state, 0, PyDict_Size(dict));	// Pushes new table on the stack.
	push_luatable_dict(self, dict);	// Fills it with contents.
	lua_setfield(self->state, -2, name);
	lua_settop(self->state, -2);
} // }}}

// load variable from lua stack into python.
PyObject *Lua_to_python(Lua *self, int index) { // {{{
	int type = lua_type(self->state, index);
	switch(type) {
	case LUA_TNIL:
		Py_RETURN_NONE;
	case LUA_TBOOLEAN:
		return PyBool_FromLong(lua_toboolean(self->state, index));
	//case LUA_TLIGHTUSERDATA: // Not used.
	//	return lua_touserdata(self->state, index)
	case LUA_TNUMBER:
	{
		// If this is exactly an integer, return it as an integer.
		lua_Integer iret = lua_tointeger(self->state, index);
		lua_Number ret = lua_tonumber(self->state, index);
		if ((lua_Number)iret == ret)
			return PyLong_FromLongLong(iret);
		return PyFloat_FromDouble(ret);
	}
	case LUA_TSTRING:
	{
		size_t len;
		char const *str = lua_tolstring(self->state, index, &len);
		return PyUnicode_DecodeUTF8(str, len, NULL);
	}
	case LUA_TTABLE:
		lua_pushvalue(self->state, index);
		return Table_create(self);
	case LUA_TFUNCTION:
		lua_pushvalue(self->state, index);
		return Function_create(self);
	case LUA_TUSERDATA:
		// This is a Python-owned object that was used by Lua.
		// The data block of the userdata stores the PyObject *.
		return *(PyObject **)lua_touserdata(self->state, index);
	//case LUA_TTHREAD: // Not used.
	//	return lua_tothread(self->state, index)
	default:
		//if (lua_iscfunction(self->state, index)) {
		//	return lua_tocfunction(self->state, index)
		//}
		return PyErr_Format(PyExc_ValueError, "Invalid type %x passed to Lua_to_python", type);
	}
} // }}}

// load variable from python onto lua stack.
void Lua_push(Lua *self, PyObject *obj) { // {{{
	if (obj == Py_None)
		lua_pushnil(self->state);
	else if (PyBool_Check(obj))
		lua_pushboolean(self->state, obj == Py_True);
	else if (PyLong_Check(obj))
		lua_pushinteger(self->state, PyLong_AsLongLong(obj));
	else if (PyUnicode_Check(obj)) {
		// A str is encoded as bytes in Lua; bytes is wrapped as an object.
		Py_ssize_t len;
		char const *str = PyUnicode_AsUTF8AndSize(obj, &len);
		lua_pushlstring(self->state, str, len);
	}
	else if (PyFloat_Check(obj))
		lua_pushnumber(self->state, PyFloat_AsDouble(obj));
	else if (PyObject_TypeCheck(obj, table_type))
		lua_rawgeti(self->state, LUA_REGISTRYINDEX, ((Table *)obj)->id);
	else if (PyObject_TypeCheck(obj, function_type))
		lua_rawgeti(self->state, LUA_REGISTRYINDEX, ((Function *)obj)->id);
	else {
		*(PyObject **)lua_newuserdatauv(self->state, sizeof(PyObject *), 0) = obj;
		Py_INCREF(obj);
		// FIXME: use gc metamethod to DECREF the object.

		lua_getfield(self->state, LUA_REGISTRYINDEX, "metatable");
		lua_setmetatable(self->state, -2);
	}
} // }}}

// Type definition. {{{
// Python-accessible methods. {{{
static PyObject *Lua_set(Lua *self, PyObject *args) { // {{{
	char const *name;
	PyObject *value;
	if (!PyArg_ParseTuple(args, "sO", &name, &value))
		return NULL;
	set(self, name, value);
	Py_RETURN_NONE;
} // }}}

static PyObject *Lua_run(Lua *self, PyObject *args, PyObject *keywords) { // {{{
	char const *code;
	char const *description = NULL;
	bool keep_single = false;
	char const *var = NULL;
	PyObject *value = NULL;
	char const *keywordnames[] = {"code", "description", "var", "value", "keep_single", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, keywords, "s|ssOp", (char **)keywordnames, &code, &description, &var, &value, &keep_single))
		return NULL;
	if (!description)
		description = code;
	if (var || value) {
		if (!var || !value) {
			PyErr_SetString(PyExc_ValueError, "var and value must both be present, or both be missing");
			return NULL;
		}
		set(self, var, value);
	}
	return run(self, code, description, keep_single);
} // }}}

static PyObject *Lua_run_file(Lua *self, PyObject *args, PyObject *keywords) { // {{{
	char const *filename;
	bool keep_single = false;
	char const *var = NULL;
	PyObject *value = NULL;
	char const *keywordnames[] = {"filename", "keep_single", "var", "value", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, keywords, "s|sOp", (char **)keywordnames, &filename, &var, &value, &keep_single))
		return NULL;
	if (var)
		set(self, var, value);
	return do_run_file(self, filename, keep_single);
} // }}}

static PyObject *Lua_module(Lua *self, PyObject *args) { // {{{
	char const *name;
	PyObject *dict;
	if (!PyArg_ParseTuple(args, "sO", &name, &dict))
		return NULL;
	lua_load_module(self, name, dict);
	Py_RETURN_NONE;
} // }}}

static PyObject *Lua_table(Lua *self, PyObject *args, PyObject *kwargs) { // {{{
	int pos = lua_gettop(self->state);
	size_t kw_size = kwargs == NULL ? 0 : PyDict_Size(kwargs);
	lua_createtable(self->state, PySequence_Size(args), kw_size);	// Pushes new table on the stack.
	push_luatable_list(self, args);
	if (kwargs != NULL)
		push_luatable_dict(self, kwargs);
	return return_stack(self, pos, false);
} // }}}
// }}}

// }}}

void Lua_dump_stack(Lua *self) { // {{{
	int n = lua_gettop(self->state);
	fprintf(stderr, "***** Lua stack dump *****\n");
	for (int i = 1; i <= n; ++i) {
		fprintf(stderr, "%d\t", i);
		PyObject *value = Lua_to_python(self, i);
		PyObject_Print(value, stderr, 0);
		Py_DECREF(value);
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "**************************\n");
} // }}}

// Module registration.
PyMODINIT_FUNC PyInit_lua() { // {{{
	static PyModuleDef Module = { // {{{
		PyModuleDef_HEAD_INIT,
		.m_name = "lua",
		.m_doc = PyDoc_STR("Use Lua code and object from Python and vice versa."),
		.m_size = 0,	// There is no module state.
		.m_methods = NULL,	// There are no functions.
		.m_slots = NULL,	// This must be NULL for PyModule_Create()
		.m_traverse = NULL,
		.m_clear = NULL,
		.m_free = NULL,
	}; // }}}
	static PyMethodDef Lua_methods[] = { // {{{
		{"set", (PyCFunction)Lua_set, METH_VARARGS, "Set a variable"},
		{"run", (PyCFunction)Lua_run, METH_VARARGS | METH_KEYWORDS, "Run a Lua script"},
		{"run_file", (PyCFunction)Lua_run_file, METH_VARARGS | METH_KEYWORDS, "Run a Lua script from a file"},
		{"module", (PyCFunction)Lua_module, METH_VARARGS, "Import a module into Lua"},
		{"table", (PyCFunction)Lua_table, METH_VARARGS | METH_KEYWORDS, "Create a Lua Table"},
		{NULL, NULL, 0, NULL}
	}; // }}}
	static PyType_Slot lua_slots[] = { // {{{
		{ Py_tp_new, Lua_new },
		{ Py_tp_dealloc, Lua_dealloc },
		{ Py_tp_doc, "Hold Lua object state" },
		{ Py_tp_methods, Lua_methods },
		{ 0, NULL },
	}; // }}}
	static PyType_Spec *LuaType; // {{{
	LuaType = malloc(sizeof(*LuaType));
	memset(LuaType, 0, sizeof(*LuaType));
	LuaType->name = "lua.Lua";
	LuaType->basicsize = sizeof(Lua);
	LuaType->itemsize = 0;
	LuaType->flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
	LuaType->slots = lua_slots;
	// }}}
	static PyType_Slot function_slots[] = { // {{{
		//{ Py_tp_new, NULL },
		{ Py_tp_dealloc, Function_dealloc },
		{ Py_tp_doc, "Access a Lua-owned function from Python" },
		{ Py_tp_call, Function_call },
		{ Py_tp_repr, Function_repr },
		{ 0, NULL },
	}; // }}}
	static PyType_Spec *FunctionType; // {{{
	FunctionType = malloc(sizeof(*FunctionType));
	memset(FunctionType, 0, sizeof(*FunctionType));
	FunctionType->name = "lua.Function";
	FunctionType->basicsize = sizeof(Function);
	FunctionType->itemsize = 0;
	FunctionType->flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
	FunctionType->slots = function_slots;
	// }}}
	static PyType_Slot table_slots[] = { // {{{
		{ Py_tp_dealloc, Table_dealloc },
		{ Py_tp_doc, "Access a Lua-owned table from Python" },
		{ Py_tp_repr, Table_repr },
		{ Py_tp_str, Table_repr },
		{ Py_mp_length, Table_len },
		{ Py_mp_subscript, Table_getitem },
		{ Py_mp_ass_subscript, Table_setitem },
		{ Py_tp_methods, Table_methods },
		{ 0, NULL },
	}; // }}}
	static PyType_Spec *TableType; // {{{
	TableType = malloc(sizeof(*TableType));
	memset(TableType, 0, sizeof(*TableType));
	TableType->name = "lua.Table";
	TableType->basicsize = sizeof(Table);
	TableType->itemsize = 0;
	TableType->flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
	TableType->slots = table_slots;
	// }}}
	static PyType_Slot table_iter_slots[] = { // {{{
		{ Py_tp_dealloc, Table_iter_dealloc },
		{ Py_tp_doc, "iterator for lua.table" },
		{ Py_tp_repr, Table_iter_repr },
		{ Py_tp_str, Table_iter_repr },
		{ Py_tp_iter, Table_iter_iter },
		{ Py_tp_iternext, Table_iter_iternext },
		{ 0, NULL },
	}; // }}}
	static PyType_Spec *TableIterType; // {{{
	TableIterType = malloc(sizeof(*TableIterType));
	memset(TableIterType, 0, sizeof(*TableIterType));
	TableIterType->name = "lua.Table.iterator";
	TableIterType->basicsize = sizeof(TableIter);
	TableIterType->itemsize = 0;
	TableIterType->flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
	TableIterType->slots = table_iter_slots;
	// }}}

	//fprintf(stderr, "creating module.\n");
	PyObject *m = PyModule_Create(&Module);
	if (!m)
		return NULL;

	//fprintf(stderr, "creating type lua.\n");
	Lua_type = (PyTypeObject *)PyType_FromModuleAndSpec(m, LuaType, NULL);
	if (!Lua_type || PyModule_AddObject(m, "Lua", (PyObject *)Lua_type) < 0) {
		Py_DECREF(m);
		return NULL;
	}
	Py_INCREF(Lua_type);

	//fprintf(stderr, "creating type function.\n");
	function_type = (PyTypeObject *)PyType_FromModuleAndSpec(m, FunctionType, NULL);
	if (!function_type || PyModule_AddObject(m, "Function", (PyObject *)function_type) < 0) {
		Py_DECREF(Lua_type);
		Py_DECREF(m);
		return NULL;
	}
	Py_INCREF(function_type);

	//fprintf(stderr, "creating type table.\n");
	table_type = (PyTypeObject *)PyType_FromModuleAndSpec(m, TableType, NULL);
	if (!table_type || PyModule_AddObject(m, "Table", (PyObject *)table_type) < 0) {
		Py_DECREF(Lua_type);
		Py_DECREF(function_type);
		Py_DECREF(m);
		return NULL;
	}
	Py_INCREF(table_type);

	//fprintf(stderr, "creating type table_iter.\n");
	table_iter_type = (PyTypeObject *)PyType_FromModuleAndSpec(m, TableIterType, NULL);
	if (!table_iter_type || PyModule_AddObject(m, "TableIter", (PyObject *)table_iter_type) < 0) {
		Py_DECREF(Lua_type);
		Py_DECREF(function_type);
		Py_DECREF(table_type);
		Py_DECREF(m);
		return NULL;
	}
	Py_INCREF(table_iter_type);

	//fprintf(stderr, "done.\n");
	return m;
} // }}}

PyTypeObject *Lua_type;

// vim: set foldmethod=marker :
