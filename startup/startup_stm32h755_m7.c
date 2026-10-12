/**
*  =================================================================================
*   @file      startup_stm32h755_m7.c
*   @brief     Cortex-M7 Vector Table and C-Runtime Environment Initialization.
*
*   @details   Establishes the early boot execution environment for the 
*               ARM Cortex-M7  (STM32 NUCLEO H755ZI-Q). Constructs the
*               hardware exception/interrupt vector table, initializes .data and 
*               .bss RAM blocks and branches control flow to the application main
*               entry point.
*  ================================================================================= 
*/

#include <stdint.h>


/* =================================================================================
*   EXTERNAL LINKS AND TYPE DEFINITIONS
*  ================================================================================= */

/*External variables tracking memory boundaries exported from linker script*/
extern uint32_t _stack_top;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;


extern int main(void);

/*Function pointer type definition*/
typedef void (*ISR_Handler_t)(void);


/* =================================================================================
*   C RUNTIME INITIALIZATION (RESET HANDLER DEFINITION)
*  ================================================================================= */

void Reset_Handler(void) 
{

    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    /*Copy of initialized global/static variables from FLASH(LMA) to AXI_RAM(VMA)*/
    while (dst < &_edata)
    {
        *dst++ = *src++;
    }

    dst = &_sbss;
    
    /*Zeroing off uninitialized global/static variables in AXI_RAM*/
    while (dst < &_ebss)
    {
        *dst++ = 0U;
    }

    /*Branching into main function (Application)*/
    (void)main();

    /*Safeguard in the event that the main function should return*/
    for (;;)
    {
        __asm volatile ("nop");
    }

}


/* =================================================================================
*   DEFAULT HANDLER DEFINITION
*  ================================================================================= */

void Default_Handler(void)
{
    __asm volatile ("bkpt #0");

    for (;;)
    {
        __asm volatile ("nop");
    }
}



/*  =================================================================================
*       EXCEPTION / INTERRUPT VECTOR MATRIX (X-MACRO LAYOUT)
*   =================================================================================
*   This structure is an X-Macro: an advanced preprocessor code-generation technique
*   that maintains all interrupt metadata in a single, centralized block.
*   
*   This layout automates three repetitive task across the system:
*       1-2. Generates function forward declarations for exception/interrupt handlers 
*          with weak aliases to Default_Handler.
*       3. Populates the final hardware vector table array in memory.
*   =================================================================================
*/

#define INTERRUPT_LIST(H, R) \
    H(2,   NMI_Handler)                            \
    H(3,   HardFault_Handler)                      \
    H(4,   MemManage_Handler)                      \
    H(5,   BusFault_Handler)                       \
    H(6,   UsageFault_Handler)                     \
    R(7)                                           \
    R(8)                                           \
    R(9)                                           \
    R(10)                                          \
    H(11,  SVCall_Handler)                         \
    H(12,  DebugMonitor_Handler)                   \
    R(13)                                          \
    H(14,  PendSV_Handler)                         \
    H(15,  Systick_Handler)                        \
    H(16,  WWDG1_Handler)                          \
    H(17,  PVD_PVM_Handler)                        \
    H(18,  RTC_TAMP_STAMP_CSS_LSE_Handler)         \
    H(19,  RTC_WKUP_Handler)                       \
    H(20,  FLASH_Handler)                          \
    H(21,  RCC_Handler)                            \
    H(22,  EXTI0_Handler)                          \
    H(23,  EXTI1_Handler)                          \
    H(24,  EXTI2_Handler)                          \
    H(25,  EXTI3_Handler)                          \
    H(26,  EXTI4_Handler)                          \
    H(27,  DMA1_STR0_Handler)                      \
    H(28,  DMA1_STR1_Handler)                      \
    H(29,  DMA1_STR2_Handler)                      \
    H(30,  DMA1_STR3_Handler)                      \
    H(31,  DMA1_STR4_Handler)                      \
    H(32,  DMA1_STR5_Handler)                      \
    H(33,  DMA1_STR6_Handler)                      \
    H(34,  ADC1_2_Handler)                         \
    H(35,  FDCAN1_IT0_Handler)                     \
    H(36,  FDCAN2_IT0_Handler)                     \
    H(37,  FDCAN1_IT1_Handler)                     \
    H(38,  FDCAN2_IT1_Handler)                     \
    H(39,  EXTI9_5_Handler)                        \
    H(40,  TIM1_BRK_Handler)                       \
    H(41,  TIM1_UP_Handler)                        \
    H(42,  TIM1_TRG_COM_Handler)                   \
    H(43,  TIM_CC_Handler)                         \
    H(44,  TIM2_Handler)                           \
    H(45,  TIM3_Handler)                           \
    H(46,  TIM4_Handler)                           \
    H(47,  I2C1_EV_Handler)                        \
    H(48,  I2C1_ER_Handler)                        \
    H(49,  I2C2_EV_Handler)                        \
    H(50,  I2C2_ER_Handler)                        \
    H(51,  SPI1_Handler)                           \
    H(52,  SPI2_Handler)                           \
    H(53,  USART1_Handler)                         \
    H(54,  USART2_Handler)                         \
    H(55,  USART3_Handler)                         \
    H(56,  EXTI15_10_Handler)                      \
    H(57,  RTC_ALARM_Handler)                      \
    R(58)                                          \
    H(59,  TIM8_BRK_TIM12_Handler)                 \
    H(60,  TIM8_UP_TIM13_Handler)                  \
    H(61,  TIM8_TRG_COM_TIM14_Handler)             \
    H(62,  TIM8_CC_Handler)                        \
    H(63,  DMA1_STR7_Handler)                      \
    H(64,  FMC_Handler)                            \
    H(65,  SDMMC1_Handler)                         \
    H(66,  TIM5_Handler)                           \
    H(67,  SPI3_Handler)                           \
    H(68,  UART4_Handler)                          \
    H(69,  UART5_Handler)                          \
    H(70,  TIM6_DAC_Handler)                       \
    H(71,  TIM7_Handler)                           \
    H(72,  DMA2_STR0_Handler)                      \
    H(73,  DMA2_STR1_Handler)                      \
    H(74,  DMA2_STR2_Handler)                      \
    H(75,  DMA2_STR3_Handler)                      \
    H(76,  DMA2_STR4_Handler)                      \
    H(77,  ETH_Handler)                            \
    H(78,  ETH_WKUP_Handler)                       \
    H(79,  FDCAN_CAL_Handler)                      \
    R(80)                                          \
    R(81)                                          \
    R(82)                                          \
    R(83)                                          \
    H(84,  DMA2_STR5_Handler)                      \
    H(85,  DMA2_STR6_Handler)                      \
    H(86,  DMA2_STR7_Handler)                      \
    H(87,  USART6_Handler)                         \
    H(88,  I2C3_EV_Handler)                        \
    H(89,  I2C3_ER_Handler)                        \
    H(90,  OTG_HS_EP1_OUT_Handler)                 \
    H(91,  OTG_HS_EP1_IN_Handler)                  \
    H(92,  OTG_HS_WKUP_Handler)                    \
    H(93,  OTG_HS_Handler)                         \
    H(94,  DCMI_Handler)                           \
    H(95,  CRYP_Handler)                           \
    H(96,  HASH_RNG_Handler)                       \
    H(97,  FPU_Handler)                            \
    H(98,  UART7_Handler)                          \
    H(99,  UART8_Handler)                          \
    H(100, SPI4_Handler)                           \
    H(101, SPI5_Handler)                           \
    H(102, SPI6_Handler)                           \
    H(103, SAI1_Handler)                           \
    H(104, LTDC_Handler)                           \
    H(105, LTDC_ER_Handler)                        \
    H(106, DMA2D_Handler)                          \
    H(107, SAI2_Handler)                           \
    H(108, QUADSPI_Handler)                        \
    H(109, LPTIM1_Handler)                         \
    H(110, CEC_Handler)                            \
    H(111, I2C4_EV_Handler)                        \
    H(112, I2C4_ER_Handler)                        \
    H(113, SPDIF_Handler)                          \
    H(114, OTG_FS_EP1_OUT_Handler)                 \
    H(115, OTG_FS_EP1_IN_Handler)                  \
    H(116, OTG_FS_WKUP_Handler)                    \
    H(117, OTG_FS_Handler)                         \
    H(118, DMAMUX1_OV_Handler)                     \
    H(119, HRTIM1_MST_Handler)                     \
    H(120, HRTIM1_TIMA_Handler)                    \
    H(121, HRTIM1_TIMB_Handler)                    \
    H(122, HRTIM1_TIMC_Handler)                    \
    H(123, HRTIM1_TIMD_Handler)                    \
    H(124, HRTIM1_TIME_Handler)                    \
    H(125, HRTIM1_FLT_Handler)                     \
    H(126, DFSDM1_FLT0_Handler)                    \
    H(127, DFSDM1_FLT1_Handler)                    \
    H(128, DFSDM1_FLT2_Handler)                    \
    H(129, DFSDM1_FLT3_Handler)                    \
    H(130, SAI3_Handler)                           \
    H(131, SWPMI1_Handler)                         \
    H(132, TIM15_Handler)                          \
    H(133, TIM16_Handler)                          \
    H(134, TIM17_Handler)                          \
    H(135, MDIOS_WKUP_Handler)                     \
    H(136, MDIOS_Handler)                          \
    H(137, JPEG_Handler)                           \
    H(138, MDMA_Handler)                           \
    H(139, DSI_Handler)                            \
    H(140, SDMMC2_Handler)                         \
    H(141, HSEM0_Handler)                          \
    R(142)                                         \
    H(143, ADC3_Handler)                           \
    H(144, DMAMUX2_OVR_Handler)                    \
    H(145, BDMA_CH0_Handler)                       \
    H(146, BDMA_CH1_Handler)                       \
    H(147, BDMA_CH2_Handler)                       \
    H(148, BDMA_CH3_Handler)                       \
    H(149, BDMA_CH4_Handler)                       \
    H(150, BDMA_CH5_Handler)                       \
    H(151, BDMA_CH6_Handler)                       \
    H(152, BDMA_CH7_Handler)                       \
    H(153, COMP_Handler)                           \
    H(154, LPTIM2_Handler)                         \
    H(155, LPTIM3_Handler)                         \
    H(156, LPTIM4_Handler)                         \
    H(157, LPTIM5_Handler)                         \
    H(158, LPUART_Handler)                         \
    H(159, WWDG2_RST_Handler)                      \
    H(160, CRS_Handler)                            \
    H(161, ECC_Handler)                            \
    H(162, SAI4_Handler)                           \
    R(163)                                         \
    H(164, HOLD_CORE_Handler)          \
    H(165, WKUP_Handler)


/* =================================================================================
*   FORWARD DECLARATION OF EXCEPTION AND INTERRUPT HANDLERS
*  ================================================================================= */

/*Defining H (declare it) and R (ignore it)*/
#define DECLARE_HANDLER_PROTOTYPE(idx, name) void name(void) __attribute__((weak, alias("Default_Handler")));
#define IGNORE_RESERVED(idx)

/*Declaration of prototypes of exception / interrupt handlers*/
INTERRUPT_LIST(DECLARE_HANDLER_PROTOTYPE, IGNORE_RESERVED)



/* =================================================================================
*   VECTOR TABLE ARRAY
*  ================================================================================= */

/*Repurposing H and R to generate the Vector table (The arguments of INTERRUPT_LIST MACRO)*/
#define AS_VECTOR(idx, name) [idx] = name,
#define AS_RESERVED(idx) [idx] = 0,

/*The Vector table*/
__attribute__((section(".isr_vector"), used))
const ISR_Handler_t g_pfnVectors[] =
{
    /* The Cortex-M architecture requires the initial mainstack pointer at offset 0 and the Reset Vector at offset 1 */
    [0] = (ISR_Handler_t)(uintptr_t)&_stack_top,
    [1] = Reset_Handler,

    /*Remaining Exceptions/Handlers are generated below, completeing the vector table*/
    INTERRUPT_LIST(AS_VECTOR, AS_RESERVED)
};