#ifndef TYPE_H
#define TYPE_H

/**
 * Integer typedefs.
 */

#include <stdint.h>

typedef int8_t      s8;
typedef uint8_t	    u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;

static inline u32 to_le32(u32 v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return v;
#else
    return __builtin_bswap32(v); 
#endif
}

static inline u32 from_le32(u32 v) {
    return to_le32(v);
}

static inline u64 to_le64(u64 v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return v;
#else
    return __builtin_bswap64(v); 
#endif
}

static inline u64 from_le64(u64 v) {
    return to_le64(v);
}

/**
 * This function properly copies ints between formats, without causing overflows.
 * It does by truncating or expanding source bytes of the integer while copying to dentanation.
 * This function operates differently based on the endiannese of the integers in memory.
 * It uses C23 standard macros to do so.
 *
 * "filler" is a value that will be filled for bytes in the dest.
 * so for example if copying (s8) 0xff to (s32) 0xff 0x?? 0x?? 0x??
 * you can specify 0x?? to be any filler byte. Sometimes it is important if you want
 * to copy -1 correctly from (s8) to (s32) otherwise you can leave this parameter at 0.
 */
void mem_copy_int(void *dest, void *src, u8 dest_size, u8 src_size, u8 filler);


#endif

