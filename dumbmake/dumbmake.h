#ifndef DUMBMAKE
#error !AIMAKE_FAIL_SILENTLY!
#endif

#define AIMAKE_ABI_VERSION(x)
#define AIMAKE_IMPORT(x) x
#define AIMAKE_EXPORT(x) x
extern const char *aimake_get_option(const char *);
