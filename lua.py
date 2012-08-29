import sys
import os
import types
import ctypes
import struct
import numbers

_DEBUGLEVEL = os.getenv ('LUA_DEBUG')
if _DEBUGLEVEL:
	_DEBUGLEVEL = int (_DEBUGLEVEL)
else:
	_DEBUGLEVEL = None

def _dprint (text, level = 0):
	'''Print stuff for debugging. This function does nothing when not debugging'''
	# Set the following to the desired debugging level.
	if _DEBUGLEVEL is not None and level > _DEBUGLEVEL:
		sys.stderr.write (text + '\n')

# option for multiple returns in `lua_pcall' and `lua_call'
LUA_MULTRET = -1

# pseudo-indices
LUA_REGISTRYINDEX = -10000
LUA_ENVIRONINDEX = -10001
LUA_GLOBALSINDEX = -10002
def lua_upvalueindex (i):
	return LUA_GLOBALSINDEX - i

# thread status; 0 is OK
LUA_YIELD = 1
LUA_ERRRUN = 2
LUA_ERRSYNTAX = 3
LUA_ERRMEM = 4
LUA_ERRERR = 5

# basic types
LUA_TNONE = -1

LUA_TNIL = 0
LUA_TBOOLEAN = 1
LUA_TLIGHTUSERDATA = 2
LUA_TNUMBER = 3
LUA_TSTRING = 4
LUA_TTABLE = 5
LUA_TFUNCTION = 6
LUA_TUSERDATA = 7
LUA_TTHREAD = 8

# minimum Lua stack available to a C (and thus python) function
LUA_MINSTACK = 20

# garbage-collection function and options
LUA_GCSTOP = 0
LUA_GCRESTART = 1
LUA_GCCOLLECT = 2
LUA_GCCOUNT = 3
LUA_GCCOUNTB = 4
LUA_GCSTEP = 5
LUA_GCSETPAUSE = 6
LUA_GCSETSTEPMUL = 7

# Debug API
# Event codes
LUA_HOOKCALL = 0
LUA_HOOKRET = 1
LUA_HOOKLINE = 2
LUA_HOOKCOUNT = 3
LUA_HOOKTAILRET = 4
# Event masks
LUA_MASKCALL = 1 << LUA_HOOKCALL
LUA_MASKRET = 1 << LUA_HOOKRET
LUA_MASKLINE = 1 << LUA_HOOKLINE
LUA_MASKCOUNT = 1 << LUA_HOOKCOUNT

class lua:
	_lib = ctypes.CDLL ("liblua5.1.so")
	_objects = {}
	def __init__ (self, debug = False):
		self._state = lua._lib.luaL_newstate ()
		lua._objects[self._state] = self
		lua._lib.luaL_openlibs (self._state)
		lua._lib.lua_tolstring.restype = ctypes.c_char_p
		lua._lib.lua_tonumber.restype = ctypes.c_double
		lua._lib.lua_touserdata.restype = ctypes.c_void_p
		lua._lib.lua_topointer.restype = ctypes.c_void_p
		lua._lib.lua_touserdata.restype = ctypes.c_void_p
		lua._lib.lua_tothread.restype = ctypes.c_void_p
		lua._lib.lua_tocfunction.restype = ctypes.c_void_p
		lua._lib.lua_newuserdata.restype = ctypes.c_void_p
		self._factory = ctypes.CFUNCTYPE (ctypes.c_int, ctypes.c_void_p)
		self._objects = {}
		self._the_add = self._factory (_object_add)
		self._the_sub = self._factory (_object_sub)
		self._the_mul = self._factory (_object_mul)
		self._the_div = self._factory (_object_div)
		self._the_mod = self._factory (_object_mod)
		self._the_pow = self._factory (_object_pow)
		self._the_unm = self._factory (_object_unm)
		self._the_concat = self._factory (_object_concat)
		self._the_len = self._factory (_object_len)
		self._the_eq = self._factory (_object_eq)
		self._the_lt = self._factory (_object_lt)
		self._the_le = self._factory (_object_le)
		self._the_get = self._factory (_object_get)
		self._the_put = self._factory (_object_put)
		self._the_call = self._factory (_object_call)
		self._the_gc = self._factory (_object_gc)
		self._the_tostring = self._factory (_object_tostring)
		lua._lib.lua_createtable (self._state, 0, 16)
		lua._lib.lua_pushcclosure (self._state, self._the_add, 0)
		lua._lib.lua_setfield (self._state, -2, '__add')
		lua._lib.lua_pushcclosure (self._state, self._the_sub, 0)
		lua._lib.lua_setfield (self._state, -2, '__sub')
		lua._lib.lua_pushcclosure (self._state, self._the_mul, 0)
		lua._lib.lua_setfield (self._state, -2, '__mul')
		lua._lib.lua_pushcclosure (self._state, self._the_div, 0)
		lua._lib.lua_setfield (self._state, -2, '__div')
		lua._lib.lua_pushcclosure (self._state, self._the_mod, 0)
		lua._lib.lua_setfield (self._state, -2, '__mod')
		lua._lib.lua_pushcclosure (self._state, self._the_pow, 0)
		lua._lib.lua_setfield (self._state, -2, '__pow')
		lua._lib.lua_pushcclosure (self._state, self._the_unm, 0)
		lua._lib.lua_setfield (self._state, -2, '__unm')
		lua._lib.lua_pushcclosure (self._state, self._the_concat, 0)
		lua._lib.lua_setfield (self._state, -2, '__concat')
		lua._lib.lua_pushcclosure (self._state, self._the_len, 0)
		lua._lib.lua_setfield (self._state, -2, '__len')
		lua._lib.lua_pushcclosure (self._state, self._the_eq, 0)
		lua._lib.lua_setfield (self._state, -2, '__eq')
		lua._lib.lua_pushcclosure (self._state, self._the_lt, 0)
		lua._lib.lua_setfield (self._state, -2, '__lt')
		lua._lib.lua_pushcclosure (self._state, self._the_le, 0)
		lua._lib.lua_setfield (self._state, -2, '__le')
		lua._lib.lua_pushcclosure (self._state, self._the_get, 0)
		lua._lib.lua_setfield (self._state, -2, '__index')
		lua._lib.lua_pushcclosure (self._state, self._the_put, 0)
		lua._lib.lua_setfield (self._state, -2, '__newindex')
		lua._lib.lua_pushcclosure (self._state, self._the_call, 0)
		lua._lib.lua_setfield (self._state, -2, '__call')
		lua._lib.lua_pushcclosure (self._state, self._the_gc, 0)
		lua._lib.lua_setfield (self._state, -2, '__gc')
		lua._lib.lua_pushcclosure (self._state, self._the_tostring, 0)
		lua._lib.lua_setfield (self._state, -2, '__tostring')
		lua._lib.lua_setfield (self._state, LUA_REGISTRYINDEX, 'metatable')
		if not debug:
			self.run ('debug = nil', name = 'disabling debug')
	def __del__ (self):
		del lua._objects[self._state]
	def run_file (self, script):
		_dprint ('running lua file %s' % script, 1)
		pos = lua._lib.lua_gettop (self._state)
		if lua._lib.luaL_loadfile (self._state, script) != 0:
			raise ValueError (lua._lib.lua_tolstring (self._state, 1, None))
		ret = lua._lib.lua_pcall (self._state, 0, LUA_MULTRET, None)
		if ret == 0:
			size = lua._lib.lua_gettop (self._state) - pos
			ret = [self._to_python (-size + i) for i in range (size)]
			lua._lib.lua_settop (self._state, pos)
			return ret
		else:
			ret = lua._lib.lua_tolstring (self._state, 1, None)
			lua._lib.lua_settop (self._state, pos)
			raise ValueError (ret)
	def run (self, script = None, var = None, value = None, name = 'python string'):
		_dprint ('running lua script %s with var %s' % (name, str (var)), 5)
		if var is not None:
			self._push (value)
			lua._lib.lua_setfield (self._state, LUA_GLOBALSINDEX, var)
		if script is not None:
			pos = lua._lib.lua_gettop (self._state)
			if lua._lib.luaL_loadbuffer (self._state, script, len (script), name) != 0:
				raise ValueError (lua._lib.lua_tolstring (self._state, 1, None))
			ret = lua._lib.lua_pcall (self._state, 0, LUA_MULTRET, None)
			if ret == 0:
				size = lua._lib.lua_gettop (self._state) - pos
				ret = [self._to_python (-size + i) for i in range (size)]
				lua._lib.lua_settop (self._state, pos)
				return ret
			else:
				ret = lua._lib.lua_tolstring (self._state, 1, None)
				lua._lib.lua_settop (self._state, pos)
				raise ValueError (ret)
	def module (self, name, value):
		_dprint ('creating lua module %s' % name, 5)
		if not isinstance (value, list) and not isinstance (value, dict):
			# module object must be a table, so convert the object to a table.
			module = {}
			for key in dir (value):
				if key.startswith ('_'):
					if key.startswith ('_lua'):
						k = '_' + key[4:]
					else:
						continue
				else:
					k = key
				module[k] = getattr (value, key)
			value = module
		self._push_luatable (value)
		lua._lib.lua_setfield (self._state, LUA_GLOBALSINDEX, name)
		self.run ('module ("%s")' % name, name = 'loading module %s' % name)
	def _to_python (self, index):
		_dprint ('creating python value from lua at %d' % index, 1)
		type = lua._lib.lua_type (self._state, index)
		if type == LUA_TNIL:
			return None
		elif type == LUA_TBOOLEAN:
			return bool (lua._lib.lua_toboolean (self._state, index))
		elif type == LUA_TLIGHTUSERDATA:
			return lua._lib.lua_touserdata (self._state, index)
		elif type == LUA_TNUMBER:
			return lua._lib.lua_tonumber (self._state, index)
		elif type == LUA_TSTRING:
			return lua._lib.lua_tolstring (self._state, index, None)
		elif type == LUA_TTABLE:
			_dprint ('creating table', 1)
			lua._lib.lua_pushvalue (self._state, index)
			return Table (self)
		elif type == LUA_TFUNCTION:
			lua._lib.lua_pushvalue (self._state, index)
			return Function (self)
		elif type == LUA_TUSERDATA:
			id = lua._lib.lua_touserdata (self._state, index)
			return self._objects[id]
		elif type == LUA_TTHREAD:
			return lua._lib.lua_tothread (self._state, index)
		elif lua._lib.lua_iscfunction (self._state, index):
			return lua._lib.lua_tocfunction (self._state, index)
		else:
			raise AssertionError ('unexpected lua type %d' % type)
	def _push (self, obj):
		_dprint ('pushing %s (%s) to lua stack' % (str (obj), type (obj)), 1)
		if obj is None:
			lua._lib.lua_pushnil (self._state)
		elif isinstance (obj, bool):
			lua._lib.lua_pushboolean (self._state, obj)
		elif isinstance (obj, int) or isinstance (obj, long):
			lua._lib.lua_pushinteger (self._state, obj)
		elif isinstance (obj, str):
			lua._lib.lua_pushlstring (self._state, obj, len (obj))
		elif isinstance (obj, float):
			lua._lib.lua_pushnumber (self._state, ctypes.c_double (obj))
		elif isinstance (obj, (Function, Table)):
			_dprint ('pushing table', 1)
			lua._lib.lua_rawgeti (self._state, LUA_REGISTRYINDEX, obj._id)
		elif isinstance (obj, object):
			o = {}
			for key in dir (obj):
				if key.startswith ('_'):
					if key.startswith ('_lua'):
						k = '_' + key[4:]
					else:
						continue
				else:
					k = key
				o[k] = getattr (obj, key)
			id = lua._lib.lua_newuserdata (self._state, 1)
			self._objects[id] = obj
			lua._lib.lua_getfield (self._state, LUA_REGISTRYINDEX, 'metatable')
			lua._lib.lua_setmetatable (self._state, -2)
		else:
			# This shouldn't be possible: everything is an object.
			raise ValueError ('object of type %s cannot be converted to lua object' % str (type (obj)))
		_dprint ('done pushing', 1)
	def _push_luatable (self, table):
		'''Push a real lua table to the stack; not a userdata emulating a table. Table must be a list or dict'''
		_dprint ('pushing lua table', 1)
		if isinstance (table, dict):
			lua._lib.lua_createtable (self._state, 0, len (table))
			for i in table:
				self._push (i)
				self._push (table[i])
				lua._lib.lua_settable (self._state, -3)
		else:
			lua._lib.lua_createtable (self._state, len (table), 0)
			for i in range (len (table)):
				self._push (table[i])
				lua._lib.lua_rawseti (self._state, -2, i + 1)
		_dprint ('done pushing lua table', 1)
	def make_table (self, data):
		return Table (self, data)

def _object_add (state):
	_dprint ('adding lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj + float (b))
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_sub (state):
	_dprint ('subtracting lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj - b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_mul (state):
	_dprint ('multiplying lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj * b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_div (state):
	_dprint ('dividing lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj / b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_mod (state):
	_dprint ('modulo lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj % b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_pow (state):
	_dprint ('powering lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj ** b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_unm (state):
	_dprint ('unary minussing lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		self._push (-obj)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_concat (state):
	_dprint ('concatenating lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj + str (b))
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_len (state):
	_dprint ('lenning lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		self._push (len (obj))
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_eq (state):
	_dprint ('equalling lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj == b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_lt (state):
	_dprint ('lessthanning lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj < b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_le (state):
	_dprint ('lessthanorequalling lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		self._push (obj <= b)
		return 1
	except:
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_get (state):
	_dprint ('getting lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		b = self._to_python (-1)
		_dprint ('trying to get %s[%s]' % (str (obj), str (b)), 2)
		if isinstance (b, str):
			if b.startswith ('_'):
				if b.startswith ('_lua'):
					self._push (obj['_' + b[4:]])
					return 1
				else:
					sys.stderr.write ('trying to get nonexistent object %s[%s]\n' % (str (obj), str (b)))
					self._push (None)
					return 1
			elif b in dir (obj):
				self._push (getattr (obj, b))
				return 1
		self._push (obj[b])
		return 1
	except:
		sys.stderr.write ('error trying to get %s[%s]: %s\n' % (str (obj), str (b), sys.exc_value))
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_put (state):
	_dprint ('putting lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	try:
		key = self._to_python (-2)
		value = self._to_python (-1)
		if isinstance (key, str):
			if key.startswith ('_'):
				setattr (obj, '_lua' + key[1:], value)
				_dprint ('set %s[%s] = %s' % (str (obj), str (key), str (value)), 3)
				return 0
		if isinstance (obj, dict):
			obj[key] = value
		else:
			setattr (obj, key, value)
		_dprint ('set %s[%s] = %s' % (str (obj), str (key), str (value)), 3)
		return 0
	except:
		sys.stderr.write ('error trying to put %s[%s] = %s: %s\n' % (str (obj), str (key), str (value), sys.exc_value))
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)

def _object_call (state):
	_dprint ('calling lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	obj = self._objects[id]
	num = lua._lib.lua_gettop (state) - 1
	args = [self._to_python (i + 2) for i in range (num)]
	# Don't bother the user with two self arguments.
	if isinstance (obj, types.MethodType):
		assert obj.im_self is args[0]
		args = args[1:]
	try:
		ret = obj (*args)
		lua._lib.lua_settop (state, 0)
		if isinstance (ret, tuple):
			for i in ret:
				self._push (i)
			return len (ret)
		else:
			self._push (ret)
			return 1
	except:
		sys.stderr.write ('Error: %s\n' % sys.exc_value)
		bt = sys.exc_info ()[2]
		while bt:
			sys.stderr.write ('\t%s:%d %s\n' % (bt.tb_frame.f_code.co_filename, bt.tb_lineno, bt.tb_frame.f_code.co_name))
			bt = bt.tb_next
		lua._lib.lua_settop (state, 0)
		self._push (str (sys.exc_value))
		lua._lib.lua_error (state)
		# lua_error does not return.

def _object_gc (state):
	_dprint ('cleaning lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	del self._objects[id]
	return 0

def _object_tostring (state):
	_dprint ('tostringing lua stuff', 1)
	self = lua._objects[state]
	id = lua._lib.lua_touserdata (state, 1)
	self._push (str (self._objects[id]))
	return 1

class Function:
	def __init__ (self, luaobject):
		_dprint ('creating lua function', 1)
		self._luaobject = luaobject
		self._id = lua._lib.luaL_ref (self._luaobject._state, LUA_REGISTRYINDEX)
	def __call__ (self, *args):
		_dprint ('calling lua function', 3)
		pos = lua._lib.lua_gettop (self._luaobject._state)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		for i in args:
			self._luaobject._push (i)
		if lua._lib.lua_pcall (self._luaobject._state, len (args), LUA_MULTRET, None) != 0:
			sys.stderr.write ('Error from lua: ' + lua._lib.lua_tolstring (self._luaobject._state, 1, None) + '\n')
		size = lua._lib.lua_gettop (self._luaobject._state) - pos
		ret = [self._luaobject._to_python (-size + i) for i in range (size)]
		lua._lib.lua_settop (self._luaobject._state, pos)
		return ret
	def __del__ (self):
		_dprint ('destroying lua function', 1)
		lua._lib.luaL_unref (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		#super (Function, self).__del__ (self)

class Table:
	def __init__ (self, luaobject, table = None):
		_dprint ('creating lua table', 1)
		self._luaobject = luaobject
		if table is not None:
			self._luaobject._push_luatable (table)
		self._id = lua._lib.luaL_ref (self._luaobject._state, LUA_REGISTRYINDEX)
	def __len__ (self):
		_dprint ('requesting length of lua table', 3)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		ret = lua._lib.lua_objlen (self._luaobject._state, -1)
		lua._lib.lua_settop (self._luaobject._state, -2)
		return ret
	def __iadd__ (self, other):
		_dprint ('adding objects to lua table', 3)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		l = lua._lib.lua_objlen (self._luaobject._state, -1)
		for i in other:
			self._luaobject._push (i)
			lua._lib.lua_rawseti (self._luaobject._state, -2, l + 1)
			l += 1
		lua._lib.lua_settop (self._luaobject._state, -2)
		return self
	def __getitem__ (self, key):
		_dprint ('requesting item of lua table: %s' % key, 3)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		self._luaobject._push (key)
		lua._lib.lua_gettable (self._luaobject._state, -2)
		ret = self._luaobject._to_python (-1)
		lua._lib.lua_settop (self._luaobject._state, -3)
		if ret is None:
			raise IndexError
		return ret
	def __setitem__ (self, key, value):
		_dprint ('setting item of lua table: %s: %s' % (key, value), 3)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		self._luaobject._push (key)
		self._luaobject._push (value)
		lua._lib.lua_settable (self._luaobject._state, -3)
		lua._lib.lua_settop (self._luaobject._state, -2)
	def __delitem__ (self, key):
		_dprint ('deleting item of lua table: %s' % key, 3)
		# Raise IndexError if key doesn't exist: use getitem.
		self[key]
		self[key] = None
	def __contains__ (self, key):
		_dprint ('checking if %s is in lua table' % key, 3)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		lua._lib.lua_pushnil (self._luaobject._state)
		while lua._lib.lua_next (self._luaobject._state, -2) != 0:
			if self._luaobject._to_python (-2) == key:
				lua._lib.lua_settop (self._luaobject._state, -4)
				return True
			lua._lib.lua_settop (self._luaobject._state, -2)
		lua._lib.lua_settop (self._luaobject._state, -2)
		return False
	def __iter__ (self):
		_dprint ('iterating over lua table', 3)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		lua._lib.lua_pushnil (self._luaobject._state)
		while lua._lib.lua_next (self._luaobject._state, -2) != 0:
			ret = self._luaobject._to_python (-2)
			lua._lib.lua_settop (self._luaobject._state, -2)
			try:
				ref = lua._lib.luaL_ref (self._luaobject._state, LUA_REGISTRYINDEX)
				lua._lib.lua_settop (self._luaobject._state, -2)
				yield ret
				lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
				lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, ref)
			finally:
				lua._lib.luaL_unref (self._luaobject._state, LUA_REGISTRYINDEX, ref)
		lua._lib.lua_settop (self._luaobject._state, -2)
	def __del__ (self):
		_dprint ('destroying lua table', 1)
		lua._lib.luaL_unref (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		#super (Table, self).__del__ (self)
	def __eq__ (self, other):
		if not isinstance (other, Table):
			return False
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, other._id)
		ret = bool (lua._lib.lua_rawequal (self._luaobject._state, -1, -2))
		lua._lib.lua_settop (self._luaobject._state, -3)
		return ret
	def __ne__ (self, other):
		return not self == other
	def dict (self):
		'Get a copy of the table as a dict'
		_dprint ('get dict from lua table', 1)
		ret = {}
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		lua._lib.lua_pushnil (self._luaobject._state)
		while lua._lib.lua_next (self._luaobject._state, -2) != 0:
			ret[self._luaobject._to_python (-2)] = self._luaobject._to_python (-1)
			lua._lib.lua_settop (self._luaobject._state, -2)
		lua._lib.lua_settop (self._luaobject._state, -2)
		return ret
	def list (self):
		'Get a copy of the table, which must be a sequence, as a list'
		_dprint ('get list from lua table', 1)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		l = lua._lib.lua_objlen (self._luaobject._state, -1)
		ret = [None] * l
		for i in range (l):
			lua._lib.lua_rawgeti (self._luaobject._state, -1, i + 1)
			ret[i] = self._luaobject._to_python (-1)
			lua._lib.lua_settop (self._luaobject._state, -2)
		lua._lib.lua_settop (self._luaobject._state, -2)
		return ret
	def pop (self, index = -1):
		'''Like list.pop'''
		_dprint ('popping item %d of lua table.' % index, 3)
		lua._lib.lua_rawgeti (self._luaobject._state, LUA_REGISTRYINDEX, self._id)
		if index < 0:
			index += len (self)
		lua._lib.lua_rawgeti (self._luaobject._state, -2, index + 1)
		ret = self._luaobject._to_python (-1)
		lua._lib.lua_settop (self._luaobject._state, -2)
		self._luaobject._push (None)
		lua._lib.lua_rawseti (self._luaobject._state, -2, index + 1)
		lua._lib.lua_settop (self._luaobject._state, -2)
		return ret
