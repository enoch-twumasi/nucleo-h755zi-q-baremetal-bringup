#include <stdint.h>

/* =================================================================================
*   REGISTERS DEFINITIONS
*  ================================================================================= */

/* Base addresses */
#define GPIOB_BASE 0x58020400
#define GPIOE_BASE 0x58021000
#define RCC_BASE 0x58024400

/* Offsets */
#define RCC_C1_AHB4ENR_OFFSET 0x140
#define GPIOx_MODER_OFFSET 0x00
#define GPIOx_ODR_OFFSET 0x14


/* Registers */
#define RCC_C1_AHB4ENR (*(volatile uint32_t *)(RCC_BASE + RCC_C1_AHB4ENR_OFFSET)) /**< Core 1 AHB4 peripheral clock enable register*/

#define GPIOB_MODER (*(volatile uint32_t *)(GPIOB_BASE + GPIOx_MODER_OFFSET)) /**< GPIO Port B Mode Register*/
#define GPIOB_ODR	(*(volatile uint32_t *)(GPIOB_BASE + GPIOx_ODR_OFFSET)) /**< GPIO Port B Output Data Register*/

#define GPIOE_MODER (*(volatile uint32_t *)(GPIOE_BASE + GPIOx_MODER_OFFSET)) /**<GPIO Port E Mode Register*/
#define GPIOE_ODR (*(volatile uint32_t *)(GPIOE_BASE + GPIOx_ODR_OFFSET)) /**<GPIO Port E Output Data Register*/

/* Bit Masks to enable GPIO Ports */
#define GPIOBEN (1U<<1) /**< GPIO Port B enabling bit mask (bit 1)*/
#define GPIOEEN (1U<<4) /**< GPIO Port E enabling bit mask (bit 4) */


/* Bit Masks to Clear/Set GPIO Ports Mode in GPIOx_MODER */
#define GPIOB_MODER_MODER0_0 (1U<<0)
#define GPIOB_MODER_MODER0_1 (1U<<1)

#define GPIOB_MODER_MODER14_0 (1U<<28)
#define GPIOB_MODER_MODER14_1 (1U<<29)

#define GPIOE_MODER_MODER1_0 (1U<<2)
#define GPIOE_MODER_MODER1_1 (1U<<3)


/* Bit Masks to Set GPIO Pins Output Data bit High (1) or Low (0) in GPIOx_ODR */
#define GPIOB_ODR_ODR0 (1U<<0)
#define GPIOB_ODR_ODR14 (1U<<14)
#define GPIOE_ODR_ODR1 (1U<<1)



/* =================================================================================
*   MAIN FUNCTION
*  ================================================================================= */
int main (void) 
{

	/* Enable Clock access to GPIO Ports B and E */
	RCC_C1_AHB4ENR |= (GPIOBEN | GPIOEEN);

	/* Clear and Set GPIO Port B pins (PB0 & PB14) and Port E Pin (PE1) Mode to General Purpose Output Mode (01) */
	GPIOB_MODER &= ~(GPIOB_MODER_MODER0_0 | GPIOB_MODER_MODER0_1 | GPIOB_MODER_MODER14_0 | GPIOB_MODER_MODER14_1); /**< Ensure bits reset(default) values of the bits to be set GPIOB_MODER are cleared before we set */
	GPIOB_MODER |= (GPIOB_MODER_MODER0_0 | GPIOB_MODER_MODER14_0);

	GPIOE_MODER &= ~ (GPIOE_MODER_MODER1_0 | GPIOE_MODER_MODER1_1);
	GPIOE_MODER |= GPIOE_MODER_MODER1_0;

	/* Set GPIO Port B pins (PB0, PB14) and Port E Pin (PE1) Output Data bit to high (1)  in GPIOx_ODR */
	GPIOB_ODR |= (GPIOB_ODR_ODR0 | GPIOB_ODR_ODR14); /**< PB0 and PB14 remains high so we keep outside the loop to set once **/
	GPIOE_ODR |= GPIOE_ODR_ODR1; /**< PE1 also remains high so we keep outside the loop to set once **/


	/* Primary background execution loop*/
	while(1) 
	{
		__asm volatile ("nop");
	}
}
