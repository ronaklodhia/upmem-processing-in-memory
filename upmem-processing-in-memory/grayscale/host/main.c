/*
 * main.c -- host program for the RGB-to-grayscale kernel.
 *
 * Steps: load (or generate) an RGB image -> split it across the DPUs ->
 * copy each part into its DPU's MRAM -> launch the kernel -> copy the gray
 * bytes back -> check them twice -> report the instruction count.
 *
 * Usage: ./bin/gray_host [-i image.ppm | -n pixels] [-d dpus] [-o out.pgm]
 *                        [-w warmups] [-e repetitions]
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <dpu.h>

#include "../common/gray.h"

#ifndef DPU_BINARY
#define DPU_BINARY "./bin/gray_dpu"
#endif
#ifndef NR_TASKLETS
#error "NR_TASKLETS must be defined (the Makefile does this)"
#endif

#define MRAM_BYTES (64u * 1024u * 1024u)        /* one DPU owns 64 MB */

#ifdef GRAY_HAS_LUT
uint8_t gray_div3_lut[766];
#endif

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static uint32_t round_up8(uint32_t x) { return (x + 7u) & ~7u; }

/* ------------------------------------------------------------------ */
/* Image files: binary PPM (P6) in, binary PGM (P5) out. Both are a    */
/* short text header followed by raw bytes, so no library is needed.   */
/* ------------------------------------------------------------------ */
static bool read_header_int(FILE *f, uint32_t *value) {
    int c = fgetc(f);
    for (;;) {                                   /* skip spaces and # comments */
        while (c == ' ' || c == '\t' || c == '\n' || c == '\r') c = fgetc(f);
        if (c != '#') break;
        while (c != '\n' && c != EOF) c = fgetc(f);
    }
    if (c < '0' || c > '9') return false;
    uint32_t v = 0;
    while (c >= '0' && c <= '9') { v = v * 10 + (uint32_t)(c - '0'); c = fgetc(f); }
    *value = v;                                  /* the one whitespace after the number is consumed */
    return true;
}

static uint8_t *load_ppm(const char *path, uint32_t *width, uint32_t *height) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    uint32_t maxval = 0;
    if (fgetc(f) != 'P' || fgetc(f) != '6' ||
        !read_header_int(f, width) || !read_header_int(f, height) || !read_header_int(f, &maxval) ||
        maxval != 255 || *width == 0 || *height == 0) {
        fprintf(stderr, "%s: not a binary PPM (P6) with 8-bit channels\n", path);
        exit(1);
    }
    size_t bytes = (size_t)*width * *height * 3;
    uint8_t *rgb = malloc(bytes);
    if (!rgb || fread(rgb, 1, bytes, f) != bytes) {
        fprintf(stderr, "%s: file is shorter than its header says\n", path);
        exit(1);
    }
    fclose(f);
    return rgb;
}

static void save_pgm(const char *path, const uint8_t *gray, uint32_t width, uint32_t height) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P5\n%u %u\n255\n", width, height);
    fwrite(gray, 1, (size_t)width * height, f);
    fclose(f);
}

/* Random test image (xorshift generator, fixed seed -> same image every run). */
static uint8_t *random_image(uint32_t n_pixels) {
    uint8_t *rgb = malloc((size_t)n_pixels * 3);
    uint32_t s = 2463534242u;
    for (size_t i = 0; i < (size_t)n_pixels * 3; i++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        rgb[i] = (uint8_t)(s >> 11);
    }
    return rgb;
}

/* ------------------------------------------------------------------ */
/* Independent reference: the lab formula in exact whole-number        */
/* arithmetic, written differently from gray.h on purpose.             */
/* ------------------------------------------------------------------ */
static uint8_t exact_gray(unsigned r, unsigned g, unsigned b) {
#if GRAY_METHOD == GRAY_LIGHTNESS
    unsigned mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    unsigned mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return (uint8_t)((mx + mn) / 2);
#elif GRAY_METHOD == GRAY_AVERAGE
    return (uint8_t)((r + g + b) / 3);
#else
    return (uint8_t)((30 * r + 59 * g + 11 * b) / 100);   /* 0.30, 0.59, 0.11 without rounding error */
#endif
}

int main(int argc, char **argv) {
    const char *in_path = NULL, *out_path = NULL;
    uint32_t n_pixels = 262144, width = 0, height = 0, nr_dpus = 1;
    int warmups = 0, reps = 1, opt;

    while ((opt = getopt(argc, argv, "i:n:d:o:w:e:h")) != -1) {
        switch (opt) {
        case 'i': in_path  = optarg; break;
        case 'n': n_pixels = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'd': nr_dpus  = (uint32_t)strtoul(optarg, NULL, 0); break;
        case 'o': out_path = optarg; break;
        case 'w': warmups  = atoi(optarg); break;
        case 'e': reps     = atoi(optarg); break;
        default:
            fprintf(stderr,
                "Usage: %s [-i image.ppm | -n pixels] [-d dpus] [-o out.pgm] [-w warmups] [-e repetitions]\n"
                "  -i  input image, binary PPM (make one with tools/image_tool.py)\n"
                "  -n  no file: use a random image with this many pixels (default 262144)\n"
                "  -d  number of DPUs (default 1)\n"
                "  -o  write the gray image as binary PGM\n"
                "  -w  untimed warm-up runs (default 0)     -e  timed runs (default 1)\n", argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }
    if (reps < 1 || nr_dpus < 1) { fprintf(stderr, "need -e >= 1 and -d >= 1\n"); return 1; }

    /* ---- 1. the image ---- */
    uint8_t *rgb;
    if (in_path) { rgb = load_ppm(in_path, &width, &height); n_pixels = width * height; }
    else         { rgb = random_image(n_pixels); width = n_pixels; height = 1; }
    if (n_pixels == 0) { fprintf(stderr, "empty image\n"); return 1; }

    /* ---- 2. DPUs ---- */
    struct dpu_set_t set, dpu;
    uint32_t i;
    DPU_ASSERT(dpu_alloc(nr_dpus, NULL, &set));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));
    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));

    /* ---- 3. split: every DPU gets the same number of pixels, a multiple of 8.
     * Pixels are 3 bytes, so 8 pixels = 24 bytes: whole pixels AND 8-byte aligned. ---- */
    const uint32_t px_per_dpu = round_up8((n_pixels + nr_dpus - 1) / nr_dpus);
    const uint32_t in_bytes   = 3 * px_per_dpu;          /* per DPU, multiple of 24 */
    const uint32_t out_bytes  = px_per_dpu;              /* per DPU, multiple of 8  */
    if ((uint64_t)in_bytes + out_bytes > MRAM_BYTES) {
        fprintf(stderr, "image too large: %u pixels per DPU, the limit is %u. Use more DPUs (-d).\n",
                px_per_dpu, MRAM_BYTES / 4);
        return 1;
    }

    /* Padded copies so that every DPU transfer has the same size. */
    uint8_t *in_pad  = calloc((size_t)nr_dpus, in_bytes);
    uint8_t *out_pad = calloc((size_t)nr_dpus, out_bytes);
    gray_args_t *args = calloc(nr_dpus, sizeof(gray_args_t));
    uint64_t (*counts)[NR_TASKLETS] = calloc(nr_dpus, sizeof(*counts));
    if (!in_pad || !out_pad || !args || !counts) { fprintf(stderr, "out of memory\n"); return 1; }
    memcpy(in_pad, rgb, (size_t)n_pixels * 3);

    for (i = 0; i < nr_dpus; i++) {
        uint64_t first = (uint64_t)i * px_per_dpu;
        uint32_t mine  = first >= n_pixels ? 0 :
                         (n_pixels - first < px_per_dpu ? (uint32_t)(n_pixels - first) : px_per_dpu);
        args[i].n_pixels   = round_up8(mine);            /* padding pixels are black and ignored later */
        args[i].out_offset = in_bytes;                   /* gray bytes sit right after the RGB bytes   */
    }

    /* ---- 4. run ---- */
    double t_in = 0, t_kernel = 0, t_out = 0;
    for (int rep = 0; rep < warmups + reps; rep++) {
        const bool timed = rep >= warmups;
        memset(out_pad, 0xAA, (size_t)nr_dpus * out_bytes);   /* so stale data can never pass the check */

        double t0 = now_ms();
        DPU_FOREACH(set, dpu, i) { DPU_ASSERT(dpu_prepare_xfer(dpu, &args[i])); }
        DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "DPU_ARGS", 0, sizeof(gray_args_t), DPU_XFER_DEFAULT));
        DPU_FOREACH(set, dpu, i) { DPU_ASSERT(dpu_prepare_xfer(dpu, in_pad + (size_t)i * in_bytes)); }
        DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, in_bytes, DPU_XFER_DEFAULT));
        double t1 = now_ms();

        DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
        double t2 = now_ms();

        DPU_FOREACH(set, dpu, i) { DPU_ASSERT(dpu_prepare_xfer(dpu, out_pad + (size_t)i * out_bytes)); }
        DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, in_bytes, out_bytes, DPU_XFER_DEFAULT));
        double t3 = now_ms();

        DPU_FOREACH(set, dpu, i) { DPU_ASSERT(dpu_prepare_xfer(dpu, counts[i])); }
        DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_FROM_DPU, "DPU_COUNT", 0, sizeof(counts[0]), DPU_XFER_DEFAULT));

        if (timed) { t_in += t1 - t0; t_kernel += t2 - t1; t_out += t3 - t2; }
    }

    /* ---- 5. instruction count: per DPU the largest tasklet sample, then add the DPUs ---- */
    uint64_t total = 0, busiest = 0;
    for (i = 0; i < nr_dpus; i++) {
        uint64_t c = 0;
        for (int t = 0; t < NR_TASKLETS; t++) if (counts[i][t] > c) c = counts[i][t];
        total += c;
        if (c > busiest) busiest = c;
    }

    /* ---- 6. two checks ---- */
    gray_init();
    uint64_t mismatches = 0, differing = 0, err_sum = 0;
    unsigned max_err = 0;
    for (uint32_t p = 0; p < n_pixels; p++) {
        unsigned r = rgb[3 * p], g = rgb[3 * p + 1], b = rgb[3 * p + 2];
        unsigned got = out_pad[p];
        if (got != gray_from_rgb(r, g, b)) {             /* check 1: same formula on the CPU */
            if (mismatches < 5)
                fprintf(stderr, "pixel %u (%u,%u,%u): DPU %u, CPU %u\n", p, r, g, b, got, gray_from_rgb(r, g, b));
            mismatches++;
        }
        unsigned ex  = exact_gray(r, g, b);              /* check 2: distance from the exact formula */
        unsigned err = got > ex ? got - ex : ex - got;
        if (err) { differing++; err_sum += err; if (err > max_err) max_err = err; }
    }
    const bool ok = (mismatches == 0);

    if (out_path) save_pgm(out_path, out_pad, width, height);

    /* ---- 7. report ---- */
    printf("version %s | unroll %d | %u DPU(s) x %d tasklets | block %u pixels | %u pixels\n",
           GRAY_VERSION, UNROLL, nr_dpus, NR_TASKLETS, BLOCK_PIXELS, n_pixels);
#ifdef PERF_CYCLES
    printf("DPU cycles      : total %llu | per pixel %.2f | busiest DPU %llu\n",
#else
    printf("DPU instructions: total %llu | per pixel %.2f | busiest DPU %llu\n",
#endif
           (unsigned long long)total, (double)total / n_pixels, (unsigned long long)busiest);
    printf("host time (ms)  : CPU->DPU %.2f | kernel %.2f | DPU->CPU %.2f   (simulator time, not hardware time)\n",
           t_in / reps, t_kernel / reps, t_out / reps);
    printf("exact formula   : max error %u gray level(s) | mean error %.4f | pixels that differ %.2f%%\n",
           max_err, (double)err_sum / n_pixels, 100.0 * differing / n_pixels);
    printf("%s DPU output %s the CPU result (%llu mismatches)\n",
           ok ? "[OK]" : "[ERROR]", ok ? "equals" : "differs from", (unsigned long long)mismatches);
    /* one machine-readable line for the sweep scripts */
    printf("RESULT,%s,%d,%u,%d,%u,%u,%llu,%.3f,%llu,%u,%.4f,%s\n",
           GRAY_VERSION, UNROLL, nr_dpus, NR_TASKLETS, BLOCK_PIXELS, n_pixels,
           (unsigned long long)total, (double)total / n_pixels, (unsigned long long)busiest,
           max_err, (double)err_sum / n_pixels, ok ? "yes" : "NO");

    free(rgb); free(in_pad); free(out_pad); free(args); free(counts);
    DPU_ASSERT(dpu_free(set));
    return ok ? 0 : 2;
}
