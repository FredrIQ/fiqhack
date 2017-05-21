#ifndef DUMBMAKE
#error !AIMAKE_FAIL_SILENTLY!
#endif

#include <string.h>

const char gamesdatadir[] = GAMESDATADIR;
const char gamesstatedir[] = GAMESSTATEDIR;

const char *aimake_get_option(const char *name) {
  if (!strcmp(name, "gamesdatadir"))
    return gamesdatadir;
  if (!strcmp(name, "gamesstatedir"))
    return gamesstatedir;
  return NULL;
}
