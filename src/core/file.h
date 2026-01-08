#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <stdlib.h>

#include "core/core.h"
#include "core/type.h"
#include "core/str.h"

/**
 * File utils.
 */


/**
 * Reads contents of the file into the buffer that is preemptivly allocated using allooator.
 * Returns pointer to the buffer and sets buffer size in bytes into the file_size.
 * @Important: Buffer should be freed manually when not used anymore.
 */
void *read_file_into_buffer(char *file_name, u64 *file_size, Allocator *allocator);

/**
 * Reads contents of the file into the String structure that is preemptivly allocated using allocator.
 * Returns String structure with pointer to dynamically allocated memory and size of it.
 * @Important: String should be freed manually when not used anymore.
 */
String read_file_into_str(char *file_name, Allocator *allocator);

/**
 * Writes contents of string into the file specified by the file_name.
 * It will overwrite file if it already exists.
 * Returns 0 on success.
 * Will return any other value and won't write to file if failed.
 */
int write_str_to_file(String str, char *file_name);

/**
 * Perfoms fwrite() of the string contents to the the specified file.
 * Returns 0 on success.
 * Will return any other value and won't write to file if failed.
 */
int fwrite_str(String str, FILE *file);


/**
 * Writes 32 bit integer to the file, enforcing little endian.
 */
static inline int fwrite_u32(u32 value, FILE *file) {
    value = to_le32(value);
    return fwrite(&value, 4, 1, file);
}

/**
 * Writes 64 bit integer to the file, enforcing little endian.
 */
static inline int fwrite_u64(u64 value, FILE *file) {
    value = to_le64(value);
    return fwrite(&value, 8, 1, file);
}

/**
 * Writes 32 bit float to the file, enforcing little endian.
 */
static inline int fwrite_float(float value, FILE *file) {
    u32 bits;
    memcpy(&bits, &value, 4);
    bits = to_le32(bits);
    return fwrite(&bits, 4, 1, file);
}

static inline u32 read_u32(u8 **ptr) {
    u32 value;
    memcpy(&value, *ptr, 4);
    *ptr += 4;
    return from_le32(value);
}

static inline u64 read_u64(u8 **ptr) {
    u64 value;
    memcpy(&value, *ptr, 8);
    *ptr += 8;
    return from_le64(value);
}

static inline float read_float(u8 **ptr) {
    u32 bits;
    memcpy(&bits, *ptr, 4);
    *ptr += 4;
    bits = from_le32(bits);
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

static inline u8 read_byte(u8 **ptr) {
    return *(*ptr)++;
}







#endif
