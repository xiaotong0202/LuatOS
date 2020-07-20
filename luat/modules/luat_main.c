

#include "luat_base.h"
#include "luat_malloc.h"
#include "luat_fs.h"
#include "stdio.h"
#include "luat_msgbus.h"
#include "luat_timer.h"

#define LUAT_LOG_TAG "luat.main"
#include "luat_log.h"

static int report (lua_State *L, int status);

lua_State *L;

static uint8_t boot_mode = 1;

void stopboot(void) {
  boot_mode = 0;
}

lua_State * luat_get_state() {
  return L;
}

static int pmain(lua_State *L) {
    int re = 0;

    #ifdef LUA_32BITS
    LLOGD("Lua complied with LUA_32BITS");
    #endif

    // 加载内置库
    luat_openlibs(L);

    size_t total; size_t used; size_t max_used;
    luat_meminfo_luavm(&total, &used, &max_used);
    LLOGD("luavm %ld %ld %ld", total, used, max_used);
    luat_meminfo_sys(&total, &used, &max_used);
    LLOGD("sys   %ld %ld %ld", total, used, max_used);

    lua_gc(L, LUA_GCSETPAUSE, 90); // 设置`垃圾收集器间歇率`要低于100%
    
    // 加载main.lua
    //re = luaL_dofile(L, "/main.lua");
    re = luaL_dostring(L, "require(\"main\")");

    report(L, re);
    lua_pushboolean(L, 1);  /* signal no errors */
    return 1;
}

/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) lua_writestringerror("%s: ", pname);
  lua_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report (lua_State *L, int status) {
  if (status != LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    l_message("LUAT", msg);
    lua_pop(L, 1);  /* remove message */
  }
  return status;
}

static int panic (lua_State *L) {
  lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
                        lua_tostring(L, -1));
  return 0;  /* return to Lua to abort */
}

int luat_main (int argc, char **argv, int _) {
  if (boot_mode == 0) {
    return 0; // just nop
  }
  //luat_print("\nI/main: Luat " LUAT_VERSION " build " __DATE__ " " __TIME__ "\n");
  LLOGI("Luat " LUAT_VERSION ", build at: " __DATE__ " " __TIME__);
  //print_list_mem("entry luat_main");
  // 1. init filesystem
  luat_fs_init();
  //print_list_mem("after fs init");

  // 2. init Lua State
  int status, result;
  L = lua_newstate(luat_heap_alloc, NULL);
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory\n");
    return 1;
  }
  if (L) lua_atpanic(L, &panic);
  //print_list_mem("after lua_newstate");
  lua_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
  lua_pushinteger(L, argc);  /* 1st argument */
  lua_pushlightuserdata(L, argv); /* 2nd argument */
  status = lua_pcall(L, 2, 1, 0);  /* do the call */
  result = lua_toboolean(L, -1);  /* get result */
  report(L, status);
  //lua_close(L);
  LLOGE("Lua VM exit!! reboot in 30s");
  luat_timer_mdelay(30*1000);
  luat_os_reboot(result);
  return (result && status == LUA_OK) ? 0 : 2;
}

#include "vsprintf.h"
__attribute__((weak)) int l_sprintf(char *buf, size_t size, const char *fmt, ...) {
    int32_t n;
    va_list args;

    va_start(args, fmt);
    n = custom_vsprintf(buf, /*size,*/ fmt, args);
    va_end(args);

    return n;
}
