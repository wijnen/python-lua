# Makefile for building constants file needed by module.
# Copyright 2012-2023 Bas Wijnen <wijnen@debian.org> {{{
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

all: src/lua/luaconst.py

src/lua/luaconst.py: mkconst Makefile
	./$< > $@

mkconst: mkconst.c Makefile
	gcc -Wall -Werror -Wextra `pkg-config --cflags lua5.4` $< -o $@

clean:
	rm -f src/lua/luaconst.py mkconst

# vim: set foldmethod=marker :
