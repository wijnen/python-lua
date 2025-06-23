#!/usr/bin/python3

from setuptools import setup, Extension

module = Extension('lua',
	sources = [
		'lua.c',
		'function.c',
		'table.c',
	],
	depends = [
		'module.h',
		'setup.py',
	],
	language = 'c',
	libraries = ['lua5.4'],
	extra_compile_args = ['-I/usr/include/lua5.4',
		       '-Wstack-protector', '-fstack-protector',
		       '-D_FORTIFY_SOURCE=2', '-O1',
		       '-Wformat', '-Wformat-security',
		       '-Wl,-z,relro', '-Wl,-z,now'],
)

setup(name = 'lua-wrapper',
	version = '1.0',
	description = 'Allow Lua and Python scripts to work together',
	ext_modules = [module],
)
