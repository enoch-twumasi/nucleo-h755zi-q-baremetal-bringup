# Technical Architecture & Implementation Reference

This document details the internal architecture of `nucleo-h755zi-q-baremetal-bringup` across its build configuration, linker memory layout, C runtime startup sequence, and register-level hardware initialization.

For build commands, memory-footprint quick reference, and OpenOCD/GDB usage, see [`../README.md`](../README.md).

---

## 1. Architectural Overview and System Boundaries

The firmware is a single-core **ARM Cortex-M7 (Core 1)** bare-metal bring-up for the dual-core **STM32H755ZI-Q** (`MB1363`), implemented from hardware reference specifications without STM32 HAL, CMSIS headers, compiler startup files (`crt0`), or a C standard library (`libc`/`libgcc`).

```text
  [Makefile]                     [linker/stm32h755_m7.ld]         [startup_stm32h755_m7.c]          [src/main.c]
  -mcpu=cortex-m7 -mthumb   ──►  FLASH (0x08000000, 2 MB)    ──►  g_pfnVectors[] (.isr_vector) ──►  int main(void)
  -std=c23 -ffreestanding        AXI_RAM (0x24000000, 512 KB)     [0] _stack_top                    RCC_C1_AHB4ENR
  -nostdlib --gc-sections        LMA/VMA (.data), .bss, .stack    [1] Reset_Handler ──► .data/.bss  GPIOB/GPIOE MODER/ODR
```

### Scope Boundaries
* **Core Ownership:** Targets Core 1 (Cortex-M7) exclusively. No Cortex-M4 image is built, and the linker script assigns the full 2 MB internal Flash and 512 KB AXI SRAM to Cortex-M7.
* **Reset-Default Environment:** The bring-up relies on hardware reset defaults for the system clock (internal HSI), `PWR` supply configuration, Flash wait states, instruction/data caches, and secondary GPIO electrical attributes (`OTYPER`, `OSPEEDR`, `PUPDR`).

---

## 2. Toolchain, ABI, and Build Contract (`Makefile`)

The compiler and linker flags in [`../Makefile`](../Makefile) establish strict constraints that [`../linker/stm32h755_m7.ld`](../linker/stm32h755_m7.ld), [`../startup/startup_stm32h755_m7.c`](../startup/startup_stm32h755_m7.c), and [`../src/main.c`](../src/main.c) must satisfy together.

* **Target Architecture & Soft-Float ABI (`-mcpu=cortex-m7 -mthumb`):**
  Generates Thumb-2 instructions for ARMv7E-M. Neither `-mfpu` nor `-mfloat-abi` is specified, so `arm-none-eabi-gcc` defaults to the software floating-point ABI. Because compiler-generated code will not emit VFP instructions, `Reset_Handler` does not need to enable CP10/CP11 in `SCB->CPACR`.
* **Freestanding ISO C23 & Standard Library Exclusion (`-std=c23 -ffreestanding -nostdlib`):**
  * `-ffreestanding` removes hosted C library assumptions while restricting standard includes to freestanding headers (`<stdint.h>`).
  * `-Wall -Wextra -Wpedantic` enforces strict ISO C23 conformance, driving idioms such as GCC's reserved `__asm volatile(...)` keyword (rather than non-standard `asm`) and casting `&_stack_top` through `uintptr_t` in the vector table.
  * `-nostdlib` omits both standard startup objects (`crt0.o`) and default libraries (`libc`, `libm`, `libgcc`). Consequently, the startup unit must supply the vector table and C runtime initialization, and application/startup code cannot rely on external `libc` or `libgcc` runtime helpers.
* **Dead-Code Elimination (`-ffunction-sections -fdata-sections` + `-Wl,--gc-sections`):**
  Each function and static object is placed into its own ELF section (`.text.*`, `.rodata.*`, `.data.*`, `.bss.*`) so the linker discards unreferenced code and data. To prevent the vector table and startup path from being pruned (since no C call references `g_pfnVectors`), the linker script declares `ENTRY(Reset_Handler)` and `KEEP(*(.isr_vector))`, paired with `__attribute__((section(".isr_vector"), used))` in C.
* **Build Determinism & `-g3` Debug Symbols:**
  * `--no-builtin-rules` and `.SUFFIXES:` disable implicit Make rules; `.DELETE_ON_ERROR:` removes partial targets (such as redirected `.lss` disassembly output) on failure.
  * `-MMD -MP` generates header dependency files, and `Makefile` itself is listed as a prerequisite of compilation and linking so flag edits trigger rebuilds.
  * Both `debug` (`-O0 -g3`) and `release` (`-O2 -g3`) retain macro-level DWARF debug symbols (`-g3`). As confirmed by `arm-none-eabi-size -A`, `.debug_macro` (`4,335 B`) and `.debug_str` (~`20 KB`) make up most of the ~35 KB ELF file size while contributing zero bytes to Flash or `.bin`/`.hex` images.

---

## 3. Memory Architecture and Linker Script (`linker/stm32h755_m7.ld`)

### 3.1 Memory Region Selection & Trade-Offs

[`../linker/stm32h755_m7.ld`](../linker/stm32h755_m7.ld) defines two memory regions:

* **`FLASH (rx)`:** `ORIGIN = 0x08000000`, `LENGTH = 2048K` (contiguous 2 MB internal Flash across Bank 1 and Bank 2).
* **`AXI_RAM (rwx)`:** `ORIGIN = 0x24000000`, `LENGTH = 512K` (D1-domain AXI SRAM).

**Design Rationale:** On the STM32H755, Cortex-M7 also has access to DTCM (`0x20000000`, 128 KB), ITCM (`0x00000000`, 64 KB), and D2/D3 AHB SRAMs (`0x30000000`, `0x38000000`). Placing `.data`, `.bss`, and `.stack` in `AXI_RAM` instead of DTCM or D2 SRAM provides two practical advantages for minimal bring-up:
1. **Unified 512 KB SRAM space** without splitting stack/data across separate 128 KB DTCM and AXI regions.
2. **Immediate reset availability:** D1 AXI SRAM is clocked out of reset, whereas D2 SRAM1/2/3 require explicit RCC clock enables before `.data`/`.bss` initialization can touch them.

The trade-off is foregoing DTCM's zero-wait-state tightly coupled latency and assuming single-core M7 ownership of the entire 2 MB Flash and 512 KB AXI SRAM.

### 3.2 Physical Memory Map & Verified Section Layout

The following map reflects the verified `debug` (`-O0`) and `release` (`-O2`) builds measured via `arm-none-eabi-size -A`:

```text
  FLASH (0x08000000 .. 0x081FFFFF, 2048 KB)
  ┌──────────────────────────────────────────┐ 0x08000000
  │ .isr_vector   (664 B, KEEP, ALIGN(1024)) │   166 entries (slots 0..165)
  ├──────────────────────────────────────────┤ 0x08000298
  │ .text         (228 B debug / 216 B rel)  │   Reset_Handler, Default_Handler, main
  │ .rodata       (0 B)                      │
  │ .ARM.extab    (0 B)                      │   Forward-compatibility unwind sections
  │ .ARM.exidx    (0 B)                      │   (__exidx_start .. __exidx_end)
  │ .data (LMA)   (0 B, AT > FLASH)          │ ◄── _sidata = LOADADDR(.data)
  ├──────────────────────────────────────────┤
  │ Unallocated Flash (~2047 KB)             │
  └──────────────────────────────────────────┘ 0x081FFFFF

  AXI_RAM (0x24000000 .. 0x2407FFFF, 512 KB)
  ┌──────────────────────────────────────────┐ 0x24000000
  │ .data (VMA)   (0 B)                      │ ◄── _sdata .. _edata
  │ .bss          (0 B)                      │ ◄── _sbss .. _ebss
  ├──────────────────────────────────────────┤ 0x24000000 (8-byte aligned)
  │                                          │ ◄── _end / PROVIDE(end = _end)
  │ Unallocated RAM (504 KB / 516,096 B)     │
  │ (Available for heap / stack headroom)    │
  ├──────────────────────────────────────────┤ 0x2407E000 (_stack_limit, 8-byte aligned)
  │ .stack (NOLOAD, _Min_Stack_Size = 0x2000)│   8,192 B reserved for descending MSP
  └──────────────────────────────────────────┘ 0x24080000 (_stack_top, 8-byte aligned)
```

### 3.3 VTOR Alignment, Relocation, and Link-Time Guards

* **VTOR Alignment & Table Size Assertion:** On ARMv7-M, the Vector Table Offset Register (`VTOR`) requires base alignment to a power of two greater than or equal to the total vector table size. Because 166 entries occupy 664 bytes, the next power of two is `1024`. Placing `.isr_vector` first in `FLASH` (`0x08000000`) with `ALIGN(1024)` enforces this hardware constraint, and `ASSERT(SIZEOF(.isr_vector) <= 1024, ...)` prevents silent table overflow if entries are added later.
* **LMA/VMA `.data` Relocation:** `.data` uses `> AXI_RAM AT > FLASH`, assigning its runtime VMA in `AXI_RAM` while storing initial values in `FLASH` immediately after preceding Flash sections. `_sidata = LOADADDR(.data)` exposes the Flash LMA start, while `.data` and `.bss` boundaries are padded with `ALIGN(4)` so `Reset_Handler` can copy and zero memory in 32-bit `uint32_t` words.
* **Top-of-RAM `.stack` & Overlap Guard:** `.stack` is marked `(NOLOAD)` (preventing empty RAM padding from appearing in `.bin`/`.hex` Flash images) and explicitly positioned at `ORIGIN(AXI_RAM) + LENGTH(AXI_RAM) - _Min_Stack_Size` (`0x2407E000`). Both `_end` (with `PROVIDE(end = _end)` for future `_sbrk`/heap compatibility) and `.stack` enforce 8-byte alignment to satisfy ARM AAPCS stack-alignment rules. `ASSERT(_end <= _stack_limit, ...)` rejects builds at link time if static RAM encroaches on the 8 KB stack reserve. (At runtime, Cortex-M7 has no hardware `MSPLIM` register like ARMv8-M, so overflow past `_stack_limit` is not hardware-trapped without MPU configuration.)

### 3.4 Interpreting `ld --print-memory-usage` vs. `arm-none-eabi-size`

During linking, GNU `ld --print-memory-usage` outputs:

```text
Memory region         Used Size  Region Size  %age Used
           FLASH:         892 B         2 MB      0.04%
         AXI_RAM:        512 KB       512 KB    100.00%
```

`AXI_RAM: 512 KB (100.00%)` does **not** indicate that RAM is exhausted. GNU `ld` computes region utilization from the **highest VMA address occupied in the region** minus the region's `ORIGIN` (`0x24080000 - 0x24000000 = 512 KB`). Because `.stack` is intentionally anchored at the top of `AXI_RAM` (`0x2407E000`–`0x24080000`), the high-water mark sits at the top of RAM regardless of how small `.data` and `.bss` are. Cross-checking with `arm-none-eabi-size -A` confirms actual static/reserved RAM usage is **8,192 B** (`.stack`), leaving **504 KB** (`0x24000000`–`0x2407DFFF`) unallocated.

### 3.5 Linker-to-Startup Symbol Interface

| Symbol / Section | Defined In | Consumed By | Role |
| :--- | :--- | :--- | :--- |
| `.isr_vector` | `startup_stm32h755_m7.c` | `stm32h755_m7.ld` | Input vector section retained via `KEEP(*(.isr_vector))` |
| `Reset_Handler` | `startup_stm32h755_m7.c` | `stm32h755_m7.ld` | ELF `ENTRY(Reset_Handler)` root and vector slot `[1]` |
| `_stack_top` | `stm32h755_m7.ld` | `startup_stm32h755_m7.c` | Initial Main Stack Pointer (`0x24080000`) at vector slot `[0]` |
| `_sidata` | `stm32h755_m7.ld` | `startup_stm32h755_m7.c` | Flash LMA start of `.data` (`LOADADDR(.data)`) |
| `_sdata`, `_edata` | `stm32h755_m7.ld` | `startup_stm32h755_m7.c` | AXI SRAM VMA start and end of `.data` |
| `_sbss`, `_ebss` | `stm32h755_m7.ld` | `startup_stm32h755_m7.c` | AXI SRAM VMA start and end of `.bss` |
| `_stack_limit` | `stm32h755_m7.ld` | Link-time `ASSERT` | Lower bound of reserved 8 KB stack (`0x2407E000`) |
| `_end`, `end` | `stm32h755_m7.ld` | Link-time `ASSERT` / future heap | 8-byte aligned end of static RAM (`PROVIDE(end = _end)`) |
| `__exidx_start`, `__exidx_end` | `stm32h755_m7.ld` | Future stack unwinder | Bounds of `.ARM.exidx` (empty under current C23 build) |

---

## 4. Boot Flow, C Runtime Initialization, and Vector Table (`startup/startup_stm32h755_m7.c`)

### 4.1 Reset Sequence & `Reset_Handler`

On power-up or reset, STM32H755 Core 1 initializes `VTOR` from the `BOOT_CM7_ADD0` option byte (default `0x08000000`), fetches word `[0]` (`_stack_top` = `0x24080000`) into `MSP`, and branches to word `[1]` (`Reset_Handler`, whose Thumb interworking bit `0` is automatically set by the ARM ELF linker relocation).

`Reset_Handler` then establishes the C runtime environment:
1. **`.data` Relocation:** Copies 32-bit words from `&_sidata` (Flash LMA) to `&_sdata .. &_edata` (AXI SRAM VMA).
2. **`.bss` Zero-Initialization:** Writes `0U` word-by-word across `&_sbss .. &_ebss`.
3. **Application Entry & Return Trap:** Calls `(void)main()`. If `main()` were ever to return, execution falls into an infinite `for (;;) { __asm volatile ("nop"); }` loop rather than wandering into unmapped memory.

> **Current Image Behavior:** Because neither `startup/startup_stm32h755_m7.c` nor `src/main.c` declares mutable global or static variables (`g_pfnVectors` is `const` in `.isr_vector`), `_sdata == _edata` and `_sbss == _ebss` in the current build (`0 B` `.data` and `0 B` `.bss`). Both initialization loops therefore execute zero iterations today while remaining active for future global/static state.

### 4.2 X-Macro Interrupt Vector Matrix

Rather than maintaining parallel prototype lists and vector array entries manually, [`../startup/startup_stm32h755_m7.c`](../startup/startup_stm32h755_m7.c) centralizes exception and IRQ slots `2..165` in a single `INTERRUPT_LIST(H, R)` X-Macro (covering 16 Cortex-M7 system exceptions `[0..15]` and 150 STM32H755 external IRQs `[16..165]`, including reserved gaps `7–10`, `13`, `58`, `80–83`, `142`, and `163`, plus dual-core slots such as `HSEM0_Handler` at `141` and `HOLD_CORE_Handler` at `164`):

1. **First Expansion — Weak Handler Prototypes:**
   `DECLARE_HANDLER_PROTOTYPE(idx, name)` expands every `H(idx, name)` into:
   ```c
   void name(void) __attribute__((weak, alias("Default_Handler")));
   ```
   while `IGNORE_RESERVED(idx)` elides `R(idx)`. Any translation unit can override an exception or peripheral IRQ simply by defining a non-weak function with the matching name.
2. **Second Expansion — Designated-Initializer Vector Table:**
   `AS_VECTOR(idx, name)` and `AS_RESERVED(idx)` expand `INTERRUPT_LIST` inside `g_pfnVectors[]` using C designated initializers (`[idx] = name,`, `[idx] = 0,`), guaranteeing exact hardware slot alignment across all **166 entries (664 bytes)**.
3. **Strict C23 `-Wpedantic` Pointer Conversion:**
   Slot `[0]` converts `&_stack_top` (`uint32_t *`) via `(ISR_Handler_t)(uintptr_t)&_stack_top`. Routing through `uintptr_t` avoids ISO C `-Wpedantic` diagnostics for direct object-pointer-to-function-pointer conversions while preserving the 32-bit stack-top address.

### 4.3 Intentional Startup Omissions

Consistent with the project's minimal bring-up scope, `Reset_Handler` deliberately omits:
* Explicit `SCB->VTOR` initialization (relying on `BOOT_CM7_ADD0` defaulting to `0x08000000`).
* FPU `SCB->CPACR` enable (consistent with soft-float ABI in `Makefile`).
* Cortex-M7 I-Cache/D-Cache enable, MPU setup, and early `SystemInit` clock/PWR setup.

---

## 5. Register-Level Peripheral Bring-Up (`src/main.c`)

### 5.1 Direct Memory-Mapped Access

[`../src/main.c`](../src/main.c) accesses peripheral registers directly via `(*(volatile uint32_t *)(BASE + OFFSET))` macros without CMSIS struct overlays. `volatile` prevents the compiler—particularly under `-O2` (`release`)—from caching, merging, or eliding peripheral reads and writes.

### 5.2 Core 1 Clock Allocation (`RCC_C1_AHB4ENR`)

Per STMicroelectronics RM0399, STM32H755 GPIO ports and RCC reside in the D3-domain AHB4 bus matrix:

* `GPIOB_BASE` = `0x58020400`
* `GPIOE_BASE` = `0x58021000`
* `RCC_BASE` = `0x58024400`

Rather than writing the global `RCC_AHB4ENR` (`0x0E0`), `main()` enables `GPIOBEN` (`1U << 1`) and `GPIOEEN` (`1U << 4`) in **`RCC_C1_AHB4ENR` (`0x140`)**, the dedicated Cortex-M7 (Core 1) peripheral allocation register. On the dual-core STM32H755, Core 1 and Core 2 maintain separate allocation registers (`C1` vs. `C2`) so the hardware tracks peripheral clock ownership per core.

### 5.3 GPIO Mode Reset State & Output Control

On STM32H7, non-debug GPIO pins reset in `GPIOx_MODER` (`0x00`) to `11` (**analog mode**) rather than `00`. Simply ORing bit `0` (`01`) without clearing bit `1` first would leave the pin in analog mode (`11`). Accordingly, `main()` executes a two-step read-modify-write for `PB0` (`[1:0]`), `PB14` (`[29:28]`), and `PE1` (`[3:2]`):

1. Clear both mode bits (`&= ~(MODERx_0 | MODERx_1)`).
2. Set bit `0` (`|= MODERx_0`) to select **General Purpose Output (`01`)**.

It then sets `PB0`, `PB14`, and `PE1` high once in `GPIOx_ODR` (`0x14`) and enters `while (1) { __asm volatile ("nop"); }`. Per UM2408 (`MB1363`), `PB0`, `PE1`, and `PB14` drive the Nucleo-H755ZI-Q's `LD1` (green), `LD2` (yellow), and `LD3` (red) user LEDs, while `OTYPER` (push-pull `0`), `OSPEEDR` (low speed `00`), and `PUPDR` (no pull `00`) remain at reset defaults.

---

## 6. Design Trade-Offs, Known Limitations, and Expansion Considerations

1. **Debugger-Coupled `Default_Handler` (`bkpt #0`):**
   `Default_Handler` executes `__asm volatile ("bkpt #0")` before its `nop` loop. With OpenOCD/GDB attached (`C_DEBUGEN == 1`), any unexpected exception or unimplemented IRQ halts the core immediately at the handler for diagnosis. However, in standalone operation without a debugger (`C_DEBUGEN == 0`), ARMv7-M escalates `BKPT` to `HardFault` (if `DebugMonitor` is disabled); because `HardFault_Handler` also aliases `Default_Handler`, an unhandled interrupt in standalone execution will fault-escalate into CPU lockup.
2. **Unexercised Runtime Infrastructure:**
   Both `debug` and `release` builds and hardware execution are verified, but because `src/main.c` declares zero mutable globals/statics and enables no interrupts, `.data` relocation, `.bss` zeroing, and weak-alias ISR overrides are implemented infrastructure not yet exercised at runtime.
3. **Boundary Pointer Comparisons & `-O2 -nostdlib` Considerations:**
   `Reset_Handler` compares distinct external symbol addresses (`dst < &_edata`, `dst < &_ebss`). While this compiles and links cleanly on the verified toolchain, representing linker boundaries as `extern uint32_t _sdata[];` (or `uintptr_t` values) avoids strict ISO C cross-object pointer-comparison subtleties. Additionally, if non-empty `.data`/`.bss` sections are introduced later under `-O2` + `-nostdlib`, verify that the compiler does not transform the copy/zero loops into calls to unprovided `memcpy`/`memset` symbols.
4. **Custom Vector Symbol Naming:**
   Several entries in `INTERRUPT_LIST` differ from ST CMSIS symbol spellings—notably `Systick_Handler` (vector `15`, vs. `SysTick_Handler`), `TIM_CC_Handler` (vector `43`, vs. `TIM1_CC_Handler`), `LPUART_Handler`, and `ECC_Handler`. Future ISR implementations must match the exact spellings in `startup/startup_stm32h755_m7.c` to override the weak aliases.
5. **RCC Clock-Enable Pipeline Latency & `ODR` vs. `BSRR`:**
   * `src/main.c` reads `GPIOB_MODER` immediately after setting `RCC_C1_AHB4ENR` without an intervening dummy read-back of `RCC_C1_AHB4ENR` or `__DSB()`. While verified working at reset HSI clocks in both `-O0` and `-O2`, ST RM0399 notes AHB bus-bridge latency after peripheral clock enables—adding a read-back delay is recommended if SYSCLK is raised via PLL.
   * `GPIOx_ODR |=` performs a non-atomic read-modify-write. This is safe in the current single-threaded `nop` loop, whereas write-only `GPIOx_BSRR` (`0x18`) should be used if interrupts or Cortex-M4 later share Port B or Port E.
6. **Checklist for Expanding Beyond Bring-Up:**
   * *High-speed clocks / PLL:* Configure `PWR_CR3` (matching Nucleo-H755ZI-Q LDO/SMPS hardware configuration), Flash wait states (`FLASH_ACR`), and RCC PLLs before raising SYSCLK.
   * *Hardware FPU:* Add `-mfpu=fpv5-d16 -mfloat-abi=hard` (or `softfp`) in `Makefile` and enable `SCB->CPACR` CP10/CP11 in `Reset_Handler` before C code executes.
   * *Performance / Dual-Core:* Enable I/D-Caches (`SCB_EnableICache`/`DCache`), optionally map hot stack/data to DTCM (`0x20000000`), and partition Flash Bank 1/Bank 2 and shared SRAM in `linker/stm32h755_m7.ld` if adding Cortex-M4 firmware.