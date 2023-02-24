#!/usr/bin/python3

from setuptools import setup, Extension

module = Extension('lua',
	sources = [
		'module.cc',
	],
	depends = [
		'setup.py',
	],
	language = 'c++',
	extra_compile_args = ['-std=c++20', '-I/usr/include/lua5.4', '-llua5.4'],
)

setup(name = 'lua',
	version = '0.6',
	description = 'Allow Lua and Python scripts to work together',
	ext_modules = [module],
)
