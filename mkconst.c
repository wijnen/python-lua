#include <lua.h>
#include <stdio.h>

int main(int argc, char **argv) {
	(void)&argc;
	(void)&argv;
	printf("LUA_MULTRET = %d\n", LUA_MULTRET);
	printf("LUA_REGISTRYINDEX = %d\n", LUA_REGISTRYINDEX);
	printf("LUA_RIDX_GLOBALS = %d\n", LUA_RIDX_GLOBALS);
	printf("LUA_RIDX_MAINTHREAD = %d\n", LUA_RIDX_MAINTHREAD);
	printf("LUA_OK = %d\n", LUA_OK);
	printf("LUA_YIELD = %d\n", LUA_YIELD);
	printf("LUA_ERRRUN = %d\n", LUA_ERRRUN);
	printf("LUA_ERRSYNTAX = %d\n", LUA_ERRSYNTAX);
	printf("LUA_ERRMEM = %d\n", LUA_ERRMEM);
	printf("LUA_ERRERR = %d\n", LUA_ERRERR);
	printf("LUA_TNONE = %d\n", LUA_TNONE);
	printf("LUA_TNIL = %d\n", LUA_TNIL);
	printf("LUA_TBOOLEAN = %d\n", LUA_TBOOLEAN);
	printf("LUA_TLIGHTUSERDATA = %d\n", LUA_TLIGHTUSERDATA);
	printf("LUA_TNUMBER = %d\n", LUA_TNUMBER);
	printf("LUA_TSTRING = %d\n", LUA_TSTRING);
	printf("LUA_TTABLE = %d\n", LUA_TTABLE);
	printf("LUA_TFUNCTION = %d\n", LUA_TFUNCTION);
	printf("LUA_TUSERDATA = %d\n", LUA_TUSERDATA);
	printf("LUA_TTHREAD = %d\n", LUA_TTHREAD);
	printf("LUA_MINSTACK = %d\n", LUA_MINSTACK);
	printf("LUA_GCSTOP = %d\n", LUA_GCSTOP);
	printf("LUA_GCRESTART = %d\n", LUA_GCRESTART);
	printf("LUA_GCCOLLECT = %d\n", LUA_GCCOLLECT);
	printf("LUA_GCCOUNT = %d\n", LUA_GCCOUNT);
	printf("LUA_GCCOUNTB = %d\n", LUA_GCCOUNTB);
	printf("LUA_GCSTEP = %d\n", LUA_GCSTEP);
	printf("LUA_GCSETPAUSE = %d\n", LUA_GCSETPAUSE);
	printf("LUA_GCSETSTEPMUL = %d\n", LUA_GCSETSTEPMUL);
	printf("LUA_HOOKCALL = %d\n", LUA_HOOKCALL);
	printf("LUA_HOOKRET = %d\n", LUA_HOOKRET);
	printf("LUA_HOOKLINE = %d\n", LUA_HOOKLINE);
	printf("LUA_HOOKCOUNT = %d\n", LUA_HOOKCOUNT);
	printf("LUA_HOOKTAILCALL = %d\n", LUA_HOOKTAILCALL);
	printf("LUA_MASKCALL = %d\n", LUA_MASKCALL);
	printf("LUA_MASKRET = %d\n", LUA_MASKRET);
	printf("LUA_MASKLINE = %d\n", LUA_MASKLINE);
	printf("LUA_MASKCOUNT = %d\n", LUA_MASKCOUNT);
	return 0;
}
