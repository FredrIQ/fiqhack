/* Last modified by Alex Smith, 2013-09-21 */
/*
 * Copyright (c) 2009-2011 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See copyright for details.
 */

#ifndef JANSSON_H
#define JANSSON_H

#include <stdio.h>
#include <stdlib.h>  /* for size_t */
#include <stdarg.h>

#include <jansson_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/* version */

#define JANSSON_MAJOR_VERSION  2
#define JANSSON_MINOR_VERSION  2
#define JANSSON_MICRO_VERSION  1

/* Micro version is omitted if it's 0 */
#define JANSSON_VERSION  "2.2.1"

/* Version as a 3-byte hex number, e.g. 0x010201 == 1.2.1. Use this
   for numeric comparisons, e.g. #if JANSSON_VERSION_HEX >= ... */
#define JANSSON_VERSION_HEX  ((JANSSON_MAJOR_VERSION << 16) |   \
                              (JANSSON_MINOR_VERSION << 8)  |   \
                              (JANSSON_MICRO_VERSION << 0))


/* types */

typedef enum {
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_INTEGER,
    JSON_REAL,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL
} json_type;

typedef struct {
    json_type type;
    size_t refcount;
} json_t;

#if JSON_INTEGER_IS_LONG_LONG
#define JSON_INTEGER_FORMAT "lld"
typedef long long json_int_t;
#else
#define JSON_INTEGER_FORMAT "ld"
typedef long json_int_t;
#endif /* JSON_INTEGER_IS_LONG_LONG */

#define json_typeof(json)      ((json)->type)
#define json_is_object(json)   (json && json_typeof(json) == JSON_OBJECT)
#define json_is_array(json)    (json && json_typeof(json) == JSON_ARRAY)
#define json_is_string(json)   (json && json_typeof(json) == JSON_STRING)
#define json_is_integer(json)  (json && json_typeof(json) == JSON_INTEGER)
#define json_is_real(json)     (json && json_typeof(json) == JSON_REAL)
#define json_is_number(json)   (json_is_integer(json) || json_is_real(json))
#define json_is_true(json)     (json && json_typeof(json) == JSON_TRUE)
#define json_is_false(json)    (json && json_typeof(json) == JSON_FALSE)
#define json_is_boolean(json)  (json_is_true(json) || json_is_false(json))
#define json_is_null(json)     (json && json_typeof(json) == JSON_NULL)

#ifdef JANSSON_IN_LIBJANSSON
#define EXPORT(x) AIMAKE_EXPORT(x)
  AIMAKE_ABI_VERSION(4.2.1)
#else
#define EXPORT(x) AIMAKE_IMPORT(x)
#endif
typedef json_t *jansson_json_t_p;
typedef void *jansson_void_p;
typedef const char *jansson_cchar_p;
typedef char *jansson_char_p;

/* construction, destruction, reference counting */
jansson_json_t_p EXPORT(json_object) (void);
jansson_json_t_p EXPORT(json_array) (void);
jansson_json_t_p EXPORT(json_string) (const char *value);
jansson_json_t_p EXPORT(json_string_nocheck) (const char *value);
jansson_json_t_p EXPORT(json_integer) (json_int_t value);
jansson_json_t_p EXPORT(json_real) (double value);
jansson_json_t_p EXPORT(json_true) (void);
jansson_json_t_p EXPORT(json_false) (void);
jansson_json_t_p EXPORT(json_null) (void);

static JSON_INLINE
json_t *json_incref(json_t *json)
{
    if(json && json->refcount != (size_t)-1)
        ++json->refcount;
    return json;
}

/* do not call json_delete directly */
void EXPORT(json_delete) (json_t *json);

static JSON_INLINE
void json_decref (json_t *json)
{
    if(json && json->refcount != (size_t)-1 && --json->refcount == 0)
        json_delete(json);
}


/* error reporting */

#define JSON_ERROR_TEXT_LENGTH    160
#define JSON_ERROR_SOURCE_LENGTH   80

typedef struct {
    int line;
    int column;
    int position;
    char source[JSON_ERROR_SOURCE_LENGTH];
    char text[JSON_ERROR_TEXT_LENGTH];
} json_error_t;


/* getters, setters, manipulation */

size_t EXPORT(json_object_size) (const json_t *object);
jansson_json_t_p EXPORT(json_object_get) (const json_t *object, const char *key);
int EXPORT(json_object_set_new) (json_t *object, const char *key, json_t *value);
int EXPORT(json_object_set_new_nocheck) (json_t *object, const char *key, json_t *value);
int EXPORT(json_object_del) (json_t *object, const char *key);
int EXPORT(json_object_clear) (json_t *object);
int EXPORT(json_object_update) (json_t *object, json_t *other);
jansson_void_p EXPORT(json_object_iter) (json_t *object);
jansson_void_p EXPORT(json_object_iter_at) (json_t *object, const char *key);
jansson_void_p EXPORT(json_object_iter_next) (json_t *object, void *iter);
jansson_cchar_p EXPORT(json_object_iter_key) (void *iter);
jansson_json_t_p EXPORT(json_object_iter_value) (void *iter);
int EXPORT(json_object_iter_set_new) (json_t *object, void *iter, json_t *value);

static JSON_INLINE
int json_object_set(json_t *object, const char *key, json_t *value)
{
    return json_object_set_new(object, key, json_incref(value));
}

static JSON_INLINE
int json_object_set_nocheck(json_t *object, const char *key, json_t *value)
{
    return json_object_set_new_nocheck(object, key, json_incref(value));
}

static JSON_INLINE
int json_object_iter_set(json_t *object, void *iter, json_t *value)
{
    return json_object_iter_set_new(object, iter, json_incref(value));
}

size_t EXPORT(json_array_size) (const json_t *array);
jansson_json_t_p EXPORT(json_array_get) (const json_t *array, size_t index);
int EXPORT(json_array_set_new) (json_t *array, size_t index, json_t *value);
int EXPORT(json_array_append_new) (json_t *array, json_t *value);
int EXPORT(json_array_insert_new) (json_t *array, size_t index, json_t *value);
int EXPORT(json_array_remove) (json_t *array, size_t index);
int EXPORT(json_array_clear) (json_t *array);
int EXPORT(json_array_extend) (json_t *array, json_t *other);

static JSON_INLINE
int json_array_set(json_t *array, size_t index, json_t *value)
{
    return json_array_set_new(array, index, json_incref(value));
}

static JSON_INLINE
int json_array_append(json_t *array, json_t *value)
{
    return json_array_append_new(array, json_incref(value));
}

static JSON_INLINE
int json_array_insert(json_t *array, size_t index, json_t *value)
{
    return json_array_insert_new(array, index, json_incref(value));
}

jansson_cchar_p EXPORT(json_string_value) (const json_t *string);
json_int_t EXPORT(json_integer_value) (const json_t *integer);
double EXPORT(json_real_value) (const json_t *real);
double EXPORT(json_number_value) (const json_t *json);

int EXPORT(json_string_set) (json_t *string, const char *value);
int EXPORT(json_string_set_nocheck) (json_t *string, const char *value);
int EXPORT(json_integer_set) (json_t *integer, json_int_t value);
int EXPORT(json_real_set) (json_t *real, double value);


/* pack, unpack */

jansson_json_t_p EXPORT(json_pack) (const char *fmt, ...);
jansson_json_t_p EXPORT(json_pack_ex) (json_error_t *error, size_t flags, const char *fmt, ...);
jansson_json_t_p EXPORT(json_vpack_ex) (json_error_t *error, size_t flags, const char *fmt, va_list ap);

#define JSON_VALIDATE_ONLY  0x1
#define JSON_STRICT         0x2

int EXPORT(json_unpack) (json_t *root, const char *fmt, ...);
int EXPORT(json_unpack_ex) (json_t *root, json_error_t *error, size_t flags, const char *fmt, ...);
int EXPORT(json_vunpack_ex) (json_t *root, json_error_t *error, size_t flags, const char *fmt, va_list ap);


/* equality */

int EXPORT(json_equal) (json_t *value1, json_t *value2);


/* copying */

jansson_json_t_p EXPORT(json_copy) (json_t *value);
jansson_json_t_p EXPORT(json_deep_copy) (json_t *value);


/* decoding */

#define JSON_REJECT_DUPLICATES 0x1
#define JSON_DISABLE_EOF_CHECK 0x2

jansson_json_t_p EXPORT(json_loads) (const char *input, size_t flags, json_error_t *error);
jansson_json_t_p EXPORT(json_loadb) (const char *buffer, size_t buflen, size_t flags, json_error_t *error);
jansson_json_t_p EXPORT(json_loadf) (FILE *input, size_t flags, json_error_t *error);
jansson_json_t_p EXPORT(json_load_file) (const char *path, size_t flags, json_error_t *error);


/* encoding */

#define JSON_INDENT(n)      (n & 0x1F)
#define JSON_COMPACT        0x20
#define JSON_ENSURE_ASCII   0x40
#define JSON_SORT_KEYS      0x80
#define JSON_PRESERVE_ORDER 0x100
#define JSON_ENCODE_ANY     0x200

typedef int (*json_dump_callback_t)(const char *buffer, size_t size, void *data);

jansson_char_p EXPORT(json_dumps) (const json_t *json, size_t flags);
int EXPORT(json_dumpf) (const json_t *json, FILE *output, size_t flags);
int EXPORT(json_dump_file) (const json_t *json, const char *path, size_t flags);
int EXPORT(json_dump_callback) (const json_t *json, json_dump_callback_t callback, void *data, size_t flags);

/* custom memory allocation */

typedef void *(*json_malloc_t)(size_t);
typedef void (*json_free_t)(void *);

void EXPORT(json_set_alloc_funcs) (json_malloc_t malloc_fn, json_free_t free_fn);

#ifdef __cplusplus
}
#endif

#endif
