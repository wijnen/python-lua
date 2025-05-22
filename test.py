#!/usr/bin/python3
import lua

# Setup.
import lua
code = lua.Lua()
code.run('python = require("python")')  # If you want to use it.

# Define a Python function.
def pyfun(arg):
        print("Python function called with arg '%s'." % arg)

# Define a Lua function.
code.run('''function luafun(arg) print("Lua function called with arg '" .. arg .. "'.") end''')

# Make the Lua function accessible to Python (this could have been done in one step).
luafun = code.run('return luafun')

# Make the Python function accessible to Lua.
code.set('pyfun', pyfun)

# Run the Lua function from Python.
luafun('from Python')

# Run the Python function from Lua.
code.run('pyfun("from Lua")')

# Create Python compound objects from Lua.
print(type(code.run('return {1, 2, 3}')))       # Lua table (not a Python object)
print(type(code.run('return python.list{1,2,3}')))      # Python list
print(type(code.run('return python.dict{foo = "bar"}')))        # Python dict

# Lua strings that are passed to Python must be UTF-8 encoded and are treated as str.
print(repr(code.run('return "Unicode"')))

# A bytes object can be created with python.bytes.
print(repr(code.run('return python.bytes("Binary")')))

t = code.run('return {"grape", 24, x = 12, ["-"] = "+"}')
print(t.concat('/'))
t.insert("new 1")
t.insert(2, "new 2")
print(t.unpack())
print(t.unpack(3))
print(t.unpack(2, 3))
print(t.move(2, 3, 4, code.table()).dict())
comp = lambda i, j: str(i) < str(j)
c = code.run('return function (i, j) return foo(i, j) end', var = 'foo', value = comp)
t.sort(c)
print(t.dict())
t = code.table(12, 7, 333, -18)
t.sort()
print(t.dict())
t = code.table('12', '7', '333', '-18', foo = 27)
t.sort()
print(t.dict())
for k, v in t.pairs():
	print('pair item', k, '=', v)
for k, v in t.ipairs():
	print('ipair item', k, '=', v)
