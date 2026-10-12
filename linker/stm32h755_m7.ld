/* ================================================================================
*   TARGET ARCHITECTURE : ARM Cortex-M7 (STM32H755 Core 1)
*   BOARD: STM32 NUCLEO H755ZI-Q
*
*   DESCRIPTION: Linker Script defining memory regions and sections
*       and exposing specific address boundaries as symbols that the
*       startup code will rely on to initialize memory
*  ================================================================================= */




/* =================================================================================
*   ENTRY POINT AND GLOBAL CONSTANTS
*  ================================================================================= */

ENTRY(Reset_Handler)

/* 8 KB minimum stack size (Global constant) */
_Min_Stack_Size = 0x2000;



/* =================================================================================
*   MEMORY REGION DEFINITIONS
*  ================================================================================= */

MEMORY
{
   /* 2MB Main Flash and 512KB AXI SRAM */
   FLASH (rx) : ORIGIN = 0x08000000 , LENGTH = 2048K
   AXI_RAM (rwx):  ORIGIN = 0x24000000 , LENGTH = 512K
}


/* =================================================================================
*   OUTPUT SECTIONS
*  ================================================================================= */

SECTIONS
{
    /* Cortex-M7 VTOR requires the vector table base address to be aligned 
       to a power of two equal to or greater than the total table size. */
    .isr_vector : ALIGN(1024)
    {
        . = ALIGN(4);

        KEEP(*(.isr_vector))

        . = ALIGN(4);

    } > FLASH


    .text : ALIGN(4)
    {
        . = ALIGN(4);

        *(.text)
        *(.text.*)

        . = ALIGN(4);
    } > FLASH


    .rodata : ALIGN(4)
    {
        . = ALIGN(4);
        
        *(.rodata)
        *(.rodata.*)

        . = ALIGN(4);
    } >FLASH


    /* 
    * Required for compiler-generated stack unwinding data.
    * Retained here to guarantee framework robustness and forward-compatibility
    * for users pulling in standard C runtime diagnostics or custom assertions.
    */
    .ARM.extab : ALIGN(4)
    {
        *(ARM.extab*)
    } > FLASH

    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;

        *(ARM.exidx*)

        __exidx_end = .;
    } > FLASH

    . = ALIGN(4);


    /* 
     *Initialized data mapped to AXI_RAM (VMA) but stored in FLASH (LMA).
     * The startup code copies this section using _sdata, _edata, and _sidata.
     * Retained to support initialized global/static variables when users expand this bare-metal application.
     */
    
    .data : ALIGN(4)
    {
        . = ALIGN(4);

        _sdata = .;

        *(.data)
        *(.data.*)

        . = ALIGN(4);

        _edata = .;
        
    } > AXI_RAM AT > FLASH


    _sidata = LOADADDR(.data);

    /* Zero-initialized global/static variables segment.
     * Retained to support zero-initialized glbal/static variables when users expand this bare-metal application. */
    .bss : ALIGN(4)
    {
        . = ALIGN(4);

        _sbss = .;

        *(.bss)
        *(.bss.*)

        . = ALIGN(4);
        _ebss = .;

    } > AXI_RAM




    /* Enforcing 8-byte alignment to ensure optimal CPU memory access cycles for the heap */
    . = ALIGN(8);

    /* Global symbol(tracker) marking the end of Static segments/ and baseline for runtime allocations */
    _end = .;

    /* Reserve uninitialized space at the top of AXI_RAM for the main stack. */
    .stack ORIGIN(AXI_RAM) + LENGTH(AXI_RAM) - _Min_Stack_Size (NOLOAD) :
    {
        . = ALIGN(8);
        _stack_limit = .;

        . += _Min_Stack_Size;

        . = ALIGN(8);

        _stack_top = .;
    } > AXI_RAM

    /* Ensure statically allocated data does not grow into the reserved stack space. */
    ASSERT(_end <= _stack_limit, "ERROR: RAM allocation overlaps the reserved stack")
    ASSERT(SIZEOF(.isr_vector) <= 1024, "ERROR: Vector table exceeds its 1024-byte alignment block")
    
    /* Historical compatibility alias required by standard C library allocators
     * to know where Heap begins */
    PROVIDE(end = _end);

}