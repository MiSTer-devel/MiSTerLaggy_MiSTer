CC = m68k-elf-gcc
OBJCOPY = m68k-elf-objcopy

TARGET = finalb_test
SRCS = init.c main.c interrupts_default.c printf/printf.c

BUILD_DIR = build

OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:c=o))
BUILD_DIRS = $(sort $(dir $(OBJS))) 
GLOBAL_DEPS = Makefile

DEFINES = -DPRINTF_SUPPORT_DECIMAL_SPECIFIERS=0 \
	-DPRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS=0 \
	-DPRINTF_SUPPORT_LONG_LONG=0 \
	-DPRINTF_ALIAS_STANDARD_FUNCTION_NAMES=1 \
	-DPRINTF_ALIAS_STANDARD_FUNCTION_NAMES_HARD=1

CFLAGS = -march=68000 -ffreestanding $(DEFINES) -O2
LIBS = -lgcc
LDFLAGS = -march=68000 -static -nostdlib

all: $(BUILD_DIR)/cpu_low.hex $(BUILD_DIR)/cpu_high.hex

$(BUILD_DIR)/cpu_high.hex: $(BUILD_DIR)/cpu.elf
	$(OBJCOPY) -S -i 2 -b 0 -O ihex $< $@

$(BUILD_DIR)/cpu_low.hex: $(BUILD_DIR)/cpu.elf
	$(OBJCOPY) -S -i 2 -b 1 -O ihex $< $@

$(BUILD_DIR)/%.o: src/%.c $(GLOBAL_DEPS) | $(BUILD_DIRS)
	@echo $@
	@$(CC) -MMD -o $@ $(CFLAGS) -c $<

$(BUILD_DIR)/cpu.elf: $(OBJS)
	@echo $@
	@$(CC) -T src/mrlaggy.ld -o $@ $(LDFLAGS) $^ $(LIBS)

$(BUILD_DIRS):
	mkdir -p $@

-include $(OBJS:o=d)