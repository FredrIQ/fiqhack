#ifdef AIMAKE_BUILDOS_MSWin32
# include <shlobj.h>
#endif
#include <iso646.h>
#include <limits.h>
#include <sys/types.h>
#include <setjmp.h>
#include <string.h>
#include <zlib.h>
static struct o {
  const char *name;
  const char *value;
  int csidl;
  #ifdef AIMAKE_BUILDOS_MSWin32
  char working[MAX_PATH];
  #endif
} options[] = {
  {"gamesdatadir", "data", -1},
  {"gamesstatedir", "save", -1},
  {0, 0, -1}
};
const char *
aimake_get_option(const char *name)
{
  struct o *x = options;
  while(x->name) {
    if (!strcmp(x->name, name)) {
      #ifdef AIMAKE_BUILDOS_MSWin32
      if (x->csidl != -1) {
        SHGetFolderPathA(0, x->csidl, 0, 0, x->working);
        strncat(x->working, "\\", MAX_PATH - 1);
        strncat(x->working, x->value, MAX_PATH - 1);
        return x->working;
      }
      #endif
      return x->value;
    }
    x++;
  }
  return 0;
}
