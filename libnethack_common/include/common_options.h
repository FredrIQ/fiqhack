/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Alex Smith, 2013-09-21 */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef COMMON_OPTIONS_H
# define COMMON_OPTIONS_H

# ifdef IN_LIBNETHACK_COMMON
#  define EXPORT(x) AIMAKE_EXPORT(x)
# else
#  define EXPORT(x) AIMAKE_IMPORT(x)
# endif

# include "nethack_types.h"

struct nhlib_boolopt_map {
    const char *optname;
    nh_bool *addr;
};

extern struct nh_option_desc *EXPORT(nhlib_find_option)(
    struct nh_option_desc *optlist, const char *name);

extern nh_bool *EXPORT(nhlib_find_boolopt)(
    const struct nhlib_boolopt_map *map, const char *name);

extern union nh_optvalue EXPORT(nhlib_string_to_optvalue)(
    const struct nh_option_desc *option, char *str);

/* Parse new autopickup rules, allocated on the heap */
extern struct nh_autopickup_rules *EXPORT(nhlib_parse_autopickup_rules)(
    const char *str);

extern nh_bool EXPORT(nhlib_option_value_ok)(
    const struct nh_option_desc *option, union nh_optvalue value);

/* Create a new copy of some autopickup rules, allocated on the heap */
extern struct nh_autopickup_rules *EXPORT(nhlib_copy_autopickup_rules)(
    const struct nh_autopickup_rules *in);

/* Copy the option value into the existing option. Pointed-to objects are
 * reallocated if necessary and the old ones freed. */
extern nh_bool EXPORT(nhlib_copy_option_value)(
    struct nh_option_desc *option, union nh_optvalue value);

#endif
