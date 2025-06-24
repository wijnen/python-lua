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
	{ "__add", "+", &PyNumber_Add, NULL },
	{ "__sub", "-", &PyNumber_Subtract, NULL },
	{ "__mul", "*", &PyNumber_Multiply, NULL },
	{ "__div", "/", &PyNumber_TrueDivide, NULL },
	{ "__mod", "%", &PyNumber_Remainder, NULL },
	{ "__pow", "^", NULL, NULL },
	{ "__idiv", "//", &PyNumber_FloorDivide, NULL },
	{ "__band", "&", &PyNumber_And, NULL },
	{ "__bor", "|", &PyNumber_Or, NULL },
	{ "__bxor", "~", &PyNumber_Xor, NULL },
	{ "__shl", "<<", &PyNumber_Lshift, NULL },
	{ "__shr", ">>", &PyNumber_Rshift, NULL },
	{ "__concat", "..", NULL, NULL },
	{ "__eq", "==", NULL, NULL },
	{ "__lt", "<", NULL, NULL },
	{ "__le", "<=", NULL, NULL },
	{ "__close", NULL, NULL, NULL },
	{ "__unm", NULL, NULL, &PyNumber_Negative },
	{ "__bnot", NULL, NULL, &PyNumber_Invert },
	{ "__len", NULL, NULL, NULL },
	{ "__tostring", NULL, NULL, &PyObject_Str },
	{ "__index", NULL, NULL, NULL },
	{ "__newindex", NULL, NULL, NULL },
	{ "__call", NULL, NULL, NULL },
	// Python also has: getattr, hasattr, setattr
}; // }}}

// Helper functions. {{{
static UserdataData *get_data(Lua *self, int index) // {{{
{
	return (UserdataData *)lua_touserdata(self->state, index);
} // }}}

static void make_userdata(Lua *self, PyObject *target) // {{{
{
	UserdataData *data =
		lua_newuserdatauv(self->state, sizeof(UserdataData), 0);
	data->target = target;
	data->next = self->first_userdata;
	data->prev = NULL;
	if (data->next)
		data->next->prev = data;
	self->first_userdata = data;
	Py_INCREF(target);

	lua_getfield(self->state, LUA_REGISTRYINDEX, "metatable");
	lua_setmetatable(self->state, -2);
} // }}}

static void del_userdata(Lua *self, UserdataData *data) // {{{
{
	Py_DECREF(data->target);
	data->target = NULL;
	if (data->prev)
		data->prev->next = data->next;
	else
		self->first_userdata = data->next;
	if (data->next)
		data->next->prev = data->prev;
	data->prev = NULL;
	data->next = NULL;
} // }}}

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

// Lua callback for all standard userdata metamethods.
// Method selection is done via operator name stored in upvalue.
// Userdata metamethods are called when Lua code uses an operator
// on a Python-owned object.
static int metamethod(lua_State *state) { // {{{
	// This function is called from Lua through a metatable.
	// Get self pointer.
	lua_getfield(state, LUA_REGISTRYINDEX, "self");
	Lua *lua = (Lua *)lua_touserdata(state, -1);
	lua_pop(state, 1);

	// 1 upvalue: python method index.
	int method_index = lua_tointeger(state, lua_upvalueindex(1));

	PyObject *target = Lua_to_python(lua, 1);
	if (operators[method_index].python_binary) {
		PyObject *rhs = Lua_to_python(lua, 2);
		PyObject *result
			= operators[method_index].python_binary(target, rhs);
		Py_DECREF(target);
		Py_DECREF(rhs);
		if (result == NULL)
			return 0;
		Lua_push(lua, result);
		Py_DECREF(result);
		return 1;
	}
	if (operators[method_index].python_unary) {
		PyObject *result = operators[method_index].python_unary(target);
		if (result == NULL)
			return 0;
		Lua_push(lua, result);
		Py_DECREF(target);
		Py_DECREF(result);
		return 1;
	}

	// Special operator.
	PyObject *rhs;
	PyObject *ret;
	switch (method_index) {
	case POW:
		rhs = Lua_to_python(lua, 2);
		ret = PyNumber_Power(target, rhs, Py_None);
		Lua_push(lua, ret);
		Py_DECREF(target);
		Py_DECREF(rhs);
		Py_DECREF(ret);
		return 1;
	case EQ:
		rhs = Lua_to_python(lua, 2);
		ret = PyObject_RichCompare(target, rhs, Py_EQ);
		Lua_push(lua, ret);
		Py_DECREF(target);
		Py_DECREF(rhs);
		Py_DECREF(ret);
		return 1;
	case LT:
		rhs = Lua_to_python(lua, 2);
		ret = PyObject_RichCompare(target, rhs, Py_LT);
		Lua_push(lua, ret);
		Py_DECREF(target);
		Py_DECREF(rhs);
		Py_DECREF(ret);
		return 1;
	case LE:
		rhs = Lua_to_python(lua, 2);
		ret = PyObject_RichCompare(target, rhs, Py_LE);
		Lua_push(lua, ret);
		Py_DECREF(target);
		Py_DECREF(rhs);
		Py_DECREF(ret);
		return 1;
	case CONCAT:
	{
		rhs = Lua_to_python(lua, 2);
		PyObject *str_target = PyObject_Str(target);
		Py_DECREF(target);
		PyObject *str_rhs = PyObject_Str(rhs);
		Py_DECREF(rhs);
		ret = PyNumber_Add(str_target, str_rhs);
		Py_DECREF(str_target);
		Py_DECREF(str_rhs);
		Lua_push(lua, ret);
		Py_DECREF(ret);
		return 1;
	}
	case LEN:
	{
		Py_ssize_t len = PyObject_Length(target);
		lua_pushinteger(lua->state, len);
		Py_DECREF(target);
		return 1;
	}
	case CLOSE:
		// TODO: Decide what to do with this.
		Py_DECREF(target);
		return 0;
	case INDEX:
	{
		PyObject *key = Lua_to_python(lua, 2);
		PyObject *value = PyObject_GetItem(target, key);
		Py_DECREF(target);
		PyErr_Clear();
		if (value == NULL) {
			// Attempt getattr.
			Py_ssize_t len = 0;
			char const *str = PyUnicode_AsUTF8AndSize(key, &len);
			if (len > 0 && str[0] != '_')
				value = PyObject_GetAttr(target, key);
			PyErr_Clear();
			if (value == NULL) {
				Py_DECREF(key);
				fprintf(stderr, "Failed to get item.\n");
				return 0;
			}
		}
		Py_DECREF(key);
		Lua_push(lua, value);
		Py_DECREF(value);
		return 1;
	}
	case NEWINDEX:
	{
		PyObject *key = Lua_to_python(lua, 2);
		PyObject *value = Lua_to_python(lua, 3);
		int success = PyObject_SetItem(target, key, value) >= 0;
		Py_DECREF(target);
		Py_DECREF(key);
		Py_DECREF(value);
		if (!success) {
			fprintf(stderr, "Failed to set item.\n");
			return 0;
		}
		return 0;
	}
	case CALL:
	{
		// Create arguments tuple
		int nargs = lua_gettop(state);
		PyObject *args = PyTuple_New(nargs - 1);
		for (int i = 2; i <= nargs; ++i) {
			// New reference.
			PyObject *arg = Lua_to_python(lua, i);

			// Steals reference to arg.
			PyTuple_SET_ITEM(args, i - 2, arg);
		}

		// Call function.
		PyObject *ret = PyObject_CallObject(target, args);
		Py_DECREF(args);
		Py_DECREF(target);
		if (ret == NULL) {
			// Call returned error.
			return 0;
		}
		Lua_push(lua, ret);
		Py_DECREF(ret);
		return 1;
	}
	case TOSTRING:
	{
		PyObject *str = PyObject_Str(target);
		Py_DECREF(target);
		Py_ssize_t size;
		const char *value = PyUnicode_AsUTF8AndSize(str, &size);
		lua_pushlstring(state, value, size);
		Py_DECREF(str);
		return 1;
	}
	default:
		fprintf(stderr, "Invalid operator %d.\n", method_index);
		Py_DECREF(target);
		return 0;
	}
} // }}}

// Garbage collect helper for Python objects that are accessible in Lua.
// This is called when the link to Lua is lost; the object may still have
// references in Python.
static int gc(lua_State *state) { // {{{
	lua_getfield(state, LUA_REGISTRYINDEX, "self");
	Lua *lua = (Lua *)lua_touserdata(state, -1);
	lua_pop(state, 1);

	UserdataData *data = get_data(lua, 1);
	del_userdata(lua, data);
	
	return 0;
} // }}}

// Run a chunk of code in Lua and store the return value in the registry.
// Return the index of the new item in the registry.
static lua_Integer build_wrapper(Lua *self, char const *code, char const *desc,
		bool make_ref)
{ // {{{
	if (luaL_loadbuffer(self->state, code, strlen(code), desc ? desc : code) != LUA_OK) {
		lua_close(self->state);
		PyErr_SetString(PyExc_ValueError, lua_tostring(self->state, -1));
		return LUA_NOREF;
	}
	if (lua_pcall(self->state, 0, 1, 0) != LUA_OK) {
		PyErr_Format(PyExc_RuntimeError,
                                "Error running build wrapper: %s",
                                lua_tostring(self->state, -1));
		lua_pop(self->state, 1);
		return LUA_NOREF;
	}
	if (!make_ref) {
		lua_pop(self->state, 1);
		return LUA_REFNIL;
	}
	return luaL_ref(self->state, LUA_REGISTRYINDEX);
} // }}}

// Create (native) Lua table on Lua stack from items in Python dict.
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

// Create (native) Lua table on Lua stack from items in Python list.
static void push_luatable_list(Lua *self, PyObject *obj) { // {{{
	Py_ssize_t size = PySequence_Size(obj);
	for (Py_ssize_t i = 0; i < size; ++i) {
		PyObject *value = PySequence_GetItem(obj, i);	// New reference.
		Lua_push(self, value);
		Py_DECREF(value);
		lua_rawseti(self->state, -2, i + 1);
	}
} // }}}

// Construct a Python bytes object from Lua.
static PyObject *construct_bytes_impl(PyObject * /*self*/, PyObject *args) // {{{
{
	// Implementation of python.bytes()
	PyObject *arg;
	if (!PyArg_ParseTuple(args, "O", &arg))
		return NULL;
	if (PyObject_IsInstance(arg, (PyObject*)table_type)) {
		PyObject *list = table_list_method(arg, NULL);
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
// }}}

static int Lua_init(PyObject *py_self, PyObject *args, PyObject *keywords)
{ // {{{
	if (!PyObject_IsInstance(py_self, (PyObject *)Lua_type)) {
		PyErr_SetString(PyExc_ValueError,
				"Lua init called on wrong object");
		return -1;
	}
	Lua *self = (Lua *)py_self;
	// Initializes a new lua object.
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
		return -1;
	// }}}

	// Create new state and store back pointer to self. {{{
	self->state = luaL_newstate();
	lua_pushlightuserdata(self->state, self);
	lua_setfield(self->state, LUA_REGISTRYINDEX, "self");
	// }}}

	// Open standard libraries. Many of them are closed again below.
	luaL_openlibs(self->state);

	// Create metatable to use Python objects from Lua. {{{
	lua_createtable(self->state, 0, NUM_OPERATORS + 1);
	for (int i = 0; i < NUM_OPERATORS; ++i) {
		// Create metatable for table wrappers.
		lua_pushinteger(self->state, i);
		lua_pushcclosure(self->state, metamethod, 1);
		lua_setfield(self->state, -2, operators[i].lua_name);
	}

	// Unary operators.
	lua_Integer index = build_wrapper(self, "return function(a) return -a end", NULL, true);
	if (index == LUA_NOREF)
		return -1;
	lua_rawgeti(self->state, LUA_REGISTRYINDEX, index);
	self->lua_operator[NEG] = Function_create(self);

	index = build_wrapper(self, "return function(a) return ~a end", NULL, true);
	if (index == LUA_NOREF)
		return -1;
	lua_rawgeti(self->state, LUA_REGISTRYINDEX, index);
	self->lua_operator[NOT] = Function_create(self);

	index = build_wrapper(self, "return function(a) return tostring(a) end", NULL, true);
	if (index == LUA_NOREF)
		return -1;
	lua_rawgeti(self->state, LUA_REGISTRYINDEX, index);
	self->lua_operator[TOSTRING] = Function_create(self);

	// Skipped: len, getitem, setitem, delitem, because they have API calls which are used.

	// Set gc metamethod. This is not passed through to Python, but instead cleans up the PyObject * reference.
	lua_pushcclosure(self->state, gc, 0);
	lua_setfield(self->state, -2, "__gc");

	// Set the metatable in the registry, for use by new userdata objects.
	lua_setfield(self->state, LUA_REGISTRYINDEX, "metatable");
	// }}}

	// Store Lua operators that the API does not define.
	for (int i = 0; i < NUM_OPERATORS; ++i) {
		// Store lua operators, for calling operators
		// on Lua objects from Python.
		if (!operators[i].lua_operator)
			continue;

		// Store binary operators. (Others are stored below this loop.)
		static const char *template
			= "return function(a, b) return a %s b end";
		char buffer[50]; //strlen(template) + 2];
		sprintf(buffer, template, operators[i].lua_operator);
		lua_Integer function_id
			= build_wrapper(self, buffer, NULL, true);
		if (function_id == LUA_NOREF)
			return -1;
		lua_rawgeti(self->state, LUA_REGISTRYINDEX, function_id);
		self->lua_operator[i] = Function_create(self);
	}

	// Store a copy of some initial values, so they still work if the original value is replaced.
	self->table_remove = build_wrapper(self, "return table.remove", "get table.remove", true);
	if (self->table_remove == LUA_NOREF)
		return -1;
	self->table_concat = build_wrapper(self, "return table.concat", "get table.concat", true);
	if (self->table_concat == LUA_NOREF)
		return -1;
	self->table_insert = build_wrapper(self, "return table.insert", "get table.insert", true);
	if (self->table_insert == LUA_NOREF)
		return -1;
	self->table_unpack = build_wrapper(self, "return table.unpack", "get table.unpack", true);
	if (self->table_unpack == LUA_NOREF)
		return -1;
	self->table_move = build_wrapper(self, "return table.move", "get table.move", true);
	if (self->table_move == LUA_NOREF)
		return -1;
	self->table_sort = build_wrapper(self, "return table.sort", "get table.sort", true);
	if (self->table_sort == LUA_NOREF)
		return -1;
	self->package_loaded = build_wrapper(self, "return package.loaded", "get package.loaded", true);
	if (self->package_loaded == LUA_NOREF)
		return -1;

	// Disable optional features that have not been requested. {{{
	if (!debug) {
		if (build_wrapper(self, "debug = nil package.loaded.debug = nil", "disabling debug", false) == LUA_NOREF)
			return -1;
	}
	if (!loadlib) {
		if (build_wrapper(self, "package.loadlib = nil", "disabling loadlib", false) == LUA_NOREF)
			return -1;
	}
	if (!searchers) {
		if (build_wrapper(self, "package.searchers = {}", "disabling searchers", false) == LUA_NOREF)
			return -1;
	}
	if (!doloadfile) {
		if (build_wrapper(self, "loadfile = nil dofile = nil", "disabling loadfile and dofile", false) == LUA_NOREF)
			return -1;
	}
	if (!os) {
		if (build_wrapper(self, "os = {clock = os.clock, date = os.date, difftime = os.difftime, setlocale = os.setlocale, time = os.time} package.loaded.os = os", "disabling some of os", false) == LUA_NOREF)
			return -1;
	}
	if (!io) {
		if (build_wrapper(self, "io = nil package.loaded.io = nil", "disabling io", false) == LUA_NOREF)
			return -1;
	}
	// }}}

	// Add access to Python object constructors from Lua (unless disabled).
	if (python_module) {
		// Lua module for accessing some Python parts from Lua. {{{
		// This is prepared as a "python" module unless disabled.
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

	return 0;
} // }}}

// Destructor.
static void Lua_dealloc(PyObject *py_self) { // {{{
	Lua *self = (Lua *)py_self;
	lua_close(self->state);
	PyObject_GC_UnTrack(py_self);
	Lua_clear(py_self);
	Py_TYPE(self)->tp_free(py_self);
} // }}}

// set variable in Lua.
static void set(Lua *self, char const *name, PyObject *value) { // {{{
	lua_rawgeti(self->state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	Lua_push(self, value);
	lua_setfield(self->state, -2, name);
	lua_settop(self->state, -2);
} // }}}

static PyObject *return_stack(Lua *self, int pos, bool keep_single) // {{{
{
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
PyObject *Lua_run_code(Lua *self, int pos, bool keep_single, int nargs) // {{{
{
	int ret = lua_pcall(self->state, nargs, LUA_MULTRET, 0);
	if (PyErr_Occurred()) {
		lua_settop(self->state, 0);
		return NULL;
	}
	if (ret != LUA_OK) {
		PyErr_Format(PyExc_RuntimeError,
				"Error running Lua code: %s",
				lua_tostring(self->state, -1));
		lua_settop(self->state, 0);
		return NULL;
	}
	return return_stack(self, pos, keep_single);
} // }}}

// run string in lua.
static PyObject *run(Lua *self, char const *cmd, char const *description, bool keep_single) { // {{{
	//fprintf(stderr, "Running code %s.\n", cmd);
	//Lua_dump_stack(self);
	int pos = lua_gettop(self->state);
	if (luaL_loadbuffer(self->state, cmd, strlen(cmd), description) != LUA_OK) {
		PyErr_SetString(PyExc_ValueError, lua_tostring(self->state, -1));
		return NULL;
	}
	return Lua_run_code(self, pos, keep_single, 0);
} // }}}

// run file in lua.
static PyObject *do_run_file(Lua *self, char const *filename, bool keep_single) { // {{{
	int pos = lua_gettop(self->state);
	if (luaL_loadfilex(self->state, filename, NULL) != LUA_OK) {
		PyErr_SetString(PyExc_ValueError, lua_tostring(self->state, -1));
		return NULL;
	}
	return Lua_run_code(self, pos, keep_single, 0);
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
	PyObject *ret;
	PyGILState_STATE state = PyGILState_Ensure();
	switch(type) {
	case LUA_TNIL:
		Py_RETURN_NONE;
	case LUA_TBOOLEAN:
		ret = PyBool_FromLong(lua_toboolean(self->state, index));
		break;
	//case LUA_TLIGHTUSERDATA: // Not used.
	//	return lua_touserdata(self->state, index)
	case LUA_TNUMBER:
	{
		// If this is exactly an integer, return it as an integer.
		lua_Integer iret = lua_tointeger(self->state, index);
		lua_Number fret = lua_tonumber(self->state, index);
		if ((lua_Number)iret == fret)
			ret = PyLong_FromLongLong(iret);
		else
			ret = PyFloat_FromDouble(fret);
		break;
	}
	case LUA_TSTRING:
	{
		size_t len;
		char const *str = lua_tolstring(self->state, index, &len);
		//printf("str: %*s\n", (int)len, str);
		ret = PyUnicode_DecodeUTF8(str, len, NULL);
		break;
	}
	case LUA_TTABLE:
		lua_pushvalue(self->state, index);
		ret = Table_create(self);
		break;
	case LUA_TFUNCTION:
		lua_pushvalue(self->state, index);
		ret = Function_create(self);
		break;
	case LUA_TUSERDATA:
	{
		// This is a Python-owned object that was used by Lua.
		// The data block of the userdata stores the PyObject *.
		UserdataData *data = get_data(self, index);
		ret = data->target;
		Py_INCREF(ret);
		break;
	}
	//case LUA_TTHREAD: // Not used.
	//	return lua_tothread(self->state, index)
	default:
		//if (lua_iscfunction(self->state, index)) {
		//	return lua_tocfunction(self->state, index)
		//}
		ret = PyErr_Format(PyExc_ValueError,
				"Invalid type %x passed to Lua_to_python",
				type);
		break;
	}
	PyGILState_Release(state);
	return ret;
} // }}}

// load variable from python onto lua stack.
void Lua_push(Lua *self, PyObject *obj) { // {{{
	if (obj == Py_None) {
		lua_pushnil(self->state);
	} else if (PyObject_TypeCheck(obj, table_type)) {
		lua_rawgeti(self->state, LUA_REGISTRYINDEX, ((Table *)obj)->id);
	} else if (PyObject_TypeCheck(obj, function_type)) {
		lua_rawgeti(self->state, LUA_REGISTRYINDEX,
				((Function *)obj)->id);
	} else if (PyBool_Check(obj)) {
		lua_pushboolean(self->state, obj == Py_True);
	} else if (PyLong_Check(obj)) {
		lua_pushinteger(self->state, PyLong_AsLongLong(obj));
	} else if (PyUnicode_Check(obj)) {
		// A str is encoded as bytes in Lua;
		// bytes is wrapped as an object.
		Py_ssize_t len;
		char const *str = PyUnicode_AsUTF8AndSize(obj, &len);
		lua_pushlstring(self->state, str, len);
	} else if (PyFloat_Check(obj)) {
		lua_pushnumber(self->state, PyFloat_AsDouble(obj));
	} else {
		make_userdata(self, obj);
	}
} // }}}

int Lua_traverse(PyObject *py_self, visitproc visit, void *arg) // {{{
{
	PyObject_VisitManagedDict(py_self, visit, arg);
	Lua *self = (Lua *)py_self;
	// Visit type.
	Py_VISIT(Py_TYPE(self));
	// Visit every userdata.
	for (UserdataData *data = self->first_userdata; data; data = data->next)
		Py_VISIT(data->target);
	return 0;
} // }}}

int Lua_clear(PyObject *py_self) // {{{
{
	PyObject_ClearManagedDict(py_self);
	// Clear every userdata.
	Lua *self = (Lua *)py_self;
	for (UserdataData *data = self->first_userdata; data; data = data->next)
		Py_CLEAR(data->target);
	return 0;
} // }}}

// Type definition.
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
	PyObject *code_obj;
	char const *description = NULL;
	bool keep_single = false;
	char const *var = NULL;
	PyObject *value = NULL;
	char const *keywordnames[] = {"code", "description", "var", "value",
		"keep_single", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, keywords, "O|ssOp",
				(char **)keywordnames,
				&code_obj,
				&description,
				&var,
				&value,
				&keep_single))
		return NULL;
	Py_ssize_t len = 0;
	const char *code;
	if (PyBytes_Check(code_obj)) {
		PyBytes_AsStringAndSize(code_obj, (char **)&code, &len);
	} else if (PyUnicode_Check(code_obj)) {
		code = PyUnicode_AsUTF8AndSize(code_obj, &len);
	} else {
		PyErr_SetString(PyExc_ValueError, "code must be str or bytes");
		return NULL;
	}
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
		{"run", (PyCFunction)Lua_run, METH_VARARGS | METH_KEYWORDS,
			"Run a Lua script"},
		{"run_file", (PyCFunction)Lua_run_file,
			METH_VARARGS | METH_KEYWORDS,
			"Run a Lua script from a file"},
		{"module", (PyCFunction)Lua_module, METH_VARARGS,
			"Import a module into Lua"},
		{"table", (PyCFunction)Lua_table, METH_VARARGS | METH_KEYWORDS,
			"Create a Lua Table"},
		{NULL, NULL, 0, NULL}
	}; // }}}
	static PyType_Slot Lua_slots[] = { // {{{
		{ Py_tp_traverse, &Lua_traverse },
		{ Py_tp_clear, &Lua_clear },
		{ Py_tp_init, &Lua_init },
		{ Py_tp_dealloc, &Lua_dealloc },
		{ Py_tp_doc, "Hold Lua object state" },
		{ Py_tp_methods, Lua_methods },
		{ 0, NULL }
	}; // }}}
	static PyType_Spec Lua_spec = { // {{{
		.name = "lua.Lua",
		.basicsize = sizeof(Lua),
		.itemsize = 0,
		.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC
			| Py_TPFLAGS_MANAGED_DICT | Py_TPFLAGS_HEAPTYPE,
		.slots = Lua_slots
	};
	// }}}

	static PyType_Slot function_slots[] = { // {{{
		{ Py_tp_traverse, &function_traverse },
		{ Py_tp_clear, &function_clear },
		{ Py_tp_dealloc, &Function_dealloc },
		{ Py_tp_doc, "Access a Lua-owned function from Python" },
		{ Py_tp_call, &Function_call },
		{ Py_tp_repr, &Function_repr },
		{ 0, NULL }
	}; // }}}
	static PyType_Spec function_spec = { // {{{
		.name = "lua.Function",
		.basicsize = sizeof(Function),
		.itemsize = 0,
		.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
			Py_TPFLAGS_MANAGED_DICT | Py_TPFLAGS_HEAPTYPE,
		.slots = function_slots
	}; // }}}

	static PyType_Slot table_slots[] = { // {{{
		{ Py_tp_traverse, &table_traverse },
		{ Py_tp_clear, &table_clear },
		{ Py_tp_dealloc, &Table_dealloc },
		{ Py_tp_doc, "Access a Lua-owned table from Python" },
		{ Py_tp_repr, &Table_repr },
		{ Py_tp_str, &Table_repr },
		{ Py_tp_richcompare, &Table_richcompare },
		{ Py_tp_call, &Table_call },
		{ Py_tp_methods, &Table_methods },
		{ Py_nb_add, &Table_add },
		{ Py_nb_inplace_add, &Table_iadd },
		{ Py_nb_subtract, &Table_sub },
		{ Py_nb_multiply, &Table_mul },
		{ Py_nb_true_divide, &Table_div },
		{ Py_nb_remainder, &Table_mod },
		{ Py_nb_power, &Table_power },
		{ Py_nb_floor_divide, &Table_idiv },
		{ Py_nb_and, &Table_and },
		{ Py_nb_or, &Table_or },
		{ Py_nb_xor, &Table_xor },
		{ Py_nb_lshift, &Table_lshift },
		{ Py_nb_rshift, &Table_rshift },
		{ Py_nb_matrix_multiply, &Table_concat },
		{ Py_nb_negative, &Table_neg },
		{ Py_nb_invert, &Table_not },
		{ Py_mp_length, &Table_len },
		{ Py_mp_subscript, &Table_getitem },
		{ Py_mp_ass_subscript, &Table_setitem },
		{ Py_sq_contains, &Table_contains },
		{ 0, NULL }
	}; // }}}
	static PyType_Spec table_spec = { // {{{
		.name = "lua.Table",
		.basicsize = sizeof(Table),
		.itemsize = 0,
		.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
			Py_TPFLAGS_MANAGED_DICT | Py_TPFLAGS_HEAPTYPE,
		.slots = table_slots
	}; // }}}

	static PyType_Slot table_iter_slots[] = { // {{{
		{ Py_tp_traverse, &table_iter_traverse },
		{ Py_tp_clear, &table_iter_clear },
		{ Py_tp_dealloc, &Table_iter_dealloc },
		{ Py_tp_doc, "iterator for lua.table" },
		{ Py_tp_repr, &Table_iter_repr },
		{ Py_tp_str, &Table_iter_repr },
		{ Py_tp_iter, &Table_iter_iter },
		{ Py_tp_iternext, &Table_iter_iternext },
		{ 0, NULL }
	}; // }}}
	static PyType_Spec table_iter_spec = { // {{{
		.name = "lua.Table.iterator",
		.basicsize = sizeof(TableIter),
		.itemsize = 0,
		.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
			Py_TPFLAGS_MANAGED_DICT | Py_TPFLAGS_HEAPTYPE,
		.slots = table_iter_slots,
	}; // }}}

	PyObject *m = PyModule_Create(&Module);
	if (!m)
		return NULL;

	Lua_type = (PyTypeObject *)PyType_FromModuleAndSpec(m, &Lua_spec, NULL);
	if (!Lua_type || PyModule_AddObject(m, "Lua",
				(PyObject *)Lua_type) < 0) {
               Py_DECREF(m);
               return NULL;
       }
       Py_INCREF(Lua_type);

       function_type = (PyTypeObject *)PyType_FromModuleAndSpec(m,
		       &function_spec, NULL);
       if (!function_type || PyModule_AddObject(m, "Function",
			       (PyObject *)function_type) < 0) {
               Py_DECREF(Lua_type);
               Py_DECREF(m);
               return NULL;
       }
       Py_INCREF(function_type);

       table_type = (PyTypeObject *)PyType_FromModuleAndSpec(m,
		       &table_spec, NULL);
       if (!table_type || PyModule_AddObject(m, "Table",
			       (PyObject *)table_type) < 0) {
               Py_DECREF(Lua_type);
               Py_DECREF(function_type);
               Py_DECREF(m);
               return NULL;
       }
       Py_INCREF(table_type);

       table_iter_type = (PyTypeObject *)PyType_FromModuleAndSpec(m,
		       &table_iter_spec, NULL);
       if (!table_iter_type || PyModule_AddObject(m, "TableIter",
			       (PyObject *)table_iter_type) < 0) {
               Py_DECREF(Lua_type);
               Py_DECREF(function_type);
               Py_DECREF(table_type);
               Py_DECREF(m);
               return NULL;
       }
       Py_INCREF(table_iter_type);

	return m;
} // }}}

PyTypeObject *Lua_type;

// vim: set foldmethod=marker :
