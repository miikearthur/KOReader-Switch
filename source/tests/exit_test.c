/*
    Host check of the `os.exit` handling in `switch/main.c`: longjmp out of LuaJIT, back to the
    function running the state on its own thread (same code, minus the libnx specific restart bits).
*/

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <pthread.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static jmp_buf g_exit_jmp;
static int g_exit_status;
static bool g_closed;
static int g_finalized;
static int failures;

static int ko_os_exit(lua_State *L)
{
    int status;
    if (lua_isboolean(L, 1))
        status = lua_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE;
    else
        status = (int)luaL_optinteger(L, 1, EXIT_SUCCESS);
    bool close = lua_toboolean(L, 2);
    g_exit_status = status;
    g_closed = close;
    if (close)
        lua_close(L);
    fflush(NULL);
    longjmp(g_exit_jmp, 1);
}

static int finalizer(lua_State *L)
{
    (void)L;
    g_finalized++;
    return 0;
}

static void *run(void *arg)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_getglobal(L, "os");
    lua_pushcfunction(L, ko_os_exit);
    lua_setfield(L, -2, "exit");
    lua_pop(L, 1);
    lua_pushcfunction(L, finalizer);
    lua_setglobal(L, "finalizer");
    if (setjmp(g_exit_jmp) == 0) {
        if (luaL_dostring(L, (const char *)arg)) {
            fprintf(stderr, "lua error: %s\n", lua_tostring(L, -1));
            g_exit_status = 99;
        }
        lua_close(L);
    }
    return NULL;
}

static void check(const char *script, int status, bool closed, int finalized)
{
    g_exit_status = -1;
    g_closed = false;
    g_finalized = 0;
    pthread_attr_t attr;
    pthread_t thread;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 * 1024 * 1024);
    if (pthread_create(&thread, &attr, run, (void *)script) != 0 || pthread_join(thread, NULL) != 0) {
        fprintf(stderr, "FAIL: thread\n");
        failures++;
        return;
    }
    pthread_attr_destroy(&attr);
    if (g_exit_status != status || g_closed != closed || g_finalized != finalized) {
        fprintf(stderr, "FAIL: %s\n  status %d (expected %d), closed %d (%d), finalized %d (%d)\n",
                script, g_exit_status, status, g_closed, closed, g_finalized, finalized);
        failures++;
    }
}

int main(void)
{
    const char *with_finalizer = "local p = newproxy(true); getmetatable(p).__gc = finalizer; ";
    char script[512];

    /* Like `reader.lua`: top level, closing the state (which runs the finalizers). */
    snprintf(script, sizeof(script), "%s os.exit(85, true)", with_finalizer);
    check(script, 85, true, 1);

    /* From deep within coroutines and protected calls (like UIManager's xpcall handler). */
    snprintf(script, sizeof(script),
             "%s coroutine.wrap(function() pcall(function() xpcall(function() os.exit(1, true) end, debug.traceback) end) end)() "
             "error('not reached')", with_finalizer);
    check(script, 1, true, 1);

    /* Without closing the state, and with a boolean status. */
    check("os.exit(false)", EXIT_FAILURE, false, 0);
    check("os.exit(true)", EXIT_SUCCESS, false, 0);

    /* No `os.exit`: the script just returns. */
    check("local x = 1", -1, false, 0);

    /* Repeatedly (the jump buffer is reused). */
    for (int i = 0; i < 100; i++)
        check("os.exit(3, true)", 3, true, 0);

    printf(failures ? "exit_test: %d failure(s)\n" : "exit_test: all checks passed\n", failures);
    return failures != 0;
}
