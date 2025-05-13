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
#include <lua.hpp>
#include <lauxlib.h>
#include <cassert>
#include <map>
#include <utility>
#include <string>
// }}}

extern PyTypeObject LuaType, TableType, FunctionType;

class Lua;
class Function;
class Table;

class Lua { // {{{
	friend class Function;
	friend class Table;

public:
	PyObject_HEAD

	// Constructor.
	Lua(bool debug = false, bool loadlib = false, bool searchers = false, bool doloadfile = false, bool io = false, bool os = false, bool python_module = true);

	// __new__ function for creating the Python object.
	static PyObject *create(PyTypeObject *type, PyObject *args, PyObject *kwds);

	// Cleanup.
	static void dealloc(Lua *self);

	static PyMethodDef methods[];

private:
	// Context for Lua environment.
	lua_State *state;

	// Names for generating lua functions that perform operator calls.
	// First item is lua operator code, second item is python metamethod name.
	static std::map <char const *, char const *> const opnames;

	// Names for calling python metamethods from lua metatable.
	// First item is lua metatable name key, seconde item is python metamethod name.
	static std::map <char const *, char const *> const lua2python;

	// Copies of initial values of some globals, to use later regardless of them changing in Lua.
	PyObject *table_remove;
	PyObject *package_loaded;
	PyObject *_G;

	// Stored Lua functions of all operators, to be used from Python calls on Lua-owned objects.
	std::map <char const *, PyObject *> ops;

	// Lua callback for all userdata metamethods. (Method selection is done via operator name stored in upvalue.)
	static int metamethod(lua_State *state);

	// Lua callback for userdata garbage collection.
	static int gc(lua_State *state);

	// Set variable in Lua.
	void set(std::string const &name, PyObject *value);

	// Run code after having loaded the buffer (internal use only).
	PyObject *run_code(int pos, bool keep_single);

	// Run string in Lua.
	PyObject *run(std::string const &cmd, std::string const &description, bool keep_single);

	// Run file in Lua.
	PyObject *run_file(std::string const &filename, std::string const &description, bool keep_single);

	// Load module into Lua.
	void load_module(std::string const &name, PyObject *dict);

	// Load variable from Lua stack into Python.
	PyObject *to_python(int index);

	// Load variable from Python onto Lua stack.
	void push(PyObject *obj);

	// Create (native) Lua table on Lua stack from items in Python list or dict.
	void push_luatable(PyObject *obj);

	// Python-accessible methods. {{{
	static PyObject *set_method(Lua *self, PyObject *args);
	static PyObject *run_method(Lua *self, PyObject *args, PyObject *keywords);
	static PyObject *run_file_method(Lua *self, PyObject *args, PyObject *keywords);
	static PyObject *module_method(Lua *self, PyObject *args);
	// }}}
}; // }}}

class Function { // {{{
public:
	PyObject_HEAD

	// Construct new function from value at top of stack.
	Function(Lua *context);

	// Cleanup.
	static void dealloc(Function *self);

	// __new__ function for creating the Python object.
	static PyObject *create(Lua *context);

	static PyMethodDef methods[];

private:
	// Context in which this object is defined.
	Lua *lua;

	// Registry index holding the Lua function.
	lua_Integer id;

	// Python-accessible methods.
	static PyObject *call_method(Function *self, PyObject *args, PyObject *keywords);

	friend class Lua;
}; // }}}

class Table { // {{{
public:
	PyObject_HEAD

	lua_Integer id;

	// Construct new table from value at top of stack.
	Table(Lua *context);

	// Cleanup.
	static void dealloc(Table *self);

	// __new__ function for creating the Python object.
	static PyObject *create(Lua *context);

	static PyMethodDef methods[];

private:
	// Context in which this object is defined.
	Lua *lua;
	lua_Integer iter_ref;

	// Python-accessible methods.
	static PyObject *len_method(Table *self, PyObject *args);
	static PyObject *iadd_method(Table *self, PyObject *args);
	static PyObject *getitem_method(Table *self, PyObject *args);
	static PyObject *setitem_method(Table *self, PyObject *args);
	static PyObject *delitem_method(Table *self, PyObject *args);
	static PyObject *contains_method(Table *self, PyObject *args);
	static PyObject *iter_method(Table *self, PyObject *args);
	static PyObject *ne_method(Table *self, PyObject *args);
	static PyObject *gt_method(Table *self, PyObject *args);
	static PyObject *ge_method(Table *self, PyObject *args);
	static PyObject *dict_method(Table *self, PyObject *args);
	static PyObject *list_method(Table *self, PyObject *args);
	static PyObject *pop_method(Table *self, PyObject *args);
	static PyObject *next_method(Table *self, PyObject *args);

	friend class Lua;

	void clear_iter();
}; // }}}

// Module registration. {{{
#define ObjDef(name, doc, create) \
PyTypeObject name ## Type { \
	/* XXX Using .ob_base is undocumented, but otherwise C++ does not allow specifying the other identifiers by name. */ \
	.ob_base = PyVarObject_HEAD_INIT(nullptr, 0) \
	.tp_name = "lua." #name, \
	.tp_basicsize = sizeof(name), \
	.tp_itemsize = 0, \
	.tp_dealloc = reinterpret_cast <destructor>(name::dealloc), \
	.tp_flags = Py_TPFLAGS_DEFAULT, \
	.tp_doc = PyDoc_STR(doc), \
	.tp_methods = name::methods, \
	.tp_new = create, \
}

ObjDef(Lua, "Hold Lua object state", Lua::create);
ObjDef(Function, "Access a Lua-owned function from Python", nullptr);
ObjDef(Table, "Access a Lua-owned table from Python", nullptr);

static PyModuleDef Module = {
	// XXX Using .m_base is undocumented, but otherwise C++ does not allow specifying the other identifiers by name.
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "lua",
	.m_doc = nullptr,
	.m_size = -1,	// State is held in global variables
};

extern "C" {
	PyMODINIT_FUNC PyInit_lua() {
		if (PyType_Ready(&LuaType) < 0)
			return nullptr;
		if (PyType_Ready(&FunctionType) < 0)
			return nullptr;
		if (PyType_Ready(&TableType) < 0)
			return nullptr;

		PyObject *m = PyModule_Create(&Module);
		if (!m)
			return nullptr;

		Py_INCREF(&LuaType);
		Py_INCREF(&FunctionType);
		Py_INCREF(&TableType);

		if (PyModule_AddObject(m, "Lua", (PyObject *) &LuaType) < 0
				|| PyModule_AddObject(m, "Function", (PyObject *) &FunctionType) < 0
				|| PyModule_AddObject(m, "Table", (PyObject *) &TableType) < 0) {
			Py_DECREF(&LuaType);
			Py_DECREF(&FunctionType);
			Py_DECREF(&TableType);
			Py_DECREF(m);
			return nullptr;
		}

		return m;
	}
}
// }}}

// class Lua implementation. {{{
// Operator name mappings. {{{
std::map <char const *, char const *> const Lua::opnames = {
	std::make_pair("+", "__add__"),
	std::make_pair("-", "__sub__"),
	std::make_pair("*", "__mul__"),
	std::make_pair("/", "__truediv__"),
	std::make_pair("%", "__mod__"),
	std::make_pair("^", "__pow__"),
	std::make_pair("//", "__floordiv__"),
	std::make_pair("&", "__and__"),
	std::make_pair("|", "__or__"),
	std::make_pair("~", "__xor__"),
	std::make_pair("<<", "__lshift__"),
	std::make_pair(">>", "__rshift__"),
	std::make_pair("..", "__matmul__"),
	std::make_pair("==", "__eq__"),
	std::make_pair("<", "__lt__"),
	std::make_pair("<=", "__le__")
};

std::map <char const *, char const *> const Lua::lua2python = {
	std::make_pair("__add", "__add__"),
	std::make_pair("__sub", "__sub__"),
	std::make_pair("__mul", "__mul__"),
	std::make_pair("__div", "__truediv__"),
	std::make_pair("__mod", "__mod__"),
	std::make_pair("__pow", "__pow__"),
	std::make_pair("__idiv", "__floordiv__"),
	std::make_pair("__band", "__and__"),
	std::make_pair("__bor", "__or__"),
	std::make_pair("__bxor", "__xor__"),
	std::make_pair("__shl", "__lshift__"),
	std::make_pair("__shr", "__rshift__"),
	std::make_pair("__concat", "__matmul__"),
	std::make_pair("__eq", "__eq__"),
	std::make_pair("__lt", "__lt__"),
	std::make_pair("__le", "__le__"),

	std::make_pair("__unm", "__neg__"),
	std::make_pair("__bnot", "__invert__"),
	std::make_pair("__len", "__len__"),
	std::make_pair("__index", "__getitem__"),
	std::make_pair("__newindex", "__setitem__"),
	std::make_pair("__call", "__call__"),
	std::make_pair("__close", "__close__"),
	//std::make_pair("__gc", "__gc__"),	// This one is special, so it shouldn't be set through the loop that is used for this list.
	std::make_pair("__tostring", "__repr__"),
};
// }}}

// Python-accessible methods. {{{
PyMethodDef Lua::methods[] = { // {{{
	{"set", reinterpret_cast <PyCFunction>(set_method), METH_VARARGS, "Set a variable"},
	{"run", reinterpret_cast <PyCFunction>(run_method), METH_VARARGS | METH_KEYWORDS, "Run a Lua script"},
	{"run_file", reinterpret_cast <PyCFunction>(run_file_method), METH_VARARGS | METH_KEYWORDS, "Run a Lua script from a file"},
	{"module", reinterpret_cast <PyCFunction>(module_method), METH_VARARGS, "Import a module into Lua"},
	{nullptr, nullptr, 0, nullptr}
}; // }}}

PyObject *Lua::set_method(Lua *self, PyObject *args) { // {{{
	char const *name;
	PyObject *value;
	if (!PyArg_ParseTuple(args, "sO", &name, &value))
		return nullptr;
	self->set(name, value);
	Py_RETURN_NONE;
} // }}}

PyObject *Lua::run_method(Lua *self, PyObject *args, PyObject *keywords) { // {{{
	char const *code;
	char const *description = nullptr;
	bool keep_single = false;
	char const *var = nullptr;
	PyObject *value = nullptr;
	char const *keywordnames[] = {"code", "description", "keep_single", "var", "value", nullptr};
	if (!PyArg_ParseTupleAndKeywords(args, keywords, "s|ssOp", const_cast <char **>(keywordnames), &code, &description, &var, &value, &keep_single))
		return nullptr;
	if (!description)
		description = code;
	if (var)
		self->set(var, value);
	return self->run(code, description, keep_single);
} // }}}

PyObject *Lua::run_file_method(Lua *self, PyObject *args, PyObject *keywords) { // {{{
	char const *filename;
	char const *description = nullptr;
	bool keep_single = false;
	char const *var = nullptr;
	PyObject *value = nullptr;
	char const *keywordnames[] = {"filename", "description", "keep_single", "var", "value", nullptr};
	if (!PyArg_ParseTupleAndKeywords(args, keywords, "s|ssOp", const_cast <char **>(keywordnames), &filename, &description, &var, &value, &keep_single))
		return nullptr;
	if (!description)
		description = filename;
	if (var)
		self->set(var, value);
	return self->run_file(filename, description, keep_single);
} // }}}

PyObject *Lua::module_method(Lua *self, PyObject *args) { // {{{
	char const *name;
	PyObject *dict;
	if (!PyArg_ParseTuple(args, "sO", &name, &dict))
		return nullptr;
	self->load_module(name, dict);
	Py_RETURN_NONE;
} // }}}
// }}}

// Lua callback for all userdata metamethods. (Method selection is done via operator name stored in upvalue.)
int Lua::metamethod(lua_State *state) { // {{{
	// This function is called from Lua through a metatable.
	// Get self pointer.
	lua_getfield(state, LUA_REGISTRYINDEX, "self");
	Lua *lua = reinterpret_cast <Lua *> (lua_touserdata(state, -1));
	lua_pop(state, 1);

	// 1 upvalues: python method name.
	char const *python_op = lua_tostring(state, lua_upvalueindex(1));

	PyObject *target = lua->to_python(1);
	PyObject *method = PyMapping_GetItemString(target, python_op);

	// Create arguments tuple
	int nargs = lua_gettop(state);
	PyObject *args = PyTuple_New(nargs - 1);
	for (int i = 2; i <= nargs; ++i) {
		PyObject *arg = lua->to_python(i);
		// fill tuple
		PyTuple_SET_ITEM(args, i - 2, arg);	// Steals reference to arg.
	}

	// Call function.
	lua->push(PyObject_CallObject(method, args));
	Py_DECREF(args);
	return 1;
} // }}}

int Lua::gc(lua_State *state) { // {{{
	lua_getfield(state, LUA_REGISTRYINDEX, "self");
	PyObject *obj = reinterpret_cast <PyObject *>(lua_touserdata(state, -1));
	lua_pop(state, 1);
	Py_DECREF(obj);
	return 0;
} // }}}

// class Lua __new__ function.
PyObject *Lua::create(PyTypeObject *type, PyObject *args, PyObject *kwds) { // {{{
	Lua *self = reinterpret_cast <Lua *>(type->tp_alloc(type, 0));
	if (!self)
		return nullptr;
	return reinterpret_cast <PyObject *>(self);
} // }}}

// Destructor.
void Lua::dealloc(Lua *self) { // {{{
	lua_close(self->state);
	LuaType.tp_free(reinterpret_cast <PyObject *>(self));
} // }}}

// set variable in Lua.
void Lua::set(std::string const &name, PyObject *value) { // {{{
	lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	push(value);
	lua_setfield(state, -2, name.c_str());
	lua_settop(state, -2);
} // }}}

// run code after having loaded the buffer (internal use only).
PyObject *Lua::run_code(int pos, bool keep_single) { // {{{
	lua_call(state, 0, LUA_MULTRET);
	int size = lua_gettop(state) - pos;
	PyObject *ret;
	if (keep_single || size > 1) {
		ret = PyTuple_New(size);
		for (int i = 0; i < size; ++i)
			PyTuple_SET_ITEM(ret, i, to_python(-size + i));
	}
	else if (size == 1)
		ret = to_python(-1);
	else
		ret = Py_None;
	lua_settop(state, pos);
	return ret;
} // }}}

// run string in lua.
PyObject *Lua::run(std::string const &cmd, std::string const &description, bool keep_single) { // {{{
	int pos = lua_gettop(state);
	if (luaL_loadbufferx(state, cmd.data(), cmd.size(), description.c_str(), nullptr) != LUA_OK) {
		PyErr_SetString(PyExc_ValueError, lua_tostring(state, -1));
		return nullptr;
	}
	return run_code(pos, keep_single);
} // }}}

// run file in lua.
PyObject *Lua::run_file(std::string const &filename, std::string const &description, bool keep_single) { // {{{
	int pos = lua_gettop(state);
	if (luaL_loadfilex(state, filename.c_str(), nullptr) != LUA_OK) {
		PyErr_SetString(PyExc_ValueError, lua_tostring(state, -1));
		return nullptr;
	}
	return run_code(pos, keep_single);
} // }}}

// load module into lua.
void Lua::load_module(std::string const &name, PyObject *dict) { // {{{
	if (!PyDict_Check(dict)) {
		PyObject *new_dict = PyDict_New();	// new reference
		PyObject *dir = PyObject_Dir(dict);	// new reference
		Py_ssize_t size = PyList_Size(dir);
		for (Py_ssize_t i = 0; i < size; ++i) {
			PyObject *key = PyList_GetItem(dir, i);	// borrowed reference
			Py_ssize_t len;
			char const *byteskey = PyUnicode_AsUTF8AndSize(key, &len);
			if (len > 0 && byteskey[0] == '_') {
				if (len < 5 || std::string(byteskey, 5) != "_lua_")
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
	push(package_loaded);
	push_luatable(dict);
	lua_setfield(state, -2, name.c_str());
	lua_settop(state, -2);
} // }}}

// load variable from lua stack into python.
PyObject *Lua::to_python(int index) { // {{{
	int type = lua_type(state, index);
	switch(type) {
	case LUA_TNIL:
		Py_RETURN_NONE;
	case LUA_TBOOLEAN:
		return PyBool_FromLong(lua_toboolean(state, index));
	//case LUA_TLIGHTUSERDATA: // Not used.
	//	return lua_touserdata(state, index)
	case LUA_TNUMBER:
	{
		// If this is exactly an integer, return it as an integer.
		lua_Integer iret = lua_tointeger(state, index);
		lua_Number ret = lua_tonumber(state, index);
		if (lua_Number(iret) == ret)
			return PyLong_FromLongLong(iret);
		return PyFloat_FromDouble(ret);
	}
	case LUA_TSTRING:
	{
		size_t len;
		char const *str = lua_tolstring(state, index, &len);
		return PyUnicode_DecodeUTF8(str, len, nullptr);
	}
	case LUA_TTABLE:
		lua_pushvalue(state, index);
		return Table::create(this);
	case LUA_TFUNCTION:
		lua_pushvalue(state, index);
		return Function::create(this);
	case LUA_TUSERDATA:
		// This is a Python-owned object that was used by Lua.
		// The data block of the userdata stores the PyObject *.
		return *reinterpret_cast <PyObject **>(lua_touserdata(state, index));
	//case LUA_TTHREAD: // Not used.
	//	return lua_tothread(state, index)
	default:
		//if (lua_iscfunction(state, index)) {
		//	return lua_tocfunction(state, index)
		//}
		return PyErr_Format(PyExc_ValueError, "Invalid type %x passed to to_python", type);
	}
} // }}}

// load variable from python onto lua stack.
void Lua::push(PyObject *obj) { // {{{
	if (obj == Py_None)
		lua_pushnil(state);
	else if (PyBool_Check(obj))
		lua_pushboolean(state, obj == Py_True);
	else if (PyLong_Check(obj))
		lua_pushinteger(state, PyLong_AsLongLong(obj));
	else if (PyUnicode_Check(obj)) {
		// A str is encoded as bytes in Lua; bytes is wrapped as an object.
		Py_ssize_t len;
		char const *str = PyUnicode_AsUTF8AndSize(obj, &len);
		lua_pushlstring(state, str, len);
	}
	else if (PyFloat_Check(obj))
		lua_pushnumber(state, PyFloat_AsDouble(obj));
	else if (PyObject_TypeCheck(obj, &TableType))
		lua_rawgeti(state, LUA_REGISTRYINDEX, reinterpret_cast <Table *>(obj)->id);
	else if (PyObject_TypeCheck(obj, &FunctionType))
		lua_rawgeti(state, LUA_REGISTRYINDEX, reinterpret_cast <Function *>(obj)->id);
	else {
		*reinterpret_cast <PyObject **>(lua_newuserdatauv(state, sizeof(PyObject *), 0)) = obj;
		Py_INCREF(obj);
		// FIXME: use gc metamethod to DECREF the object.
		
		lua_getfield(state, LUA_REGISTRYINDEX, "metatable");
		lua_setmetatable(state, -2);
	}
} // }}}

// Create (native) Lua table on Lua stack from items in Python list or dict.
void Lua::push_luatable(PyObject *obj) { // {{{
	if (PyDict_Check(obj)) {
		lua_createtable(state, 0, PyDict_Size(obj));	// Pushes new table on the stack.
		Py_ssize_t ppos = 0;
		PyObject *key;
		PyObject *value;
		while (PyDict_Next(obj, &ppos, &key, &value)) {	// Key and value become borrowed references.
			push(key);
			push(value);
			lua_rawset(state, -3); // Pops key and value from the stack.
		}
	}
	else if (PySequence_Check(obj)) {
		lua_createtable(state, PySequence_Size(obj), 0);	// Pushes new table on the stack.
		Py_ssize_t size = PySequence_Size(obj);
		for (Py_ssize_t i = 0; i < size; ++i) {
			PyObject *value = PySequence_GetItem(obj, i);	// New reference.
			push(value);
			Py_DECREF(value);
			lua_rawseti(state, -2, i + 1);
		}
	}
	else {
		std::abort();
	}
} // }}}

// Constructor.
Lua::Lua(bool debug, bool loadlib, bool searchers, bool doloadfile, bool io, bool os, bool python_module) { // {{{
	// Create a new lua object.
	// This object provides the interface into the lua library.
	// It also provides access to all the symbols that lua owns.

	// Create new state and store back pointer to self.
	state = luaL_newstate();
	lua_pushlightuserdata(state, this);
	lua_setfield(state, LUA_REGISTRYINDEX, "self");

	// Open standard libraries. Many of them are closed again below.
	luaL_openlibs(state);

	// Store operators, because the API does not define them. {{{
	// Binary operators.
	for (auto pair: opnames) {
		char const * const &op = pair.first;
		char const * const &name = pair.second;
		ops[name] = run(std::string("return function(a, b) return a ") + op + " b end", std::string("get ") + name, false);
	}

	// Unary operators.
	ops["neg"] = run("return function(a) return -a end", std::string("get neg"), false);
	ops["invert"] = run("return function(a) return ~a end", std::string("get invert"), false);
	ops["repr"] = run("return function(a) return tostring(a) end", std::string("get repr"), false);
	// Skipped: len, getitem, setitem, delitem, because they have API calls which are used.
	// }}}

	// Store a copy of some initial values, so they still work if the original value is replaced.
	table_remove = run("return table.remove", "get table.remove", false);
	package_loaded = run("return package.loaded", "get package.loaded", false);
	_G = run("return _G", "get _G", false);

	// Set up metatable for userdata objects. {{{
	lua_createtable(state, 0, lua2python.size() + 1);
	for (auto op: lua2python) {
		// Set upvalue to python method name.
		lua_pushstring(state, op.second);

		// Create metatable function.
		lua_pushcclosure(state, metamethod, 1);
		lua_setfield(state, -2, op.first);
	}
	// Set gc metamethod. This is not passed through to Python, but instead cleans up the PyObject * reference.
	lua_pushcclosure(state, gc, 0);
	lua_setfield(state, -2, "__gc");

	// Set the metatable in the registry, for use by new userdata objects.
	lua_setfield(state, LUA_REGISTRYINDEX, "metatable");
	// }}}

	// Disable optional features that have not been requested. {{{
	if (!debug)
		run("debug = nil package.loaded.debug = nil", "disabling debug", false);
	if (!loadlib)
		run("package.loadlib = nil", "disabling loadlib", false);
	if (!searchers)
		run("package.searchers = {}", "disabling searchers", false);
	if (!doloadfile)
		run("loadfile = nil dofile = nil", "disabling loadfile and dofile", false);
	if (!os)
		run("os = {clock = os.clock, date = os.date, difftime = os.difftime, setlocale = os.setlocale, time = os.time} package.loaded.os = os", "disabling some of os", false);
	if (!io)
		run("io = nil package.loaded.io = nil", "disabling io", false);
	// }}}

	/* Add access to Python object constructors from Lua (unless disabled).
	   TODO when Table interface has been implemented.
	if (python_module) {
# Lua module for accessing some Python parts from Lua. This is prepared as a "python" module unless disabled. {{{
def construct_bytes(arg):
	'Implementation of python.bytes()'
	if isinstance(arg, Table):
		return bytes(arg.list())
	if isinstance(arg, str):
		return arg.encode('utf-8')
	return bytes(arg)

python = {
	'list': lambda table: table.list(),
	'dict': lambda table: table.dict(),
	'bytes': construct_bytes
}
# }}}

		create_lua_module("python", Py_BuildValue("{sO sO sO}",
					"list", table.list,
					"dict", table.dict,
					"bytes", construct_bytes));
	}*/
} // }}}
// }}}

// class Function implementation. {{{
// Python-accessible methods.
PyMethodDef Function::methods[] = { // {{{
	{"__call__", reinterpret_cast <PyCFunction>(call_method), METH_VARARGS | METH_KEYWORDS, "Call the Lua function"},
	{nullptr, nullptr, 0, nullptr}
}; // }}}

// __new__ Function.
PyObject *Function::create(Lua *context) { // {{{
	Function *self = reinterpret_cast <Function *>(FunctionType.tp_alloc(&FunctionType, 0));
	if (!self)
		return nullptr;
	self->lua = context;
	Py_INCREF(self->lua);
	self->id = luaL_ref(self->lua->state, LUA_REGISTRYINDEX);
	return reinterpret_cast <PyObject *>(self);
}; // }}}

// Destructor.
void Function::dealloc(Function *self) { // {{{
	luaL_unref(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Py_DECREF(self->lua);
	FunctionType.tp_free(reinterpret_cast <PyObject *>(self));
} // }}}

PyObject *Function::call_method(Function *self, PyObject *args, PyObject *keywords) { // {{{

	// Parse keep_single argument.
	bool keep_single = false;
	PyObject *kw = PyDict_GetItemString(keywords, "keep_single");	// Returns borrowed reference.
	Py_ssize_t size = PyDict_Size(keywords);
	if (kw) {
		if (!PyBool_Check(kw)) {
			PyErr_SetString(PyExc_ValueError, "keep_single argument must be of bool type");
			return nullptr;
		}
		keep_single = kw == Py_True;
		size -= 1;
	}
	if (size > 0) {
		PyErr_SetString(PyExc_ValueError, "only keep_single is supported as a keyword argument");
		return nullptr;
	}

	// Push target function to stack.
	int pos = lua_gettop(self->lua->state);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);

	// Push arguments to stack.
	assert(PyList_Check(args));
	Py_ssize_t nargs = PyList_Size(args);
	for (Py_ssize_t a = 0; a < nargs; ++a) {
		PyObject *arg = PyList_GetItem(args, a);	// Borrowed reference.
		self->lua->push(arg);
	}
	lua_call(self->lua->state, nargs, LUA_MULTRET);
	size = lua_gettop(self->lua->state) - pos;
	if (!keep_single && size < 2) {
		if (size == 0)
			Py_RETURN_NONE;
		// Size is 1.
		return self->lua->to_python(-1);
	}
	PyObject *ret = PyTuple_New(size);
	for (int i = 0; i < size; ++i) {
		PyObject *value = self->lua->to_python(-size + i);	// New reference.
		PyTuple_SET_ITEM(ret, i, value);	// Steal reference.
	}
	lua_settop(self->lua->state, pos);
	return ret;
} // }}}
// }}}

// class Table implementation. {{{
// Python-accessible methods.
PyMethodDef Table::methods[] = { // {{{
	{"__len__", reinterpret_cast <PyCFunction>(len_method), METH_VARARGS, "Call the # operator on the Lua table"},
	{"__iadd__", reinterpret_cast <PyCFunction>(iadd_method), METH_VARARGS, "Call the += operator on the Lua table"},
	{"__getitem__", reinterpret_cast <PyCFunction>(getitem_method), METH_VARARGS, "Get item from Lua table"},
	{"__setitem__", reinterpret_cast <PyCFunction>(setitem_method), METH_VARARGS, "Set item in Lua table"},
	{"__delitem__", reinterpret_cast <PyCFunction>(delitem_method), METH_VARARGS, "Remove item from Lua table"},
	{"__contains__", reinterpret_cast <PyCFunction>(contains_method), METH_VARARGS, "Check if Lua table contains item"},
	{"__iter__", reinterpret_cast <PyCFunction>(iter_method), METH_VARARGS, "Loop over the Lua table"},
	{"__ne__", reinterpret_cast <PyCFunction>(ne_method), METH_VARARGS, "Call the ~= operator on the Lua table"},
	{"__gt__", reinterpret_cast <PyCFunction>(gt_method), METH_VARARGS, "Call the > operator on the Lua table"},
	{"__ge__", reinterpret_cast <PyCFunction>(ge_method), METH_VARARGS, "Call the >= operator on the Lua table"},
	{"dict", reinterpret_cast <PyCFunction>(dict_method), METH_VARARGS, "Create dict from Lua table"},
	{"list", reinterpret_cast <PyCFunction>(list_method), METH_VARARGS, "Create list from Lua table"},
	{"pop", reinterpret_cast <PyCFunction>(pop_method), METH_VARARGS, "Remove item from Lua table"},
	{nullptr, nullptr, 0, nullptr}
}; // }}}

PyObject *Table::create(Lua *context) { // {{{
	Table *self = reinterpret_cast <Table *>(TableType.tp_alloc(&TableType, 0));
	if (!self)
		return nullptr;
	self->lua = context;
	Py_INCREF(self->lua);
	self->iter_ref = LUA_NOREF;
	self->id = luaL_ref(self->lua->state, LUA_REGISTRYINDEX);
	for (auto p: context->lua2python)
		PyObject_SetAttrString(reinterpret_cast <PyObject *>(self), p.second, context->ops[p.first]);
	return reinterpret_cast <PyObject *>(self);
}; // }}}

// Destructor.
void Table::dealloc(Table *self) { // {{{
	luaL_unref(self->lua->state, LUA_REGISTRYINDEX, self->id);
	Py_DECREF(self->lua);
	TableType.tp_free(reinterpret_cast <PyObject *>(self));
} // }}}

PyObject *Table::iadd_method(Table *self, PyObject *args) { // {{{
	PyObject *iterator;
	if (!PyArg_ParseTuple(args, "O", &iterator))	// borrowed reference.
		return nullptr;
	if (!PyIter_Check(iterator)) {
		PyErr_SetString(PyExc_TypeError, "argument must be iterable");
		return nullptr;
	}
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	lua_len(self->lua->state, -1);	// Length pushed.
	Py_ssize_t length = lua_tointeger(self->lua->state, -1);
	lua_pop(self->lua->state, 1);	// Length popped.
	PyObject *item;
	while ((item = PyIter_Next(iterator))) {
		self->lua->push(item);
		lua_seti(self->lua->state, -2, length + 1);
		length += 1;
		Py_DECREF(item);
	}
	Py_DECREF(iterator);
	lua_pop(self->lua->state, 1);
	return reinterpret_cast <PyObject *>(self);
} // }}}

PyObject *Table::getitem_method(Table *self, PyObject *args) { // {{{
	PyObject *key;
	if (!PyArg_ParseTuple(args, "O", &key))	// borrowed reference.
		return nullptr;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	self->lua->push(key);	// Key pushed.
	lua_gettable(self->lua->state, -2);	// Key replaced by value.
	PyObject *ret = self->lua->to_python(-1);
	lua_pop(self->lua->state, 2);
	if (ret == Py_None) {
		PyErr_Format(PyExc_IndexError, "Key %s does not exist in Lua table", PyObject_Str(key));
		Py_DECREF(ret);
		return nullptr;
	}
	return ret;
} // }}}

PyObject *Table::setitem_method(Table *self, PyObject *args) { // {{{
	PyObject *key;
	PyObject *value;
	if (!PyArg_ParseTuple(args, "OO", &key, &value))	// borrowed references.
		return nullptr;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	self->lua->push(key);
	self->lua->push(value);
	lua_settable(self->lua->state, -3);	// Pops key and value from stack
	lua_pop(self->lua->state, 1);
	Py_DECREF(key);
	Py_DECREF(value);
	Py_RETURN_NONE;
} // }}}

PyObject *Table::delitem_method(Table *self, PyObject *args) { // {{{
	PyObject *key;
	if (!PyArg_ParseTuple(args, "O", &key))	// borrowed reference.
		return nullptr;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	self->lua->push(key);	// Key pushed.
	// Raise IndexError if key doesn't exist: use getitem.
	self->lua->push(key);	// Key pushed.
	lua_gettable(self->lua->state, -2);	// Key replaced by value.
	if (lua_isnil(self->lua->state, -1)) {
		// Key did not exist in table.
		PyErr_Format(PyExc_IndexError, "Key %s does not exist in Lua table", PyObject_Str(key));
		lua_pop(self->lua->state, 2);
		return nullptr;
	}
	lua_pop(self->lua->state, 1);	// Old value popped.
	self->lua->push(key);	// Key pushed.
	lua_pushnil(self->lua->state);	// nil pushed.
	lua_settable(self->lua->state, -3);	// Pops key and value from stack
	Py_RETURN_NONE;
} // }}}

PyObject *Table::contains_method(Table *self, PyObject *args) { // {{{
	// Check if the argument exists in the table as a value (not a key).
	PyObject *value;
	if (!PyArg_ParseTuple(args, "O", &value))	// borrowed reference.
		return nullptr;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	lua_pushnil(self->lua->state);
	while (lua_next(self->lua->state, -2) != 0) {
		PyObject *candidate = self->lua->to_python(-1);
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

void Table::clear_iter() { // {{{
	if (iter_ref != LUA_REFNIL)
		luaL_unref(lua->state, LUA_REGISTRYINDEX, iter_ref);
	iter_ref = LUA_NOREF;
} // }}}

PyObject *Table::iter_method(Table *self, PyObject *args) { // {{{
	// Destroy pending iteration, if any.
	if (self->iter_ref != LUA_NOREF)
		self->clear_iter();
	self->iter_ref = LUA_REFNIL;
	// TODO: the iterator should really be a copy of the object, to allow multiple iterations simultaneously.
	Py_INCREF(self);
	return reinterpret_cast <PyObject *>(self);
} // }}}

PyObject *Table::next_method(Table *self, PyObject *args) { // {{{
	if (!PyArg_ParseTuple(args, ""))
		return nullptr;
	if (self->iter_ref == LUA_NOREF) {
		PyErr_SetString(PyExc_ValueError, "next called on invalid iterator");
		return nullptr;
	}
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);	// Table pushed.
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->iter_ref);	// previous key pushed.
	if (lua_next(self->lua->state, -2) == 0) {
		self->clear_iter();
		lua_pop(self->lua->state, 1);
		PyErr_SetNone(PyExc_StopIteration);
		return nullptr;
	}
	PyObject *ret = self->lua->to_python(-2);
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
} // }}}

PyObject *Table::ne_method(Table *self, PyObject *args) { // {{{
	PyObject *other;
	if (!PyArg_ParseTuple(args, "O", &other))	// borrowed reference.
		return nullptr;
	self->lua->push(self->lua->ops["__eq"]);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	self->lua->push(other);
	lua_call(self->lua->state, 2, 1);
	PyObject *inverted = self->lua->to_python(-1);
	lua_pop(self->lua->state, 1);
	PyObject *ret = PyBool_FromLong(PyObject_Not(inverted));
	Py_DECREF(inverted);
	return ret;
} // }}}

PyObject *Table::gt_method(Table *self, PyObject *args) { // {{{
	PyObject *other;
	if (!PyArg_ParseTuple(args, "O", &other))	// borrowed reference.
		return nullptr;
	self->lua->push(self->lua->ops["__lt"]);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	self->lua->push(other);
	lua_call(self->lua->state, 2, 1);
	PyObject *ret = self->lua->to_python(-1);
	lua_settop(self->lua->state, -1);
	return ret;
} // }}}

PyObject *Table::ge_method(Table *self, PyObject *args) { // {{{
	PyObject *other;
	if (!PyArg_ParseTuple(args, "O", &other))	// borrowed reference.
		return nullptr;
	self->lua->push(self->lua->ops["__le"]);
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	self->lua->push(other);
	lua_call(self->lua->state, 2, 1);
	PyObject *ret = self->lua->to_python(-1);
	lua_settop(self->lua->state, -1);
	return ret;
} // }}}

PyObject *Table::dict_method(Table *self, PyObject *args) { // {{{
	// Get a copy of the table as a dict
	PyObject *ret = PyDict_New();
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_pushnil(self->lua->state);
	while (lua_next(self->lua->state, -2) != 0) {
		PyObject *key = self->lua->to_python(-2);
		PyObject *value = self->lua->to_python(-1);
		bool fail = PyDict_SetItem(ret, key, value) < 0;
		Py_DECREF(key);
		Py_DECREF(value);
		if (fail) {
			Py_DECREF(ret);
			return nullptr;
		}
		lua_pop(self->lua->state, 1);
	}
	lua_pop(self->lua->state, 1);
	return ret;
} // }}}

PyObject *Table::list_method(Table *self, PyObject *args) { // {{{
	// Get a copy of (the sequence elements of) the table, as a list. note that table[1] becomes list[0].
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_len(self->lua->state, -1);
	Py_ssize_t length = lua_tointeger(self->lua->state, -1);
	lua_pop(self->lua->state, 1);
	PyObject *ret = PyList_New(length);
	for (Py_ssize_t i = 1; i <= length; ++i) {
		lua_rawgeti(self->lua->state, -1, i);
		PyObject *value = self->lua->to_python(-1);
		PyList_SET_ITEM(ret, i - 1, value);
		Py_DECREF(value);
		lua_pop(self->lua->state, 1);
	}
	lua_pop(self->lua->state, 1);
	return ret;
} // }}}

PyObject *Table::pop_method(Table *self, PyObject *args) { // {{{
	Py_ssize_t index = -1;
	if (!PyArg_ParseTuple(args, "|n", &index))
		return nullptr;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	if (index < 0) {
		lua_len(self->lua->state, -1);
		index += lua_tointeger(self->lua->state, -1);
		lua_pop(self->lua->state, 1);
	}
	self->lua->push(self->lua->table_remove);
	lua_pushinteger(self->lua->state, index);
	lua_call(self->lua->state, 1, 1);
	PyObject *ret = self->lua->to_python(-1);
	lua_pop(self->lua->state, 2);
	return ret;
} // }}}

PyObject *Table::len_method(Table *self, PyObject *args) { // {{{
	PyObject *other;
	if (!PyArg_ParseTuple(args, "O", &other))
		return nullptr;
	lua_rawgeti(self->lua->state, LUA_REGISTRYINDEX, self->id);
	lua_len(self->lua->state, -1);
	PyObject *ret = self->lua->to_python(-1);
	lua_pop(self->lua->state, 2);
	return ret;
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

// vim: set foldmethod=marker :
