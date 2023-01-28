# lua.py - Use Lua in Python programs
# Copyright 2012-2022 Bas Wijnen <wijnen@debian.org> {{{
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
# }}}

'''Module documentation {{{

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

}}}'''

# Imports. {{{
import sys
import os
import types
import ctypes
import struct
import numbers
import traceback
from .luaconst import *
# }}}

# Some workarounds to make this file usable in Python 2 as well as 3. FIXME: remove these; Python 2 is no longer supported. {{{
makebytes = lambda x: bytes(x, 'utf-8') if isinstance(x, str) else x
makestr = lambda x: str(x, 'utf-8') if isinstance(x, bytes) else x
# }}}

# Debugging settings. {{{
_DEBUGLEVEL = os.getenv('LUA_DEBUG')
if _DEBUGLEVEL:
	_DEBUGLEVEL = int(_DEBUGLEVEL)
else:
	_DEBUGLEVEL = None

def _dprint(text, level = 0):
	'''Print stuff for debugging. This function does nothing when not debugging'''
	# Set the following to the desired debugging level.
	if _DEBUGLEVEL is not None and level > _DEBUGLEVEL:
		sys.stderr.write(text + '\n')
# }}}

# Load shared library and add workarounds for older lua versions. {{{
# Allow users (or the calling program) to choose their lua version.
_library = ctypes.CDLL("liblua" + os.getenv('PYTHON_LUA_VERSION', '5.4') + ".so") # TODO: use .dll for Windows.
if not hasattr(_library, 'lua_len'):
	_library.lua_len = _library.lua_objlen
# }}}

# Module for accessing some Python parts from Lua. This prepared as a "python" module unless disabled. {{{
python = {
	'list': lambda table: table.list(),
	'dict': lambda table: table.dict()
}
# }}}

class Lua(object): # {{{
	# Lookup machinery. {{{
	_states = {}

	@classmethod
	def _lookup(cls, state):
		return cls._states[state.value]

	def __del__(self):
		del self._states[self._state.value]
	# }}}

	def __init__(self, debug = False, loadlib = False, searchers = False, doloadfile = False, io = False, os = False, python_module = True): # {{{
		'''Create a new lua object.
		This object provides the interface into the lua library.
		It also provides access to all the symbols that lua owns.'''

		_library.luaL_newstate.restype = ctypes.c_void_p
		self._state = ctypes.c_void_p(_library.luaL_newstate())
		assert self._state.value not in self._states

		# Load standard functions and set return types. {{{
		_library.luaL_openlibs(self._state)
		_library.lua_tolstring.restype = ctypes.c_char_p
		_library.lua_tonumberx.restype = ctypes.c_double
		_library.lua_touserdata.restype = ctypes.c_void_p
		_library.lua_topointer.restype = ctypes.c_void_p
		_library.lua_touserdata.restype = ctypes.c_void_p
		_library.lua_tothread.restype = ctypes.c_void_p
		_library.lua_tocfunction.restype = ctypes.c_void_p
		_library.lua_newuserdatauv.restype = ctypes.c_void_p
		# }}}

		# Set attributes. {{{
		self._objects = {}
		# Store back "pointer" to self.
		self._states[self._state.value] = self
		self._push(self._state)
		_library.lua_setfield(self._state, LUA_REGISTRYINDEX, b'self')
		# }}}

		# Store operators, because the API does not define them.
		ops = {
			'+': 'add',
			'-': 'sub',
			'*': 'mul',
			'/': 'truediv',
			'%': 'mod',
			'^': 'pow',
			'//': 'floordiv',
			'&': 'and',
			'|': 'or',
			'~': 'xor',
			'<<': 'lshift',
			'>>': 'rshift',
			'..': 'matmul',
			'==': 'eq',
			'<': 'lt',
			'<=': 'le'
		}
		self._ops = {}
		# Binary operators.
		for op, name in ops.items():
			self._ops[name] = self.run('return function(a, b) return a %s b end' % op, name = 'get %s' % name)
		# Unary operators.
		self._ops['neg'] = self.run('return function(a) return -a end', name = 'get neg')
		self._ops['invert'] = self.run('return function(a) return ~a end', name = 'get neg')
		self._ops['repr'] = self.run('return function(a) return tostring(a) end', name = 'get neg')
		# TODO?: __close, __gc
		# Skipped: len, getitem, setitem, delitem, because they have API calls which are used.

		# Store a copy of table.remove and package.loaded, so they still work if the original value is replaced.
		self._table_remove = self.run('return table.remove', name = 'get table.remove')
		self._package_loaded = self.run('return package.loaded', name = 'get package.loaded')

		# Set up metatable for userdata objects. {{{
		self._factory = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)
		ops = ('add', 'sub', 'mul', 'div', 'mod', 'pow', 'unm', 'idiv', 'band', 'bor', 'bxor', 'bnot', 'shl', 'shr', 'concat', 'len', 'eq', 'lt', 'le', 'index', 'newindex', 'call', 'close', 'gc', 'tostring')
		_library.lua_createtable(self._state, 0, len(ops))
		for op in ops:
			setattr(self, '_the_' + op, self._factory(globals()['_object_' + op]))
			_library.lua_pushcclosure(self._state, getattr(self, '_the_' + op), 0)
			_library.lua_setfield(self._state, -2, b'__' + makebytes(op))
		_library.lua_setfield(self._state, LUA_REGISTRYINDEX, b'metatable')
		# }}}

		# Disable optional features that have not been requested. {{{
		if not debug:
			self.run('debug = nil package.loaded.debug = nil', name = 'disabling debug')
		if not loadlib:
			self.run('package.loadlib = nil', name = 'disabling loadlib')
		if not searchers:
			self.run('package.searchers = {}', name = 'disabling searchers')
		if not doloadfile:
			self.run('loadfile = nil dofile = nil', name = 'disabling loadfile and dofile')
		if not os:
			self.run('os = {clock = os.clock, date = os.date, difftime = os.difftime, setlocale = os.setlocale, time = os.time} package.loaded.os = os', name = 'disabling some of os')
		if not io:
			self.run('io = nil package.loaded.io = nil', name = 'disabling io')
		# }}}

		# Add access to Python object constructors from Lua (unless disabled).
		if python_module is not False:
			self.module('python', python)
	# }}}

	def run_file(self, script, keep_single = False): # {{{
		_dprint('running lua file %s' % script, 1)
		pos = _library.lua_gettop(self._state)
		if _library.luaL_loadfilex(self._state, makebytes(script), None) != LUA_OK:
			raise ValueError(makestr(_library.lua_tolstring(self._state, 1, None)))
		ret = _library.lua_pcallk(self._state, 0, LUA_MULTRET, None, 0, None)
		if ret == 0:
			size = _library.lua_gettop(self._state) - pos
			ret = [self._to_python(-size + i) for i in range(size)]
			_library.lua_settop(self._state, pos)
			if not keep_single:
				if len(ret) == 0:
					ret = None
				elif len(ret) == 1:
					ret = ret[0]
			return ret
		else:
			ret = makestr(_library.lua_tolstring(self._state, 1, None))
			_library.lua_settop(self._state, pos)
			raise ValueError(ret)
	# }}}

	def run(self, script = None, var = None, value = None, name = 'python string', keep_single = False): # {{{
		_dprint('running lua script %s with var %s' % (name, str(var)), 5)
		if var is not None:
			_library.lua_rawgeti(self._state, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS)
			self._push(value)
			var = makebytes(var)
			_library.lua_setfield(self._state, -2, var)
			_library.lua_settop(self._state, -2)
		if script is not None:
			pos = _library.lua_gettop(self._state)
			script = makebytes(script)
			if _library.luaL_loadbufferx(self._state, script, len(script), name, None) != LUA_OK:
				raise ValueError(makestr(_library.lua_tolstring(self._state, 1, None)))
			ret = _library.lua_pcallk(self._state, 0, LUA_MULTRET, None, 0, None)
			if ret == 0:
				size = _library.lua_gettop(self._state) - pos
				ret = [self._to_python(-size + i) for i in range(size)]
				_library.lua_settop(self._state, pos)
				if not keep_single:
					if len(ret) == 0:
						ret = None
					elif len(ret) == 1:
						ret = ret[0]
				return ret
			else:
				ret = _library.lua_tolstring(self._state, -1, None)
				_library.lua_settop(self._state, pos)
				raise ValueError(makestr(ret))
	# }}}

	def module(self, name, value): # {{{
		_dprint('creating lua module %s' % name, 5)
		if not isinstance(value, (list, dict)):
			# module object must be a table, so convert the object to a table.
			module = {}
			for key in dir(value):
				k = makebytes(key)
				if k.startswith(b'_'):
					if k.startswith(b'_lua'):
						k = b'_' + k[4:]
					else:
						continue
				module[k] = getattr(value, key)
			value = module
		self._push(self._package_loaded)
		self._push_luatable(value)
		n = makebytes(name)
		_library.lua_setfield(self._state, -2, n)
		_library.lua_settop(self._state, -2)
	# }}}

	def _to_python(self, index): # {{{
		_dprint('creating python value from lua at %d' % index, 1)
		type = _library.lua_type(self._state, index)
		if type == LUA_TNIL:
			return None
		elif type == LUA_TBOOLEAN:
			return bool(_library.lua_toboolean(self._state, index))
		elif type == LUA_TLIGHTUSERDATA:
			return _library.lua_touserdata(self._state, index)
		elif type == LUA_TNUMBER:
			ret = _library.lua_tonumberx(self._state, index, None)
			if int(ret) == ret:
				return int(ret)
			return ret
		elif type == LUA_TSTRING:
			return makestr(_library.lua_tolstring(self._state, index, None))
		elif type == LUA_TTABLE:
			_dprint('creating table', 1)
			_library.lua_pushvalue(self._state, index)
			return Table(self)
		elif type == LUA_TFUNCTION:
			_library.lua_pushvalue(self._state, index)
			return Function(self)
		elif type == LUA_TUSERDATA:
			id = _library.lua_touserdata(self._state, index)
			return self._objects[id]
		elif type == LUA_TTHREAD:
			return _library.lua_tothread(self._state, index)
		elif _library.lua_iscfunction(self._state, index):
			return _library.lua_tocfunction(self._state, index)
		else:
			raise AssertionError('unexpected lua type %d' % type)
	# }}}

	def _push(self, obj): # {{{
		_dprint('pushing %s (%s) to lua stack' % (str(obj), type(obj)), 1)
		if obj is None:
			_library.lua_pushnil(self._state)
		elif isinstance(obj, bool):
			_library.lua_pushboolean(self._state, obj)
		elif isinstance(obj, int):
			_library.lua_pushinteger(self._state, obj)
		elif isinstance(obj, (bytes, str)):
			obj = makebytes(obj)
			_library.lua_pushlstring(self._state, obj, len(obj))
		elif isinstance(obj, float):
			_library.lua_pushnumber(self._state, ctypes.c_double(obj))
		elif isinstance(obj, (Function, Table)):
			_dprint('pushing table', 1)
			_library.lua_rawgeti(self._state, LUA_REGISTRYINDEX, obj._id)
		elif isinstance(obj, object):
			id = _library.lua_newuserdatauv(self._state, 1, 1)
			self._objects[id] = obj
			_library.lua_getfield(self._state, LUA_REGISTRYINDEX, b'metatable')
			_library.lua_setmetatable(self._state, -2)
		else:
			# This shouldn't be possible: everything is an object.
			raise ValueError('object of type %s cannot be converted to lua object' % str(type(obj)))
		_dprint('done pushing', 1)
	# }}}

	def _push_luatable(self, table): # {{{
		'''Push a real lua table to the stack; not a userdata emulating a table. Table must be a list or dict'''
		_dprint('pushing lua table', 1)
		if isinstance(table, dict):
			_library.lua_createtable(self._state, 0, len(table))
			for i in table:
				self._push(i)
				self._push(table[i])
				_library.lua_settable(self._state, -3)
		else:
			_library.lua_createtable(self._state, len(table), 0)
			for i in range(len(table)):
				self._push(table[i])
				_library.lua_rawseti(self._state, -2, i + 1)
		_dprint('done pushing lua table', 1)
	# }}}

	def make_table(self, data = ()): # {{{
		return Table(self, data)
	# }}}
# }}}

# Operator definitions for using Python objects from Lua. {{{
def _op1(fn):
	def ret(state):
		self = Lua._lookup(ctypes.c_void_p(state))
		try:
			A = self._to_python(1)
			self._push(fn(A))
		except:
			_library.lua_settop(ctypes.c_void_p(state), 0)
			self._push(str(sys.exc_info()[1]))
			_library.lua_error(ctypes.c_void_p(state))
		return 1
	return ret

def _op2(fn):
	def ret(state):
		self = Lua._lookup(ctypes.c_void_p(state))
		try:
			A = self._to_python(1)
			B = self._to_python(2)
			self._push(fn(A, B))
		except:
			_library.lua_settop(ctypes.c_void_p(state), 0)
			self._push(str(sys.exc_info()[1]))
			_library.lua_error(ctypes.c_void_p(state))
		return 1
	return ret

@_op2
def _object_add(A, B): # + {{{
	_dprint('adding lua stuff', 1)
	return A + B
# }}}

@_op2
def _object_sub(A, B): # + {{{
	_dprint('subbing lua stuff', 1)
	return A - B
# }}}

@_op2
def _object_mul(A, B): # + {{{
	_dprint('mulling lua stuff', 1)
	return A * B
# }}}

@_op2
def _object_div(A, B): # + {{{
	_dprint('diving lua stuff', 1)
	return A / B
# }}}

@_op2
def _object_mod(A, B): # + {{{
	_dprint('modding lua stuff', 1)
	return A % B
# }}}

@_op2
def _object_pow(A, B): # + {{{
	_dprint('powing lua stuff', 1)
	return A ** B
# }}}

@_op1
def _object_unm(A): # + {{{
	_dprint('unming lua stuff', 1)
	return -A
# }}}

@_op2
def _object_idiv(A, B): # + {{{
	_dprint('idiving lua stuff', 1)
	return A // B
# }}}

@_op2
def _object_band(A, B): # + {{{
	_dprint('banding lua stuff', 1)
	return A & B
# }}}

@_op2
def _object_bor(A, B): # + {{{
	_dprint('powing lua stuff', 1)
	return A | B
# }}}

@_op2
def _object_bxor(A, B): # + {{{
	_dprint('bxoring lua stuff', 1)
	return A ^ B
# }}}

@_op1
def _object_bnot(A): # + {{{
	_dprint('bnotting lua stuff', 1)
	return ~A
# }}}

@_op2
def _object_shl(A, B): # + {{{
	_dprint('shling lua stuff', 1)
	return A << B
# }}}

@_op2
def _object_shr(A, B): # + {{{
	_dprint('shring lua stuff', 1)
	return A >> B
# }}}

@_op2
def _object_concat(A, B): # + {{{
	_dprint('concatting lua stuff', 1)
	# @ is matrix multiplication, but no other operator is available, so (ab)use it for concat.
	return A @ B
# }}}

@_op1
def _object_len(A): # + {{{
	_dprint('lenning lua stuff', 1)
	return len(A)
# }}}

@_op2
def _object_eq(A, B): # + {{{
	_dprint('eqing lua stuff', 1)
	return A == B
# }}}

@_op2
def _object_lt(A, B): # + {{{
	_dprint('neing lua stuff', 1)
	return A < B
# }}}

@_op2
def _object_le(A, B): # + {{{
	_dprint('leing lua stuff', 1)
	return A <= B
# }}}

def _object_index(state): # [] (rvalue) {{{
	_dprint('indexing lua stuff', 1)
	self = Lua._lookup(ctypes.c_void_p(state))
	A = self._to_python(1)
	B = self._to_python(2)
	_dprint('trying to index %s[%s]' % (str(A), str(B)), 2)
	if isinstance(B, (str, bytes)):
		B = makebytes(B)
		if b.startswith(B'_'):
			B = b'_lua' + B[1:]
	try:
		value = A[B]
		self._push(value)
		return 1
	except:
		sys.stderr.write("Trying to index nonexistent %s[%s], returning nil.\n" % (str(A), str(B)))
		self._push(None)
		return 1
# }}}

def _object_newindex(state): # [] (lvalue) {{{
	_dprint('newindexing lua stuff', 1)
	self = Lua._lookup(ctypes.c_void_p(state))
	table = self._to_python(1)
	key = self._to_python(2)
	value = self._to_python(3)
	try:
		if isinstance(key, str):
			if key.startswith('_'):
				key = '_lua' + key[1:]
		table[key] = value
		_dprint('set %s[%s] = %s' % (str(table), str(key), str(value)), 3)
		return 0
	except:
		sys.stderr.write('error trying to newindex %s[%s] = %s: %s\n' % (str(table), str(key), str(value), sys.exc_info()[1]))
		_library.lua_settop(ctypes.c_void_p(state), 0)
		self._push(str(sys.exc_info()[1]))
		_library.lua_error(ctypes.c_void_p(state))
# }}}

def _object_call(state): # () {{{
	_dprint('calling lua stuff', 1)
	self = Lua._lookup(ctypes.c_void_p(state))
	obj = self._to_python(1)
	num = _library.lua_gettop(ctypes.c_void_p(state)) - 1
	args = [self._to_python(i + 2) for i in range(num)]
	# Don't bother the user with two self arguments.
	if isinstance(obj, types.MethodType):
		assert obj.im_self is args[0]
		args = args[1:]
	try:
		ret = obj(*args)
		_library.lua_settop(ctypes.c_void_p(state), 0)
		if isinstance(ret, tuple):
			for i in ret:
				self._push(i)
			return len(ret)
		else:
			self._push(ret)
			return 1
	except:
		sys.stderr.write('Error: %s\n' % sys.exc_info()[1])
		bt = sys.exc_info()[2]
		while bt:
			sys.stderr.write('\t%s:%d %s\n' % (bt.tb_frame.f_code.co_filename, bt.tb_lineno, bt.tb_frame.f_code.co_name))
			bt = bt.tb_next
		_library.lua_settop(ctypes.c_void_p(state), 0)
		self._push(str(sys.exc_info()[1]))
		_library.lua_error(ctypes.c_void_p(state))
		# lua_error does not return.
# }}}

def _object_close(state): # {{{
	_dprint('closing lua stuff', 0)
	self = Lua._lookup(ctypes.c_void_p(state))
	id = _library.lua_touserdata(ctypes.c_void_p(state), 1)
	arg = self._to_python(-1)
	self._objects[id].__close__(arg)
	return 0
# }}}

def _object_gc(state): # {{{
	_dprint('cleaning lua stuff', 1)
	self = Lua._lookup(ctypes.c_void_p(state))
	id = _library.lua_touserdata(ctypes.c_void_p(state), 1)
	del self._objects[id]
	return 0
# }}}

def _object_tostring(state): # {{{
	_dprint('tostringing lua stuff', 1)
	self = Lua._lookup(ctypes.c_void_p(state))
	A = self._to_python(1)
	self._push(str(A))
	return 1
# }}}
# }}}

class Function: # Using Lua functions from Python. {{{
	'''Python interface to a lua function.
	The function is defined and owned by lua; this class makes it callable from Python.'''

	def __init__(self, lua): # {{{
		'''Create Python handle for Lua function.
		The function must have been pushed to the Lua stack before calling this.'''
		_dprint('creating lua function', 1)
		self._lua = lua
		self._id = _library.luaL_ref(self._lua._state, LUA_REGISTRYINDEX)
	# }}}

	def __call__(self, *args, **kwargs): # {{{
		_dprint('calling lua function', 3)
		if 'keep_single' in kwargs:
			keep_single = kwargs.pop('keep_single')
		else:
			keep_single = False
		assert len(kwargs) == 0
		pos = _library.lua_gettop(self._lua._state)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		for arg in args:
			self._lua._push(arg)
		if _library.lua_pcallk(self._lua._state, len(args), LUA_MULTRET, None, 0, None) != LUA_OK:
			msg = _library.lua_tolstring(self._lua._state, 1, None)
			sys.stderr.write('Error from lua: ' + makestr(msg) + '\n')
			raise ValueError('Error from lua: ' + makestr(msg))
		size = _library.lua_gettop(self._lua._state) - pos
		ret = [self._lua._to_python(-size + i) for i in range(size)]
		if not keep_single:
			if len(ret) == 0:
				ret = None
			elif len(ret) == 1:
				ret = ret[0]
		_library.lua_settop(self._lua._state, pos)
		return ret
	# }}}

	def __del__(self): # {{{
		_dprint('destroying lua function', 1)
		_library.luaL_unref(self._lua._state, LUA_REGISTRYINDEX, self._id)
	# }}}

# }}}

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
		self._id = _library.luaL_ref(self._lua._state, LUA_REGISTRYINDEX)
		for name, impl in self._lua._ops.items():
			setattr(self, '__' + name + '__', impl)
	# }}}

	def __len__(self): # {{{
		_dprint('requesting length of lua table', 3)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		_library.lua_len(self._lua._state, -1)
		ret = self._lua._to_python(-1)
		_library.lua_settop(self._lua._state, -3)
		return ret
	# }}}

	def __iadd__(self, other): # {{{
		_dprint('adding objects to lua table', 3)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		_library.lua_len(self._lua._state, -1)
		length = self._lua._to_python(-1)
		_library.lua_settop(self._lua._state, -2)
		for item in other:
			self._lua._push(item)
			_library.lua_seti(self._lua._state, -2, length + 1)
			length += 1
		_library.lua_settop(self._lua._state, -2)
		return self
	# }}}

	def __getitem__(self, key): # {{{
		_dprint('requesting item of lua table: %s' % key, 3)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		self._lua._push(makebytes(key))
		_library.lua_gettable(self._lua._state, -2)
		ret = self._lua._to_python(-1)
		_library.lua_settop(self._lua._state, -3)
		if ret is None:
			raise IndexError('Key %s does not exist in Lua table' % key)
		return ret
	# }}}

	def __setitem__(self, key, value): # {{{
		_dprint('setting item of lua table: %s: %s' % (key, value), 3)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		self._lua._push(makebytes(key))
		self._lua._push(value)
		_library.lua_settable(self._lua._state, -3)
		_library.lua_settop(self._lua._state, -2)
	# }}}

	def __delitem__(self, key): # {{{
		_dprint('deleting item of lua table: %s' % key, 3)
		# Raise IndexError if key doesn't exist: use getitem.
		self[key]
		self[key] = None
	# }}}

	def __contains__(self, key): # {{{
		_dprint('checking if %s is in lua table' % key, 3)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		_library.lua_pushnil(self._lua._state)
		while _library.lua_next(self._lua._state, -2) != 0:
			if makebytes(self._lua._to_python(-2)) == makebytes(key):
				_library.lua_settop(self._lua._state, -4)
				return True
			_library.lua_settop(self._lua._state, -2)
		_library.lua_settop(self._lua._state, -2)
		return False
	# }}}

	def __iter__(self): # {{{
		_dprint('iterating over lua table', 3)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		_library.lua_pushnil(self._lua._state)
		while _library.lua_next(self._lua._state, -2) != 0:
			ret = self._lua._to_python(-2)
			_library.lua_settop(self._lua._state, -2)
			ref = _library.luaL_ref(self._lua._state, LUA_REGISTRYINDEX)
			try:
				_library.lua_settop(self._lua._state, -2)
				yield ret
				_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
				_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, ref)
			finally:
				_library.luaL_unref(self._lua._state, LUA_REGISTRYINDEX, ref)
		_library.lua_settop(self._lua._state, -2)
	# }}}

	def __del__(self): # {{{
		_dprint('destroying lua table', 1)
		_library.luaL_unref(self._lua._state, LUA_REGISTRYINDEX, self._id)
	# }}}

	def __ne__(self, other): # {{{
		return not self == other
	# }}}

	def __gt__(self, other): # {{{
		return self._lua._ops['lt'](other, self)
	# }}}

	def __ge__(self, other): # {{{
		return self._lua._ops['le'](other, self)
	# }}}

	def dict(self): # {{{
		'Get a copy of the table as a dict'
		_dprint('get dict from lua table', 1)
		ret = {}
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		_library.lua_pushnil(self._lua._state)
		while _library.lua_next(self._lua._state, -2) != 0:
			ret[self._lua._to_python(-2)] = self._lua._to_python(-1)
			_library.lua_settop(self._lua._state, -2)
		_library.lua_settop(self._lua._state, -2)
		return ret
	# }}}

	def list(self): # {{{
		'Get a copy of the table, which must be a sequence, as a list. note that table[1] becomes list[0].'
		_dprint('get list from lua table', 1)
		_library.lua_rawgeti(self._lua._state, LUA_REGISTRYINDEX, self._id)
		_library.lua_len(self._lua._state, -1)
		length = self._lua._to_python(-1)
		_library.lua_settop(self._lua._state, -2)
		ret = [None] * length
		for i in range(1, length + 1):
			_library.lua_rawgeti(self._lua._state, -1, i)
			ret[i - 1] = self._lua._to_python(-1)
			_library.lua_settop(self._lua._state, -2)
		_library.lua_settop(self._lua._state, -2)
		return ret
	# }}}

	def pop(self, index = -1): # {{{
		'''Like list.pop'''
		_dprint('popping item %d of lua table.' % index, 3)
		if isinstance(index, int) and index < 0:
			index += len(self)
		return self._lua._table_remove(self, index)
	# }}}

# }}}

# vim: set foldmethod=marker :
