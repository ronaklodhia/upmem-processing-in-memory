# RGB-to-grayscale on UPMEM PIM (Processing-in-Memory).
#
#   make                                      default build
#   make VERSION=AVG_LUT NR_TASKLETS=11       pick a formula and a tasklet count
#   make clean
#
# Parameters (all fixed at compile time):
NR_TASKLETS ?= 16
#   pixels per MRAM<->WRAM block = 2^BLOCK  (8 -> 256 pixels = 768 bytes in, 256 bytes out)
BLOCK       ?= 8
#   LIGHT_BASE LIGHT_SORT LIGHT_TREE | AVG_BASE AVG_RECIP AVG_LUT | LUM_DOUBLE LUM_FLOAT LUM_FIXED LUM_MUL8
VERSION     ?= LUM_FIXED
#   pixels converted per loop iteration: 1 2 4 8
UNROLL      ?= 1
#   what the DPU performance counter counts: INSTRUCTIONS or CYCLES
PERF        ?= INSTRUCTIONS

BIN      := bin
HOST     := $(BIN)/gray_host
DPU      := $(BIN)/gray_dpu
DEFINES  := -DNR_TASKLETS=$(NR_TASKLETS) -DBLOCK=$(BLOCK) -D$(VERSION) -DUNROLL=$(UNROLL) -DPERF_$(PERF)
WARN     := -Wall -Wextra

# Remember the parameters of the last build. If they change, bin/.config is
# rewritten and both programs are rebuilt, so a stale binary can never run.
CONFIG := $(NR_TASKLETS) $(BLOCK) $(VERSION) $(UNROLL) $(PERF)
$(shell mkdir -p $(BIN); echo "$(CONFIG)" | cmp -s - $(BIN)/.config || echo "$(CONFIG)" > $(BIN)/.config)

.PHONY: all clean
all: $(HOST) $(DPU)

# Host program: normal C compiler, linked against the UPMEM host library.
# -ffp-contract=off keeps float maths identical to the DPU's software floating point.
$(HOST): host/main.c common/gray.h $(BIN)/.config
	$(CC) -std=c11 -O2 -ffp-contract=off $(WARN) $(DEFINES) -o $@ host/main.c `dpu-pkg-config --cflags --libs dpu`

# DPU kernel: UPMEM's clang for the DPU instruction set.
$(DPU): dpu/kernel.c common/gray.h $(BIN)/.config
	dpu-upmem-dpurte-clang -O2 $(WARN) $(DEFINES) -DGRAY_ON_DPU -o $@ dpu/kernel.c

clean:
	rm -rf $(BIN)
