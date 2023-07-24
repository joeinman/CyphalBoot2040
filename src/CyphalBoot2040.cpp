#include <hardware/resets.h>
#include <pico/stdlib.h>
#include <RP2040.h>

static void disableInterrupts()
{
	SysTick->CTRL &= ~1;

	NVIC->ICER[0] = 0xFFFFFFFF;
	NVIC->ICPR[0] = 0xFFFFFFFF;
}

static void resetPeripherals()
{
    reset_block(~(
            RESETS_RESET_IO_QSPI_BITS |
            RESETS_RESET_PADS_QSPI_BITS |
            RESETS_RESET_SYSCFG_BITS |
            RESETS_RESET_PLL_SYS_BITS
    ));
}

static void jumpToVTOR(uint32_t vtor)
{
	// Derived from the Leaf Labs Cortex-M3 bootloader.
	// Copyright (c) 2010 LeafLabs LLC.
	// Modified 2021 Brian Starkey <stark3y@gmail.com>
	// Originally under The MIT License
	uint32_t reset_vector = *(volatile uint32_t *)(vtor + 0x04);

	SCB->VTOR = (volatile uint32_t)(vtor);

	asm volatile("msr msp, %0"::"g"
			(*(volatile uint32_t *)vtor));
	asm volatile("bx %0"::"r" (reset_vector));
}

int main()
{
    // Disable Interrupts & Reset Peripherals
    disableInterrupts();
	resetPeripherals();

    // Jump To Start Of Application
    uint32_t vtor = *((uint32_t *)(XIP_BASE + (12 * 1024)));
	jumpToVTOR(vtor);

    while(1)
    {
        tight_loop_contents();
    }
}