all: src/lua/luaconst.py

src/lua/luaconst.py: mkconst Makefile
	./$< > $@

mkconst: mkconst.c Makefile
	gcc -Wall -Werror -Wextra `pkg-config --cflags lua5.4` $< -o $@

clean:
	rm -f src/lua/luaconst.py mkconst
