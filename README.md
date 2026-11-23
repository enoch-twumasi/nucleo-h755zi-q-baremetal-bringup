# nucleo-h755zi-q-baremetal-bringup

Bare-metal ARM Cortex-M7 (Core 1) firmware bring-up for the **STM32 Nucleo-H755ZI-Q**, written in freestanding ISO C23 (`-std=c23`) without STM32 HAL, CMSIS headers, `crt0`, or `libc` (`-nostdlib`).

> All build rules, linker definitions, startup code, and register-level firmware in this repository were hand-written directly by **Enoch Twumasi** without CubeMX, STM32 HAL, CMSIS headers, or generated code.

At runtime, the firmware enables Core 1 AHB4 peripheral clocks for `GPIOB` and `GPIOE`, configures `PB0`, `PB14`, and `PE1` as push-pull outputs, drives them high (lighting the three Nucleo user LEDs), and enters an idle `nop` loop. Beyond that visible output, the repository implements the complete bare-metal foundation required to boot a Cortex-M7 from reset: a custom GNU `ld` memory layout, a C runtime startup sequence with an X-Macro interrupt vector table, direct reference-manual register control, and a deterministic GNU Make + OpenOCD/GDB workflow.

---

## Engineering Highlights

* **Custom Linker Architecture ([`linker/stm32h755_m7.ld`](linker/stm32h755_m7.ld)):** Defines a 2 MB `FLASH` (`0x08000000`) and 512 KB `AXI_RAM` (`0x24000000`) layout with Flash-to-RAM `.data` LMA/VMA relocation, a 1024-byte VTOR-aligned `.isr_vector` section, an 8 KB AAPCS-aligned top-of-RAM `.stack` reservation, and link-time `ASSERT` checks for vector-table size and stack-collision protection.
* **Freestanding C23 Startup & Vector Table ([`startup/startup_stm32h755_m7.c`](startup/startup_stm32h755_m7.c)):** Implements `Reset_Handler` (`.data` copy, `.bss` zero-initialization, `main()` invocation, and return trap) alongside a 166-entry (664-byte) Cortex-M7 vector table generated from a single `INTERRUPT_LIST` X-Macro using C designated initializers and weak aliases to `Default_Handler`.
* **Direct Register-Level Bring-Up ([`src/main.c`](src/main.c)):** Addresses STM32H755 D3-domain AHB4 peripherals directly via `volatile uint32_t *` pointers, using Core 1's dedicated `RCC_C1_AHB4ENR` clock-enable register and clearing STM32H7's `11` analog reset bits in `GPIOx_MODER` before setting output mode (`01`).
* **Deterministic Build & Debug Workflow ([`Makefile`](Makefile)):** Disables implicit rules (`--no-builtin-rules`), enforces strict `-std=c23 -ffreestanding -Wall -Wextra -Wpedantic`, strips unused sections (`-ffunction-sections -fdata-sections` + `-Wl,--gc-sections`), tracks header dependencies (`-MMD -MP`), isolates `debug` (`-O0 -g3`) and `release` (`-O2 -g3`) outputs, and integrates OpenOCD/GDB targets.

For a detailed technical walkthrough of the boot sequence, memory layout mechanics, cross-file ABI contracts, and design trade-offs, see [`docs/architecture.md`](docs/architecture.md).

---

## Repository Structure

```text
nucleo-h755zi-q-baremetal-bringup/
├── docs/
│   └── architecture.md              # Technical architecture, boot flow, and design notes
├── linker/
│   └── stm32h755_m7.ld              # Memory map, LMA/VMA relocation, stack reservation
├── src/
│   └── main.c                       # Core 1 RCC/GPIO register bring-up (PB0, PB14, PE1)
├── startup/
│   └── startup_stm32h755_m7.c       # Reset_Handler, Default_Handler, X-Macro vector table
└── Makefile                         # Build, inspection, OpenOCD flash, and GDB workflow
```

Build outputs are placed under `build/debug/` or `build/release/` (with mirrored `src/` and `startup/` object directories) and are excluded from versioned source.

---

## Target Hardware & Scope

* **Board / MCU:** STMicroelectronics STM32 Nucleo-H755ZI-Q (`MB1363`), STM32H755ZI (ARM Cortex-M7, Core 1)

| MCU Pin | Peripheral / Bus | Nucleo-H755ZI-Q Function (UM2408) | Configured Mode (`src/main.c`) |
| :--- | :--- | :--- | :--- |
| `PB0` | `GPIOB` (D3 AHB4) | `LD1` (Green user LED) | General-purpose output, driven high (`1`) |
| `PE1` | `GPIOE` (D3 AHB4) | `LD2` (Yellow user LED) | General-purpose output, driven high (`1`) |
| `PB14` | `GPIOB` (D3 AHB4) | `LD3` (Red user LED) | General-purpose output, driven high (`1`) |

* **Scope Boundaries:**
  * Targets **Cortex-M7 (Core 1)** exclusively; no Cortex-M4 image or inter-core synchronization is included, and the Cortex-M7 linker script assumes full ownership of internal Flash and AXI SRAM.
  * Leaves system clocks (internal HSI reset default), `PWR` supply configuration, and GPIO output type/speed/pull registers (`OTYPER`, `OSPEEDR`, `PUPDR`) at hardware reset defaults.

---

## Prerequisites

The [`Makefile`](Makefile) requires:

* **ARM EABI Toolchain:** `arm-none-eabi-gcc` (with `-std=c23` support), `arm-none-eabi-objcopy`, `arm-none-eabi-objdump`, `arm-none-eabi-size`, and `arm-none-eabi-gdb` (overridable via `PREFIX`).
* **Build Tools:** GNU Make and a POSIX shell (`/bin/sh`).
* **Hardware Flash / Debug:** `openocd` with `interface/stlink.cfg` and `target/stm32h7x.cfg`, plus an ST-LINK USB connection to the Nucleo-H755ZI-Q.

---

## Build, Memory Footprint, and Hardware Usage

### Building Firmware

```sh
# Default debug build (-O0 -g3) -> build/debug/
make

# Release build (-O2 -g3) -> build/release/
make BUILD=release
```

Both configurations generate `firmware.elf`, `firmware.bin`, `firmware.hex`, `firmware.map`, and `firmware.lss` under `build/<BUILD>/` and print section/region sizes upon linking.

#### Overridable Build Options (`make VAR=value`)

| Variable | Default | Description |
| :--- | :--- | :--- |
| `BUILD` | `debug` | `debug` (`-O0 -g3`) or `release` (`-O2 -g3`) |
| `TARGET` | `firmware` | Output artifact base filename |
| `BUILD_ROOT` | `build` | Root directory for generated build artifacts |
| `PREFIX` | `arm-none-eabi-` | Cross-toolchain executable prefix |
| `V` | `0` | Set `V=1` to print full compiler/linker shell commands |

### Verified Memory Footprint

Measured from `make all` (`debug`), `make BUILD=release` (`release`), and `arm-none-eabi-size -A`:

| Build Profile | `.isr_vector` | `.text` | `.rodata` | `.data` | `.bss` | `.stack` (Reserved) | Total Flash Used | `size` `dec` |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `debug` (`-O0 -g3`) | 664 B | 228 B | 0 B | 0 B | 0 B | 8,192 B | **892 B** / 2 MB (0.04%) | 9,084 B (`0x237c`) |
| `release` (`-O2 -g3`) | 664 B | 216 B | 0 B | 0 B | 0 B | 8,192 B | **880 B** / 2 MB (0.04%) | 9,072 B (`0x2370`) |

> **Note on `ld --print-memory-usage` output:** During linking, GNU `ld` reports `AXI_RAM: 512 KB / 512 KB (100.00%)`. This reflects the region's high-water mark—not RAM exhaustion—because `.stack` is anchored at the top of `AXI_RAM` (`0x2407E000`–`0x24080000`). Actual static/reserved RAM is **8,192 B** (`.stack`), leaving **504 KB** (`0x24000000`–`0x2407DFFF`) unallocated. See [`docs/architecture.md`](docs/architecture.md) for the full memory map.

### Flashing, Debugging, and Utility Targets

The [`Makefile`](Makefile) defines the following utility targets:

```sh
# Program and verify ELF via OpenOCD + ST-LINK, then reset and run
make flash

# Two-terminal GDB debugging session
make openocd   # Terminal 1: start OpenOCD GDB server (localhost:3333)
make debug     # Terminal 2: attach GDB, reset halt, load ELF, break at main, continue
```

| Target | Action |
| :--- | :--- |
| `make all` (default) | Build `.elf`, `.bin`, `.hex`, `.map`, and `.lss` artifacts |
| `make flash` | Flash and verify `$(ELF)` via OpenOCD (`reset exit`) |
| `make openocd` | Launch OpenOCD server with `interface/stlink.cfg` and `target/stm32h7x.cfg` |
| `make debug` | Connect `arm-none-eabi-gdb` to `localhost:3333`, load `$(ELF)`, and run to `main` |
| `make size` | Print Berkeley section sizes for `$(ELF)` |
| `make clean` | Remove active `build/<BUILD>/` directory |
| `make clean-all` | Remove entire `build/` directory |
| `make help` | Print targets and override options |

---

## Verification Status

* **Completed and Verified:**
  * Clean compilation and linking of both `debug` (`make all`, `-O0 -g3`) and `release` (`make BUILD=release`, `-O2 -g3`) profiles, verified with `arm-none-eabi-size -A` section breakdowns.
  * Execution of both `debug` and `release` firmware builds tested and confirmed working on hardware.
* **Implemented Infrastructure Not Exercised by Current Application:**
  * [`startup/startup_stm32h755_m7.c`](startup/startup_stm32h755_m7.c) and [`linker/stm32h755_m7.ld`](linker/stm32h755_m7.ld) implement Flash-to-RAM `.data` initialization, `.bss` zero-fill, and weak-aliased interrupt vector overrides for future expansion, which are not exercised by [`src/main.c`](src/main.c) (`0 B` `.data`/`.bss` and no enabled interrupts).
  * Individual convenience targets (`clean`, `clean-all`, `size`, `help`, and separate `flash` vs. `openocd`/`debug` paths) are documented as implemented in [`Makefile`](Makefile) rather than claimed as individually tested.

---

## Further Documentation

* **[`docs/architecture.md`](docs/architecture.md):** End-to-end technical reference covering the compiler/linker ABI contract, `FLASH`/`AXI_RAM` section layout and `AXI_RAM` high-water-mark behavior, `Reset_Handler` boot flow, X-Macro vector table generation, STM32H755 register rationale, and known design trade-offs.