# ===============================================================================================
# 1. DECLARATIONS
# ===============================================================================================

# BUILD CONFIGURATION (Environment, Tools, Flags) & Project Inputs and Outputs(Build Artifacts)
# ===============================================================================================

# -----------------------------------------------------------------------------------------------
# Environment
# -----------------------------------------------------------------------------------------------
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

SHELL := /bin/sh
.DEFAULT_GOAL := all

.DELETE_ON_ERROR:

# Overridable Configurations
# -----------------------------------------------------------------------------------------------
TARGET     ?= firmware
BUILD      ?= debug
BUILD_ROOT ?= build
BUILD_DIR  ?= $(BUILD_ROOT)/$(BUILD)
PREFIX     ?= arm-none-eabi-
V          ?= 0

ifeq ($(BUILD),debug)
	OPT_FLAGS := -O0 -g3
else ifeq ($(BUILD),release)
	OPT_FLAGS := -O2 -g3
else
	$(error Invalid BUILD='$(BUILD)'. Expected 'debug' or 'release')
endif

ifeq ($(V),1)
	Q :=
else
	Q := @
endif

# -----------------------------------------------------------------------------------------------
# Tools
# -----------------------------------------------------------------------------------------------
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE    := $(PREFIX)size
GDB     := $(PREFIX)gdb
OPENOCD ?= openocd

# -----------------------------------------------------------------------------------------------
# Project Inputs and Outputs (Build Artifacts)
# -----------------------------------------------------------------------------------------------

# Project Inputs
# -----------------------------------------------------------------------------------------------
SRCS := \
	src/main.c \
	startup/startup_stm32h755_m7.c

INC_DIRS := 

LDSCRIPT := linker/stm32h755_m7.ld

# Output (Build Artifacts)
# -----------------------------------------------------------------------------------------------
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

ELF  := $(BUILD_DIR)/$(TARGET).elf
BIN  := $(BUILD_DIR)/$(TARGET).bin
HEX  := $(BUILD_DIR)/$(TARGET).hex
MAP  := $(BUILD_DIR)/$(TARGET).map
LSS  := $(BUILD_DIR)/$(TARGET).lss

# unique Output directories stripped of trailing slashes for order-only rules
ALL_DIRS := $(patsubst %/,%,$(sort $(BUILD_DIR) $(dir $(OBJS))))

# -----------------------------------------------------------------------------------------------
# Flags
# -----------------------------------------------------------------------------------------------

# Tool chain Flags
# -----------------------------------------------------------------------------------------------
# Shared architecture/ABI flags
ARCH_FLAGS := -mcpu=cortex-m7 -mthumb

#Preprocessor flags
CPPFLAGS := $(addprefix -I,$(INC_DIRS)) -MMD -MP

# C	compiler flags
CFLAGS := $(ARCH_FLAGS) \
          -std=c23 \
		  -ffreestanding \
		  -Wall -Wextra -Wpedantic \
		  -ffunction-sections -fdata-sections \
		  $(OPT_FLAGS)

#Linker Flags
LDFLAGS := $(ARCH_FLAGS) \
           -T $(LDSCRIPT) \
		   -nostdlib \
		   -Wl,-Map=$(MAP) \
		   -Wl,--gc-sections \
		   -Wl,--print-memory-usage

# OpenOCD Hardware Interface Configuration & flags
# -----------------------------------------------------------------------------------------------
OPENOCD_IFACE  := interface/stlink.cfg
OPENOCD_TARGET := target/stm32h7x.cfg
OPENOCD_FLAGS  := -f $(OPENOCD_IFACE) -f $(OPENOCD_TARGET)

# ===============================================================================================
# 2. BUILD RULES
# ===============================================================================================

# Artifacts Build Rules
# ===============================================================================================
.PHONY: all flash openocd debug size clean clean-all help

all: $(ELF) $(BIN) $(HEX) $(LSS)

# Link ELF
$(ELF): $(OBJS) $(LDSCRIPT) Makefile | $(BUILD_DIR)
	@echo "LD $@"
	$(Q)$(CC) $(LDFLAGS) $(OBJS) -o $@
	$(Q)$(SIZE) $@

# Compile C sources into path-mirrored + .d dependecy files
$(BUILD_DIR)/%.o: %.c Makefile | $(ALL_DIRS)
	@echo "CC  $<"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Extract raw binary images
$(BIN): $(ELF)
	@echo "BIN  $@"
	$(Q)$(OBJCOPY) -O binary $< $@

# Extract Intel HEX images
$(HEX): $(ELF)
	@echo "HEX  $@"
	$(Q)$(OBJCOPY) -O ihex $< $@
	
# Generate disassembly listing
$(LSS): $(ELF)
	@echo "LSS  $@"
	$(Q)$(OBJDUMP) -h -d -S $< > $@

# Order-only directory creation (safe for parallel 'make -j')
$(ALL_DIRS):
	$(Q)mkdir -p $@


# Utility Rules
# ===============================================================================================
flash: $(ELF)
	@echo "FLASH $(ELF)"
	$(Q)$(OPENOCD) $(OPENOCD_FLAGS) -c "program $(ELF) verify reset exit"

openocd:
	@echo "Starting OpenOCD server (Ctrl+C to stop)..."
	$(Q)$(OPENOCD) $(OPENOCD_FLAGS)

debug: $(ELF)
	@echo "GDB $(ELF)"
	$(Q)$(GDB) $(ELF) \
		-ex "target remote localhost:3333" \
		-ex "monitor reset halt" \
		-ex "load" \
		-ex "b main" \
		-ex "continue"
	
size: $(ELF)
	$(Q)$(SIZE) $(ELF)

clean:
	@echo "CLEAN $(BUILD_DIR)"
	$(Q)rm -rf $(BUILD_DIR)

clean-all:
	@echo "CLEAN $(BUILD_ROOT)"
	$(Q)rm -rf $(BUILD_ROOT)

help:
	@echo "STM32H755ZI-Q (Cortex-M7) Bare-Metal Build System"
	@echo ""
	@echo "Targets:"
	@echo " all          Build ELF, BIN, HEX, MAP, and LSS artifacts (default)"
	@echo " flash        Program and verify $(ELF) via OpenOCD"
	@echo " openocd      Start OpenOCD GDB server (Terminal 1)"
	@echo " debug        Connect GDB to OpenOCD, load ELF, break at main (Terminal 2)"
	@echo " size         Display Flash/RAM section sizes for $(ELF)"
	@echo " clean        Remove active build directory ($(BUILD_DIR))"
	@echo " clean-all    Remove entire build root ($(BUILD_ROOT))"
	@echo " help         Display this help message"
	@echo ""
	@echo "Options (override via 'make VAR=value'):"
	@echo " BUILD        debug (default: -O0 -g3) | release (-O2 -g3)"
	@echo " TARGET       Artifact base name (default: $(TARGET))"
	@echo " PREFIX       Cross-toolchain prefix (default: $(PREFIX))"
	@echo " V            Set V=1 to print full shell commands (default: $(V))"

# Include compiler-generated header dependecies (ignored on first clean build)
-include $(DEPS)
