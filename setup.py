#!/usr/bin/env python

import distutils.core
distutils.core.setup (
		name = 'lua',
		py_modules = ['lua'],
		version = '0.2',
		description = 'Lua scripting interface',
		author = 'Bas Wijnen',
		author_email = 'wijnen@debian.org',
		)
