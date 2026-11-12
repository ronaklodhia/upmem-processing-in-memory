/*
 * gray.h -- shared by the host program and the DPU kernel.
 *
 * Holds (1) the build-time parameters, (2) the argument block the host sends
 * to every DPU, and (3) the pixel formula itself. Because both sides compile
 * the SAME gray_from_rgb(), the host can check the DPU result byte for byte.
 *
 * VERSION (chosen in the Makefile) selects one formula:
 *
 *   LIGHT_BASE   (min + max) / 2, min and max found with 4 comparisons
 *   LIGHT_SORT   same result: order r and g first, then place b (2 or 3 comparisons)
 *   LIGHT_TREE   same result: a decision tree, each of the 6 orderings adds its own pair
 *   AVG_BASE     (r + g + b) / 3 with a real division
 *   AVG_RECIP    ((r + g + b) * 683) >> 11   -- exact for every sum 0..765
 *   AVG_LUT      table[r + g + b]            -- 766-byte lookup table in WRAM
 *   LUM_DOUBLE   0.3*r + 0.59*g + 0.11*b in double  (what a beginner writes)
 *   LUM_FLOAT    the same in single-precision float (the lab formula as given)
 *   LUM_FIXED    (77*r + 151*g + 28*b) >> 8  -- 8-bit fixed point, weights sum to 256
 *   LUM_MUL8     the same numbers, but each product is ONE instruction on the
 *                DPU's 8 x 8-bit hardware multiplier
 */
#ifndef GRAY_H
#define GRAY_H

#include <stdint.h>

/* ---------------- build-time parameters ---------------- */
#ifndef BLOCK
#define BLOCK 8                       /* log2(pixels per block) */
#endif
#if BLOCK < 3 || BLOCK > 9
#error "BLOCK must be 3..9: one block is 3 * 2^BLOCK bytes and an MRAM transfer is at most 2048 bytes"
#endif
#define BLOCK_PIXELS (1u << BLOCK)    /* pixels moved MRAM <-> WRAM at a time */

#ifndef UNROLL
#define UNROLL 1                      /* pixels converted per loop iteration: 1, 2, 4 or 8 */
#endif

/* ---------------- host -> DPU arguments (8 bytes) ---------------- */
typedef struct {
    uint32_t n_pixels;    /* pixels this DPU converts; always a multiple of 8 */
    uint32_t out_offset;  /* where the gray bytes go, in bytes from the MRAM heap start */
} gray_args_t;

/* ---------------- the pixel formula ---------------- */
#define GRAY_LIGHTNESS  0
#define GRAY_AVERAGE    1
#define GRAY_LUMINOSITY 2

#if defined(LIGHT_BASE)
#define GRAY_VERSION "LIGHT_BASE"
#define GRAY_METHOD  GRAY_LIGHTNESS
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    uint32_t mn = r, mx = r;
    if (g < mn) mn = g;
    if (b < mn) mn = b;
    if (g > mx) mx = g;
    if (b > mx) mx = b;
    return (uint8_t)((mn + mx) / 2);
}

#elif defined(LIGHT_SORT)
#define GRAY_VERSION "LIGHT_SORT"
#define GRAY_METHOD  GRAY_LIGHTNESS
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    uint32_t lo, hi;
    if (r < g) { lo = r; hi = g; } else { lo = g; hi = r; }  /* order r and g  */
    if (b > hi) hi = b;                                      /* b is the max?  */
    else if (b < lo) lo = b;                                 /* ...or the min? */
    return (uint8_t)((lo + hi) >> 1);
}

#elif defined(LIGHT_TREE)
#define GRAY_VERSION "LIGHT_TREE"
#define GRAY_METHOD  GRAY_LIGHTNESS
/* Two or three comparisons find which of the six orderings we are in; each
 * ending already knows its min and max, so nothing has to be copied around. */
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    if (r < g) {
        if (b > g) return (uint8_t)((r + b) >> 1);   /* r <  g <  b */
        if (b < r) return (uint8_t)((b + g) >> 1);   /* b <  r <  g */
        return (uint8_t)((r + g) >> 1);              /* r <= b <= g */
    } else {
        if (b > r) return (uint8_t)((g + b) >> 1);   /* g <= r <  b */
        if (b < g) return (uint8_t)((b + r) >> 1);   /* b <  g <= r */
        return (uint8_t)((g + r) >> 1);              /* g <= b <= r */
    }
}

#elif defined(AVG_BASE)
#define GRAY_VERSION "AVG_BASE"
#define GRAY_METHOD  GRAY_AVERAGE
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (uint8_t)((r + g + b) / 3);
}

#elif defined(AVG_RECIP)
#define GRAY_VERSION "AVG_RECIP"
#define GRAY_METHOD  GRAY_AVERAGE
/* 683 / 2048 = 0.33349..., just above 1/3. For sums up to 765 the extra
 * 0.00016 * sum never reaches the next whole number, so the result is exact. */
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (uint8_t)(((r + g + b) * 683u) >> 11);
}

#elif defined(AVG_LUT)
#define GRAY_VERSION "AVG_LUT"
#define GRAY_METHOD  GRAY_AVERAGE
#define GRAY_HAS_LUT 1
extern uint8_t gray_div3_lut[766];    /* defined once in each program */
/* Fill the table with 0,0,0,1,1,1,2,2,2,... using only additions. */
static inline void gray_init(void) {
    uint32_t q = 0, m = 0;
    for (uint32_t s = 0; s < 766; s++) {
        gray_div3_lut[s] = (uint8_t)q;
        if (++m == 3) { m = 0; q++; }
    }
}
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    return gray_div3_lut[r + g + b];
}

#elif defined(LUM_DOUBLE)
#define GRAY_VERSION "LUM_DOUBLE"
#define GRAY_METHOD  GRAY_LUMINOSITY
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (uint8_t)(0.3 * r + 0.59 * g + 0.11 * b);
}

#elif defined(LUM_FLOAT)
#define GRAY_VERSION "LUM_FLOAT"
#define GRAY_METHOD  GRAY_LUMINOSITY
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (uint8_t)(0.3f * (float)r + 0.59f * (float)g + 0.11f * (float)b);
}

#elif defined(LUM_FIXED)
#define GRAY_VERSION "LUM_FIXED"
#define GRAY_METHOD  GRAY_LUMINOSITY
/* 0.30 * 256 = 76.8 -> 77,  0.59 * 256 = 151.04 -> 151,  0.11 * 256 = 28.16 -> 28.
 * 77 + 151 + 28 = 256, so white (255,255,255) still gives exactly 255. */
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (uint8_t)((77u * r + 151u * g + 28u * b) >> 8);
}

#elif defined(LUM_MUL8)
#define GRAY_VERSION "LUM_MUL8"
#define GRAY_METHOD  GRAY_LUMINOSITY
/* A pixel value and a weight both fit in 8 bits, and the DPU can multiply two
 * 8-bit numbers in a single instruction (mul_ul_ul). The compiler does not use
 * it for constants (it builds shift-and-add chains instead), so we ask for it
 * by name. On the host the ordinary multiplication gives the same numbers. */
#ifdef GRAY_ON_DPU
#include <built_ins.h>
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    uint32_t wr = 77, wg = 151, wb = 28, pr, pg, pb;
    __builtin_mul_ul_ul_rrr(pr, r, wr);
    __builtin_mul_ul_ul_rrr(pg, g, wg);
    __builtin_mul_ul_ul_rrr(pb, b, wb);
    return (uint8_t)((pr + pg + pb) >> 8);
}
#else
static inline uint8_t gray_from_rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (uint8_t)((77u * r + 151u * g + 28u * b) >> 8);
}
#endif

#else
#error "Choose a VERSION: LIGHT_BASE LIGHT_SORT LIGHT_TREE AVG_BASE AVG_RECIP AVG_LUT LUM_DOUBLE LUM_FLOAT LUM_FIXED LUM_MUL8"
#endif

#ifndef GRAY_HAS_LUT
static inline void gray_init(void) {}   /* nothing to prepare for the other versions */
#endif

/* ---------------- convert one block of interleaved RGB ---------------- */
/* in  : n_pixels * 3 bytes  R G B R G B ...
 * out : n_pixels bytes
 * n_pixels is always a multiple of 8, so the unrolled loops need no clean-up code. */
#define GRAY_PX(k) out[(k)] = gray_from_rgb(in[3 * (k)], in[3 * (k) + 1], in[3 * (k) + 2])

static inline void gray_convert_block(const uint8_t *restrict in, uint8_t *restrict out, uint32_t n_pixels) {
#if UNROLL == 8
    for (uint32_t i = 0; i < n_pixels; i += 8, in += 24, out += 8) {
        GRAY_PX(0); GRAY_PX(1); GRAY_PX(2); GRAY_PX(3);
        GRAY_PX(4); GRAY_PX(5); GRAY_PX(6); GRAY_PX(7);
    }
#elif UNROLL == 4
    for (uint32_t i = 0; i < n_pixels; i += 4, in += 12, out += 4) {
        GRAY_PX(0); GRAY_PX(1); GRAY_PX(2); GRAY_PX(3);
    }
#elif UNROLL == 2
    for (uint32_t i = 0; i < n_pixels; i += 2, in += 6, out += 2) {
        GRAY_PX(0); GRAY_PX(1);
    }
#elif UNROLL == 1
    for (uint32_t i = 0; i < n_pixels; i++, in += 3, out += 1) {
        GRAY_PX(0);
    }
#else
#error "UNROLL must be 1, 2, 4 or 8"
#endif
}

#endif /* GRAY_H */
