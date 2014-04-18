import sys
import os
import types
import ctypes
import struct
import numbers

# Some workarounds to make this file usable in Python 2 as well as 3.
if sys.version >= '3':
	long = int
	makebytes = lambda x: bytes (x, 'utf-8') if isinstance (x, str) else x
	makestr = lambda x: str (x, 'utf-8') if isinstance (x, bytes) else x
else:
	makebytes = lambda x: x
	makestr = lambda x: x

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

class lua (object):
	_instance = None
	def __new__ (cls, debug = False, loadlib = False, doloadfile = False, io = False, os = False, all = False):
		if cls._instance is None:
			cls._instance = super (lua, cls).__new__ (cls)
			self = cls._instance
			self._lib = ctypes.CDLL ("liblua5.1.so")
			self._objects = {}
			self._state = [self._lib.luaL_newstate ()]
			self._lib.luaL_openlibs (self._state[-1])
			self._lib.lua_tolstring.restype = ctypes.c_char_p
			self._lib.lua_tonumber.restype = ctypes.c_double
			self._lib.lua_touserdata.restype = ctypes.c_void_p
			self._lib.lua_topointer.restype = ctypes.c_void_p
			self._lib.lua_touserdata.restype = ctypes.c_void_p
			self._lib.lua_tothread.restype = ctypes.c_void_p
			self._lib.lua_tocfunction.restype = ctypes.c_void_p
			self._lib.lua_newuserdata.restype = ctypes.c_void_p
			self._factory = ctypes.CFUNCTYPE (ctypes.c_int, ctypes.c_void_p)
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
			self._lib.lua_createtable (self._state[-1], 0, 16)
			self._lib.lua_pushcclosure (self._state[-1], self._the_add, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__add')
			self._lib.lua_pushcclosure (self._state[-1], self._the_sub, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__sub')
			self._lib.lua_pushcclosure (self._state[-1], self._the_mul, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__mul')
			self._lib.lua_pushcclosure (self._state[-1], self._the_div, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__div')
			self._lib.lua_pushcclosure (self._state[-1], self._the_mod, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__mod')
			self._lib.lua_pushcclosure (self._state[-1], self._the_pow, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__pow')
			self._lib.lua_pushcclosure (self._state[-1], self._the_unm, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__unm')
			self._lib.lua_pushcclosure (self._state[-1], self._the_concat, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__concat')
			self._lib.lua_pushcclosure (self._state[-1], self._the_len, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__len')
			self._lib.lua_pushcclosure (self._state[-1], self._the_eq, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__eq')
			self._lib.lua_pushcclosure (self._state[-1], self._the_lt, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__lt')
			self._lib.lua_pushcclosure (self._state[-1], self._the_le, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__le')
			self._lib.lua_pushcclosure (self._state[-1], self._the_get, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__index')
			self._lib.lua_pushcclosure (self._state[-1], self._the_put, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__newindex')
			self._lib.lua_pushcclosure (self._state[-1], self._the_call, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__call')
			self._lib.lua_pushcclosure (self._state[-1], self._the_gc, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__gc')
			self._lib.lua_pushcclosure (self._state[-1], self._the_tostring, 0)
			self._lib.lua_setfield (self._state[-1], -2, b'__tostring')
			self._lib.lua_setfield (self._state[-1], LUA_REGISTRYINDEX, b'metatable')
			if not debug and not all:
				self.run ('debug = nil', name = 'disabling debug')
			if not loadlib and not all:
				self.run ('package.loadlib = nil', name = 'disabling loadlib')
			if not doloadfile and not all:
				self.run ('loadfile = nil dofile = nil', name = 'disabling loadfile and dofile')
			if not os and not all:
				self.run ('os = {clock = os.clock, date = os.date, difftime = os.difftime, setlocale = os.setlocale, time = os.time}', name = 'disabling some of os')
			if not io and not all:
				self.run ('io = nil', name = 'disabling io')
			self._table_remove = self.run ('return table.remove', state = self._state[-1])[0]
		return cls._instance
	def run_file (self, script, state = None):
		_dprint ('running lua file %s' % script, 1)
		if state is None:
			state = self._state[-1]
		pos = self._lib.lua_gettop (state)
		if self._lib.luaL_loadfile (state, script) != 0:
			raise ValueError (self._lib.lua_tolstring (state, 1, None))
		ret = self._lib.lua_pcall (state, 0, LUA_MULTRET, None)
		if ret == 0:
			size = self._lib.lua_gettop (state) - pos
			ret = [self._to_python (-size + i, state) for i in range (size)]
			self._lib.lua_settop (state, pos)
			return ret
		else:
			ret = self._lib.lua_tolstring (state, 1, None)
			self._lib.lua_settop (state, pos)
			raise ValueError (ret)
	def run (self, script = None, var = None, value = None, name = 'python string', state = None):
		_dprint ('running lua script %s with var %s' % (name, str (var)), 5)
		if state is None:
			state = self._state[-1]
		if var is not None:
			self._push (value)
			var = makebytes (var)
			self._lib.lua_setfield (state, LUA_GLOBALSINDEX, var)
		if script is not None:
			pos = self._lib.lua_gettop (state)
			script = makebytes (script)
			if self._lib.luaL_loadbuffer (state, script, len (script), name) != 0:
				raise ValueError (self._lib.lua_tolstring (state, 1, None))
			ret = self._lib.lua_pcall (state, 0, LUA_MULTRET, None)
			if ret == 0:
				size = self._lib.lua_gettop (state) - pos
				ret = [self._to_python (-size + i, state) for i in range (size)]
				self._lib.lua_settop (state, pos)
				return ret
			else:
				ret = self._lib.lua_tolstring (state, 1, None)
				self._lib.lua_settop (state, pos)
				raise ValueError (ret)
	def module (self, name, value, state = None):
		_dprint ('creating lua module %s' % name, 5)
		if state is None:
			state = self._state[-1]
		if not isinstance (value, (list, dict)):
			# module object must be a table, so convert the object to a table.
			module = {}
			for key in dir (value):
				k = makebytes (key)
				if k.startswith (b'_'):
					if k.startswith (b'_lua'):
						k = b'_' + k[4:]
					else:
						continue
				module[k] = getattr (value, key)
			value = module
		self._push_luatable (value)
		n = makebytes (name)
		self._lib.lua_setfield (state, LUA_GLOBALSINDEX, n)
		self.run (b'module ("' + n + b'")', name = 'loading module %s' % name)
	def _to_python (self, index, state = None):
		_dprint ('creating python value from lua at %d' % index, 1)
		if state is None:
			state = self._state[-1]
		type = self._lib.lua_type (state, index)
		if type == LUA_TNIL:
			return None
		elif type == LUA_TBOOLEAN:
			return bool (self._lib.lua_toboolean (state, index))
		elif type == LUA_TLIGHTUSERDATA:
			return self._lib.lua_touserdata (state, index)
		elif type == LUA_TNUMBER:
			return self._lib.lua_tonumber (state, index)
		elif type == LUA_TSTRING:
			return makestr (self._lib.lua_tolstring (state, index, None))
		elif type == LUA_TTABLE:
			_dprint ('creating table', 1)
			self._lib.lua_pushvalue (state, index)
			return Table ()
		elif type == LUA_TFUNCTION:
			self._lib.lua_pushvalue (state, index)
			return Function ()
		elif type == LUA_TUSERDATA:
			id = self._lib.lua_touserdata (state, index)
			return self._objects[id]
		elif type == LUA_TTHREAD:
			return self._lib.lua_tothread (state, index)
		elif self._lib.lua_iscfunction (state, index):
			return self._lib.lua_tocfunction (state, index)
		else:
			raise AssertionError ('unexpected lua type %d' % type)
	def _push (self, obj, state = None):
		_dprint ('pushing %s (%s) to lua stack' % (str (obj), type (obj)), 1)
		if state is None:
			state = self._state[-1]
		if obj is None:
			self._lib.lua_pushnil (state)
		elif isinstance (obj, bool):
			self._lib.lua_pushboolean (state, obj)
		elif isinstance (obj, (int, long)):
			self._lib.lua_pushinteger (state, obj)
		elif isinstance (obj, (bytes, str)):
			self._lib.lua_pushlstring (state, obj, len (obj))
		elif isinstance (obj, float):
			self._lib.lua_pushnumber (state, ctypes.c_double (obj))
		elif isinstance (obj, (Function, Table)):
			_dprint ('pushing table', 1)
			self._lib.lua_rawgeti (state, LUA_REGISTRYINDEX, obj._id)
		elif isinstance (obj, object):
			id = self._lib.lua_newuserdata (state, 1)
			self._objects[id] = obj
			self._lib.lua_getfield (state, LUA_REGISTRYINDEX, b'metatable')
			self._lib.lua_setmetatable (state, -2)
		else:
			# This shouldn't be possible: everything is an object.
			raise ValueError ('object of type %s cannot be converted to lua object' % str (type (obj)))
		_dprint ('done pushing', 1)
	def _push_luatable (self, table, state = None):
		'''Push a real lua table to the stack; not a userdata emulating a table. Table must be a list or dict'''
		_dprint ('pushing lua table', 1)
		if state is None:
			state = self._state[-1]
		if isinstance (table, dict):
			self._lib.lua_createtable (state, 0, len (table))
			for i in table:
				self._push (i)
				self._push (table[i])
				self._lib.lua_settable (state, -3)
		else:
			self._lib.lua_createtable (state, len (table), 0)
			for i in range (len (table)):
				self._push (table[i])
				self._lib.lua_rawseti (state, -2, i + 1)
		_dprint ('done pushing lua table', 1)
	def make_table (self, data = (), state = None):
		if state is None:
			state = self._state[-1]
		return Table (data)

def _object_add (state):
	_dprint ('adding lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj + float (b))
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_sub (state):
	_dprint ('subtracting lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj - b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_mul (state):
	_dprint ('multiplying lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj * b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_div (state):
	_dprint ('dividing lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj / b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_mod (state):
	_dprint ('modulo lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj % b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_pow (state):
	_dprint ('powering lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj ** b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_unm (state):
	_dprint ('unary minussing lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			self._push (-obj)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_concat (state):
	_dprint ('concatenating lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj + str (b))
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_len (state):
	_dprint ('lenning lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			self._push (len (obj))
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_eq (state):
	_dprint ('equalling lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj == b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_lt (state):
	_dprint ('lessthanning lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj < b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_le (state):
	_dprint ('lessthanorequalling lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			b = self._to_python (-1, state)
			self._push (obj <= b)
			return 1
		except:
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_get (state):
	_dprint ('getting lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		b = self._to_python (-1, state)
		_dprint ('trying to get %s[%s]' % (str (obj), str (b)), 2)
		if isinstance (b, float) and abs (b - int (b)) < 1e-10:
			b = int (b - .5)	# Use - .5 so it will be 1 lower; lua users expect 1-based arrays.
		if isinstance (b, (str, bytes)):
			b = makebytes (b)
			if b.startswith (b'_'):
				b = b'_lua' + b[1:]
		if isinstance (obj, dict):
			if b in obj:
				self._push (obj[b])
				return 1
		if hasattr (obj, b):
			self._push (getattr (obj, b))
			return 1
		sys.stderr.write ("Trying to get nonexistent %s[%s], returning nil.\n" % (str (obj), str (b)))
		self._push (None)
		return 1
	finally:
		self._state.pop ()

def _object_put (state):
	_dprint ('putting lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		try:
			key = self._to_python (-2, state)
			value = self._to_python (-1, state)
			if isinstance (key, str):
				if key.startswith ('_'):
					key = '_lua' + key[1:]
			if isinstance (obj, dict):
				obj[key] = value
			else:
				if isinstance (key, float) and abs (key - int (key)) < 1e-10:
					key = int (key - .5)	# Use - .5 so it will be 1 lower; lua users expect 1-based arrays.
				setattr (obj, key, value)
			_dprint ('set %s[%s] = %s' % (str (obj), str (key), str (value)), 3)
			return 0
		except:
			sys.stderr.write ('error trying to put %s[%s] = %s: %s\n' % (str (obj), str (key), str (value), sys.exc_info ()[1]))
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
	finally:
		self._state.pop ()

def _object_call (state):
	_dprint ('calling lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		obj = self._objects[id]
		num = self._lib.lua_gettop (state) - 1
		args = [self._to_python (i + 2, state) for i in range (num)]
		# Don't bother the user with two self arguments.
		if isinstance (obj, types.MethodType):
			assert obj.im_self is args[0]
			args = args[1:]
		try:
			ret = obj (*args)
			self._lib.lua_settop (state, 0)
			if isinstance (ret, tuple):
				for i in ret:
					self._push (i)
				return len (ret)
			else:
				self._push (ret)
				return 1
		except:
			sys.stderr.write ('Error: %s\n' % sys.exc_info ()[1])
			bt = sys.exc_info ()[2]
			while bt:
				sys.stderr.write ('\t%s:%d %s\n' % (bt.tb_frame.f_code.co_filename, bt.tb_lineno, bt.tb_frame.f_code.co_name))
				bt = bt.tb_next
			self._lib.lua_settop (state, 0)
			self._push (str (sys.exc_info ()[1]))
			self._lib.lua_error (state)
			# lua_error does not return.
	finally:
		self._state.pop ()

def _object_gc (state):
	_dprint ('cleaning lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		del self._objects[id]
		return 0
	finally:
		self._state.pop ()

def _object_tostring (state):
	_dprint ('tostringing lua stuff', 1)
	self = lua ()
	self._state.append (state)
	try:
		id = self._lib.lua_touserdata (state, 1)
		self._push (str (self._objects[id]))
		return 1
	finally:
		self._state.pop ()

class Function:
	'''Python interface to a lua function.
	The function is defined and owned by lua; this class makes it callable from Python.'''
	def __init__ (self):
		_dprint ('creating lua function', 1)
		l = lua ()
		self._id = l._lib.luaL_ref (l._state[-1], LUA_REGISTRYINDEX)
	def __call__ (self, *args):
		_dprint ('calling lua function', 3)
		l = lua ()
		pos = l._lib.lua_gettop (l._state[-1])
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		for i in args:
			l._push (i, l._state[-1])
		if l._lib.lua_pcall (l._state[-1], len (args), LUA_MULTRET, None) != 0:
			sys.stderr.write ('Error from lua: ' + l._lib.lua_tolstring (l._state[-1], 1, None) + '\n')
		size = l._lib.lua_gettop (l._state[-1]) - pos
		ret = [l._to_python (-size + i, l._state[-1]) for i in range (size)]
		l._lib.lua_settop (l._state[-1], pos)
		return ret
	def __del__ (self):
		_dprint ('destroying lua function', 1)
		l = lua ()
		l._lib.luaL_unref (l._state[-1], LUA_REGISTRYINDEX, self._id)
		#super (Function, self).__del__ (self)

class Table:
	'''Python interface to access a lua table.
	The table is owned by lua; this class only provides a view to it.'''
	def __init__ (self, table = None):
		_dprint ('creating lua table', 1)
		l = lua ()
		if table is not None:
			l._push_luatable (table, l._state[-1])
		self._id = l._lib.luaL_ref (l._state[-1], LUA_REGISTRYINDEX)
	def __len__ (self):
		_dprint ('requesting length of lua table', 3)
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		ret = l._lib.lua_objlen (l._state[-1], -1)
		l._lib.lua_settop (l._state[-1], -2)
		return ret
	def __iadd__ (self, other):
		_dprint ('adding objects to lua table', 3)
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		length = l._lib.lua_objlen (l._state[-1], -1)
		for i in other:
			l._push (i, l._state[-1])
			l._lib.lua_rawseti (l._state[-1], -2, length + 1)
			length += 1
		l._lib.lua_settop (l._state[-1], -2)
		return self
	def __getitem__ (self, key):
		_dprint ('requesting item of lua table: %s' % key, 3)
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		l._push (makebytes (key), l._state[-1])
		l._lib.lua_gettable (l._state[-1], -2)
		ret = l._to_python (-1, l._state[-1])
		l._lib.lua_settop (l._state[-1], -3)
		if ret is None:
			raise IndexError
		return ret
	def __setitem__ (self, key, value):
		_dprint ('setting item of lua table: %s: %s' % (key, value), 3)
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		l._push (makebytes (key), l._state[-1])
		l._push (value, l._state[-1])
		l._lib.lua_settable (l._state[-1], -3)
		l._lib.lua_settop (l._state[-1], -2)
	def __delitem__ (self, key):
		_dprint ('deleting item of lua table: %s' % key, 3)
		# Raise IndexError if key doesn't exist: use getitem.
		self[key]
		self[key] = None
	def __contains__ (self, key):
		_dprint ('checking if %s is in lua table' % key, 3)
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		l._lib.lua_pushnil (l._state[-1])
		while l._lib.lua_next (l._state[-1], -2) != 0:
			if makebytes (l._to_python (-2, l._state[-1])) == makebytes (key):
				l._lib.lua_settop (l._state[-1], -4)
				return True
			l._lib.lua_settop (l._state[-1], -2)
		l._lib.lua_settop (l._state[-1], -2)
		return False
	def __iter__ (self):
		_dprint ('iterating over lua table', 3)
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		l._lib.lua_pushnil (l._state[-1])
		while l._lib.lua_next (l._state[-1], -2) != 0:
			ret = l._to_python (-2, l._state[-1])
			l._lib.lua_settop (l._state[-1], -2)
			try:
				ref = l._lib.luaL_ref (l._state[-1], LUA_REGISTRYINDEX)
				l._lib.lua_settop (l._state[-1], -2)
				yield ret
				l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
				l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, ref)
			finally:
				l._lib.luaL_unref (l._state[-1], LUA_REGISTRYINDEX, ref)
		l._lib.lua_settop (l._state[-1], -2)
	def __del__ (self):
		_dprint ('destroying lua table', 1)
		l = lua ()
		l._lib.luaL_unref (l._state[-1], LUA_REGISTRYINDEX, self._id)
		#super (Table, self).__del__ (self)
	def __eq__ (self, other):
		if not isinstance (other, Table):
			return False
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, other._id)
		ret = bool (l._lib.lua_rawequal (l._state[-1], -1, -2))
		l._lib.lua_settop (l._state[-1], -3)
		return ret
	def __ne__ (self, other):
		return not self == other
	def dict (self):
		'Get a copy of the table as a dict'
		_dprint ('get dict from lua table', 1)
		l = lua ()
		ret = {}
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		l._lib.lua_pushnil (l._state[-1])
		while l._lib.lua_next (l._state[-1], -2) != 0:
			ret[l._to_python (-2, l._state[-1])] = l._to_python (-1, l._state[-1])
			l._lib.lua_settop (l._state[-1], -2)
		l._lib.lua_settop (l._state[-1], -2)
		return ret
	def list (self):
		'Get a copy of the table, which must be a sequence, as a list'
		_dprint ('get list from lua table', 1)
		l = lua ()
		l._lib.lua_rawgeti (l._state[-1], LUA_REGISTRYINDEX, self._id)
		length = l._lib.lua_objlen (l._state[-1], -1)
		ret = [None] * length
		for i in range (length):
			l._lib.lua_rawgeti (l._state[-1], -1, i + 1)
			ret[i] = l._to_python (-1, l._state[-1])
			l._lib.lua_settop (l._state[-1], -2)
		l._lib.lua_settop (l._state[-1], -2)
		return ret
	def pop (self, index = -1):
		'''Like list.pop'''
		_dprint ('popping item %d of lua table.' % index, 3)
		l = lua ()
		if index < 0:
			index += len (self)
		return l._table_remove (self, index + 1, l._state[-1])
