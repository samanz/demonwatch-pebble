# Makefile for pDOOM (Pebble Time 2 / Emery)
# Target CPU: SiFli Cortex-M33 / Cortex-M4 Thumb-2

CC = arm-none-eabi-gcc
SIZE = arm-none-eabi-size

DOOM_SRCS = $(wildcard src/doom/*.c)
PEBBLE_SRCS = $(wildcard src/pebble/*.c)
SRCS = $(DOOM_SRCS) $(PEBBLE_SRCS)

OBJS = $(SRCS:.c=.o)

CFLAGS = -std=gnu11 -mthumb -mcpu=cortex-m33 -mfloat-abi=soft -Os \
  -ffunction-sections -fdata-sections -fno-common \
  -DFLAT_SPAN -DFLAT_NUKAGE1_COLOR=118 \
  -DVIEWWINDOWWIDTH=38 -DVIEWWINDOWHEIGHT=28 -DMAPWIDTH=38 \
  -DLOW_MEMORY -DPEBBLE_EMERY -DC_ONLY=1 \
  -Isrc/doom -Isrc/pebble

LDFLAGS = -specs=nano.specs -specs=nosys.specs -Wl,--gc-sections -Wl,-Map=pdoom.map

PBW = pdoom.pbw
WAD_SRC ?= /mnt/c/Users/Sam/.gemini/antigravity/scratch/genesis-DOOM64KB/scripts/doom64.wad
PBL_RES = resources/e1m1.pbl

all: $(PBW)

%.o: %.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

pdoom.elf: $(OBJS)
	@echo "LINK $@"
	@$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	@echo ""
	@echo "=== pDOOM Binary Size ==="
	@$(SIZE) -A -d pdoom.elf
	@echo ""
	@$(SIZE) pdoom.elf

$(PBL_RES): tools/pack_e1m1.py
	@echo "PACK $(PBL_RES)"
	@python3 tools/pack_e1m1.py $(WAD_SRC) $(PBL_RES)

$(PBW): pdoom.elf $(PBL_RES) tools/build_pbw.py
	@echo "BUILD $(PBW)"
	@python3 tools/build_pbw.py . $(PBW)
	@python3 tools/verify_pbw.py

clean:
	rm -f $(OBJS) pdoom.elf pdoom.map pdoom.bin $(PBW)

.PHONY: all clean pbw

