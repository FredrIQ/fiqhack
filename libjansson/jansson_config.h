/* Last modified by Alex Smith, 2013-09-21 */
/*
 * Copyright (c) 2010-2011 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See copyright for details.
 *
 * This file specifies a part of the site-specific configuration for
 * Jansson, namely those things that affect the public API in
 * jansson.h.
 */

#ifndef JANSSON_CONFIG_H
#define JANSSON_CONFIG_H

/* If your compiler supports the inline keyword in C, JSON_INLINE is
   defined to `inline', otherwise empty. In C++, the inline is always
   supported. */
#ifdef __cplusplus
#define JSON_INLINE inline
#else
#define JSON_INLINE inline
#endif

/* If your compiler supports the `long long` type,
   JSON_INTEGER_IS_LONG_LONG is defined to 1, otherwise to 0. */
#define JSON_INTEGER_IS_LONG_LONG 1

/* If locale.h and localeconv() are available, define to 1,
   otherwise to 0. */
#define JSON_HAVE_LOCALECONV 1

#endif
