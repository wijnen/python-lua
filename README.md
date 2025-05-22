This module allows Lua code to be used in Python programs.

The interface allows passing Python objects into the Lua environment and using
Lua objects in the Python program.

# Simple Example

```Python
# Setup.
import lua
code = lua.Lua()
code.run('python = require("python")')	# If you want to use it.

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
print(type(code.run('return {1, 2, 3}')))	# Lua table (not a Python object)
print(type(code.run('return python.list{1,2,3}')))	# Python list
print(type(code.run('return python.dict{foo = "bar"}')))	# Python dict

# Lua strings that are passed to Python must be UTF-8 encoded and are treated as str.
print(repr(code.run('return "Unicode"')))

# A bytes object can be created with python.bytes.
print(repr(code.run('return python.bytes("Binary")')))
```

This generates the following output:
```
Lua function called with arg 'from Python'.
Python function called with arg 'from Lua'.
<class 'lua.Table'>
<class 'list'>
<class 'dict'>
'Unicode'
b'Binary'
```

# API description
The module API is described below. Note that due to limited time available,
this may not be kept entirely up to date. If things don't seem to work as
described here, please check the source code.

## Module Loading
```Python
# Import Lua module.
import lua
```

## Creating a Lua instance
To use the module, an instance of the Lua class must be created. Creating
multiple instances of this class allows you to have completely separate
environments, which do not share any state with each other. Note that combining
Lua variables from different instances results in undefined behavior. For
exampe, this should not be done:
```Python
# INCORRECT CODE!
import lua
A = lua.Lua()
B = lua.Lua()

table = A.run('return {}')
B.set('table_from_A', table)
# INCORRECT CODE!
```

When creating an instance, the default is to disable all Lua features that are
a risk for security or user privacy. Each of these features can be enabled by
setting the corresponding constructor parameter to True. The features are:

- debug: enable the debugging library. Not actually unsafe (probably?), but should only be enabled explicitly.
- loadlib: enable the loadlib function. Allows running code on host system and thus cause damage and violate privacy.
- searchers: enable the default contents of package.searchers. This allows loading lua modules using require. When disabled, only standard modules and modules that have been explicitly provided by the host can be accessed.
- doloadfile: enable the dofile and loadfile functions. Allows running lua code from external files. Should not pose risks to the system, but does contain a privacy risk, because lua can find out what other lua files are available on the system. Users should also normally expect that an embedded language cannot access external files, so disabling it follows the Principle of Least Astonishment.
- io: enable the io library. This library allows arbitrary file access on the host system and should only be enabled for trusted Lua code.
- os: enable all of the os module. Even when not enabled, `clock`, `date`, `difftime`, `setlocale` and `time` are still available. The rest of os allows running executables on the host system. This should only be enabled for trusted Lua code.

Additionally, the 'python' module, which contains list and dict constructors,
can be disabled by setting `python_module` to False. It is enabled by default.
Note that this setting does not create the `python` variable; it only allows
lua code to `require` it.

An example instance that allows access of the host filesystem through `io` is:

```Python
code = lua.Lua(io = True)
```

## Setting up the Lua environment
Lua code will usually require access to some variables or functions from
Python. There are two methods for granting this access: setting variables, and
providing modules.

### Setting variables
To set a Lua variable to a Python value, the `set` method is used. It takes two
arguments: a string which is the name of the (possibly new) Lua variable, and
its value. For convenience, if `run` is passed keyword arguments `var` and `value`, it calls `set` with those parameters before running the Lua code.

If the variable is mutable (for example, it is a `list`), then changing the
contents of the value will result in a changed value in Python. In other words,
mutable variables are passed by reference and remain owned by Python.

```Python
my_list = [1, 2, 3]
code.run('foo[2] = "bar"', var = 'foo', value = my_list)
# my_list is now [1, 'bar', 3]. Note that Lua-indexing is 1-based.
```

### Providing modules
The more common way to provide access to host functionality is through a module
that can be loaded using the `require` function in Lua. This is done by calling
the `module` function. The argument is a dict, or a Python module. The contents
of the argument are provided to Lua. Note that the object itself is not, so if
new items are added to the dict, these changes will not show up in Lua. Changes
to the _values_ of the items will show up, however.

```Python
import my_custom_module
code.module('my_module', my_custom_module)
code.run('mod = require "my_module"; mod.custom_function()')
```

### The builtin `python` module
Unless `python_module` is set to `False` in the Lua constructor, the `python` module is accessible to Lua code. It contains three functions:
1. `list(lua_table)` converts a lua table to a Python list, as when calling `lua_table.list()` from Python.
1. `dict(lua_table)` converts a lua table to a Python dict, as when calling `lua_table.dict()` from Python.
1. `bytes(obj)` converts an object to a Python bytes object.

#### The `python.bytes` function
This function converts its argument to a Python `bytes` object. There are several cases:
- If the argument is a Lua Table, it converts it to a Python list using its `list` method. This is then passed to Python's `bytes` constructor. It should therefore contain only integers in the range from 0 to 255.
- If the argument is a Lua string, it encodes it as UTF-8.
- Otherwise, the object is passed to Python's `bytes` constructor.

## Running Lua code
There are two ways to run Lua code. Using the `run()` function demonstrated in
the previous section, and using the `run_file()` function.

The `run_file()` function works very similar to `run()`. There are two
differences:

1. The parameter of `run_file` is a file name, for `run()` it is Lua code.
1. `run()` allows setting a variable to a Python value. `run_file()` does not.

The intent of both functions is slightly different: `run()` is meant to be used
mostly for situations where `eval` might have been used if it was Python code;
`run_file()` is meant to be used when `exec` is more appropriate. In other
words, `run()` would be used for a short computation, while `run_file()` would
be more likely to be used for running a potentially large chunk of code.
Because of this, it is convenient for `run()` to pass a Python value, or
perhaps a list or dict, to be used directly, while `run_file()` will normally
need much more, and the `module()` function will be used to provide it in a
more elegant way.

However, while this is the consideration behind the functions, there is nothing
to enforce it. If you want to run large amounts of code with `run()`, or a
single line computation with `run_file()`, it will work without any problems.

The `run` method accept a `description` argument, which must be a string. It is
used in error messages. If not provided, the lua code is used as a description.

### Return values
Both `run()` and `run_file()` can return a value. This is the primary method
for accessing Lua values from Python. (The other option is to provide a
container from Python and injecting a value from Lua.) Because Lua functions
can return multiple values, this leads to a usability issue: the return value
could always be a sequence, but that is inconvenient for the usual case when
zero or one values are returned.

This is solved by converting the result in those cases:

- When 0 values are returned, the functions return None instead.
- When 1 value is returned, the functions return that value.
- When 2 or more values are returned, the functions return a list of return values.

This means that Python cannot see the difference between returning 0 values,
and returning 1 value which is `nil`, or between multiple values and a single
list. Normally this will not be a problem, but if this is important to the
Python program, it can pass the parameter `keep_single = True`. In that case
the returned value is always a list of values, even if it has 0 or 1 elements.

## Operators
Most Python operators have obvious behavior when applied to Lua objects and
vice versa. For example, when using `*` the objects will be multiplied.
However, there are some exceptions.

1. In Lua there is a difference between adding numbers (which is done with `+`) and strings (which is done with `..`). Unfortunately there is no appropriate operator available in Python, so `..` has been mapped to `@`, the matrix multiplication operator. This means that running `lua_table @ python_object` from Python will call the `__concat` method in `lua_table`'s metatable, and that running `python_object .. lua_object` from Lua will call the `__matmul__` method on `python_object`. Note that this does not apply to simple objects like numbers and strings, because these are converted into native objects on both sides (so a Lua string that is passed to Python becomes a Python `str` object, and using the @ operator on that does not involve Lua).
1. Lua uses the `__close` metatable method for to-be-closed variables. Python does not have this concept. The module will call the `__close__` method (which despite its name, does not have a special meaning in Python) on the Python object when the underlying Lua object is closed by Lua.

## Using Lua tables from Python code
In addition to all the obvious operators (including `@` as concat) working
normally on Lua tables, there are a few extra members defined on them. Note
that attributes cannot be used to access table contents, so `luatable.foo` does
not work, but `luatable['foo']` does.

- `luatable.dict()`: Converts the table to a Python dict, containing all items.
- `luatable.list()`: Converts the table to a Python list. Only the values with integer keys are inserted, and the list stops at the first missing element. In other words, this is only useful for lists that don't contain nil "values". Also note that the indexing changes: luatable[1] is the first element, and it is the same as luatable.list()[0].
- `luatable.pop(index = -1)`: Removes the last index (or the given index) from the table and shifts the contents of the table to fill its place using `table.remove`. The index must be an integer. Negative indices count from the back of the list, with -1 being the last element.

## First elements
Because there is a difference between the index of the first element of a list
in Python (0) and that of a table in Lua (1), indexing such structures from the
other language can be confusing. The module does not account for this. What
this means is that you must be aware of which language owns the object. When
indexing a Lua table from Python code, the first index is 1, just like in Lua.
When indexing a Python list from Lua, the first index is 0, just like in
Python.

Here's an example to show this:
```Python
python_list = ['zero', 'one', 'two']
lua_table = code.run('return {"one", "two", "three"}')

code.run('''print('element 2 in the python list is "two": ' .. list[2])''', var = 'list', value = python_list)
print('element 2 in the lua table is "two":', lua_table[2])
```
