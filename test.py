#!/usr/bin/python3

import faulthandler
faulthandler.enable()
import lua

# Setup.
import lua
code = lua.Lua()

# Set up a complex type which is wrapped and supports many operators.
code.set('p', complex(1,2))

# Set up a Python list.
code.set('a', ['a', 'b', 'c', 'd'])

# Set up a Python function.
def m(a, b):
	print('function called')
	return 'A: %s, B: %s' % (a, b)
code.set('m', m)
code.run('m(1,2)')

# Set up a custom Python class for operators that cannot be tested otherwise.
class foo:
	def __mod__(self, other):
		return 'moddy %s' % other
	def __floordiv__(self, other):
		return '\\div %s' % other
	def __and__(self, other):
		return 'and %s' % other
	def __or__(self, other):
		return 'or %s' % other
	def __xor__(self, other):
		return 'xor %s' % other
	def __lshift__(self, other):
		return '<< %s' % other
	def __rshift__(self, other):
		return '>> %s' % other
	def __le__(self, other):
		return '<= %s' % other
	def __eq__(self, other):
		return '== %s' % other
	def __lt__(self, other):
		return '< %s' % other
	def __invert__(self):
		return '!!'
	def __len__(self):
		return 18
code.set('foo', foo())

# Test running Python operators from Lua.
print('require python')
code.run('python = require("python")')
# Set up a Lua table.
print('create table')
code.run('nn = {4, "x", y = 5}')
print('running operators')
code.run('print("1+2j + 1 = " .. (p + 1))')
code.run('print("1+2j - 1 = " .. (p - 1))')
code.run('print("1+2j * 2 = " .. (p * 2))')
code.run('print("1+2j / 2 = " .. (p / 2))')
code.run('print("foo % 2 = " .. (foo % 2))')
code.run('print("1+2j ^ 2 = " .. (p ^ 2))')
code.run('print("foo // 2 = " .. (foo // 2))')
code.run('print("foo & 2 = " .. (foo & 2))')
code.run('print("foo | 2 = " .. (foo | 2))')
code.run('print("foo << 2 = " .. (foo << 2))')
code.run('print("foo >> 2 = " .. (foo >> 2))')
code.run('print("1+2j .. 2 = " .. (p .. 2))')
code.run('print("foo ~ 2 = " .. (foo ~ 2))')
code.run('print("1+2j == 2 = " .. tostring(p == 2))')
code.run('print("foo < 2 = " .. tostring(foo < 2))')
code.run('print("foo <= 2 = " .. tostring(foo <= 2))')
code.run('print("-(1+2j) = " .. -p)')
code.run('print("~foo = " .. ~foo)')
code.run('print("#foo = " .. #foo)')
code.run('print("a[2]=x")')
code.run('a[2] = "x"')
code.run('print(a)')
code.run('print("a[2] = " .. a[2])')
code.run('print("m(1+2j, nn) = ")')
code.run('print(m(p, nn))')
code.run('print("tostring(1+2j) = " .. tostring(p))')

# Now test calls from python to lua.
code.run('''
	meta = {
		__add = function(self, other) return "add" .. other end,
		__sub = function(self, other) return "sub" .. other end,
		__mul = function(self, other) return "mul" .. other end,
		__div = function(self, other) return "div" .. other end,
		__mod = function(self, other) return "mod" .. other end,
		__pow = function(self, other) return "pow" .. other end,
		__idiv = function(self, other) return "idiv" .. other end,
		__band = function(self, other) return "band" .. other end,
		__bor = function(self, other) return "bor" .. other end,
		__bxor = function(self, other) return "bxor" .. other end,
		__shl = function(self, other) return "shl" .. other end,
		__shr = function(self, other) return "shr" .. other end,
		__concat = function(self, other) return "concat" .. other end,
		__eq = function(self, other) return true end,
		__lt = function(self, other) return false end,
		__le = function(self, other) return false end,
		__unm = function(self) return "unm" end,
		__bnot = function(self) return "bnot" end,
		__len = function(self) return rawlen(self) + 2 end,
		__index = function(self, key) return key * 2 end,
		__newindex = function(self, key, value)
			rawset(self, key, value * 3)
		end,
	}
''')
print('defining function')
code.run('function f(a, b, c) return a .. b .. c end')
print('setting meta table')
code.run('setmetatable(nn, meta)')
print('get function and array from Lua')
meta, f = code.run('return nn, f')
print('testing bytes')
code.run(b'print("running bytes object with non-utf-8: \x88")')

print('add', meta + 3)
print('sub', meta - 3)
print('mul', meta * 3)
print('div', meta / 3)
print('mod', meta % 3)
print('pow', meta ** 3)
print('idiv', meta // 3)
print('and', meta & 3)
print('or', meta | 3)
print('xor', meta ^ 3)
print('shl', meta << 3)
print('shr', meta >> 3)
print('concat', meta @ 3)
print('eq', meta == 3)
print('lt', meta < 3)
print('le', meta <= 3)
print('unm', -meta)
print('bnot', ~meta)
print('len', len(meta))
print('index', meta[3])
meta[4] = 5
print('str after newindex', str(meta.list()))
print('call', f(19, 'x', 'kk'))

# Test getattr from Lua __index.
code.run('a.append(43) print(a)')

class C:
	def __init__(self):
		print('init!')
	def __del__(self):
		print('del!')

def x():
	c = C()
	code.set('c', c)
	code.run('c = nil collectgarbage()')

x()

a = code.run('a = {3, 5} return a')
print('in', 5 in a, 4 in a)
print('not in', 4 not in a, 5 not in a)
