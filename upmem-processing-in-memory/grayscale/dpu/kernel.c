/*
 * kernel.c -- RGB-to-grayscale kernel. Every tasklet of every DPU runs main().
 *
 * MRAM layout (set by the host):
 *   heap + 0            : n_pixels * 3 bytes, interleaved R G B R G B ...
 *   heap + out_offset   : n_pixels bytes, the gray result
 *
 * Work split: the image is cut into blocks of BLOCK_PIXELS pixels and the
 * blocks are dealt out in turn: tasklet t takes blocks t, t + n, t + 2n, ...
 * For each block a tasklet copies 3 * BLOCK_PIXELS bytes MRAM -> WRAM,
 * converts them, and copies BLOCK_PIXELS gray bytes WRAM -> MRAM.
 */
#include <stdint.h>
#include <defs.h>          /* me()              */
#include <mram.h>          /* mram_read / write */
#include <alloc.h>         /* mem_alloc / reset */
#include <barrier.h>
#include <perfcounter.h>

#include "../common/gray.h"

/* One tasklet needs a 1 KB stack plus 4 * BLOCK_PIXELS bytes of buffers. */
#if NR_TASKLETS * (1024 + 4 * (1 << BLOCK)) > 58000
#error "Not enough WRAM (64 KB): lower BLOCK or NR_TASKLETS"
#endif

__host gray_args_t DPU_ARGS;                /* written by the host before launch */
__host uint64_t    DPU_COUNT[NR_TASKLETS];  /* read by the host after the run    */

#ifdef GRAY_HAS_LUT
uint8_t gray_div3_lut[766];                 /* lives in WRAM, shared by all tasklets */
#endif

BARRIER_INIT(start_barrier, NR_TASKLETS);

int main(void) {
    const unsigned int id = me();

    if (id == 0) {
        mem_reset();                                  /* empty the WRAM heap     */
#ifdef PERF_CYCLES
        perfcounter_config(COUNT_CYCLES, true);       /* counter := 0            */
#else
        perfcounter_config(COUNT_INSTRUCTIONS, true);
#endif
        gray_init();                                  /* build the table, if any */
    }
    barrier_wait(&start_barrier);                     /* everybody starts together */

    const uint32_t n_pixels = DPU_ARGS.n_pixels;
    __mram_ptr uint8_t *src = DPU_MRAM_HEAP_POINTER;
    __mram_ptr uint8_t *dst = DPU_MRAM_HEAP_POINTER + DPU_ARGS.out_offset;

    /* Private WRAM buffers for this tasklet (mem_alloc returns 8-byte aligned memory). */
    uint8_t *in  = (uint8_t *) mem_alloc(3 * BLOCK_PIXELS);
    uint8_t *out = (uint8_t *) mem_alloc(BLOCK_PIXELS);

    for (uint32_t px = id * BLOCK_PIXELS; px < n_pixels; px += BLOCK_PIXELS * NR_TASKLETS) {
        /* The last block may be shorter. n_pixels is a multiple of 8, so the
         * byte counts below stay multiples of 8, as MRAM transfers require. */
        const uint32_t count = (px + BLOCK_PIXELS > n_pixels) ? (n_pixels - px) : BLOCK_PIXELS;

        mram_read(src + 3 * px, in, 3 * count);       /* MRAM -> WRAM */
        gray_convert_block(in, out, count);           /* compute      */
        mram_write(out, dst + px, count);             /* WRAM -> MRAM */
    }

    /* The counter is shared by the whole DPU: this is "everything the DPU has
     * executed since the reset", sampled when this tasklet finishes. */
    DPU_COUNT[id] = perfcounter_get();
    return 0;
}
