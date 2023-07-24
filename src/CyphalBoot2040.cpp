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

static void startUserApplication(uint32_t applicationAddress)
{
    // Disable Interrupts & Reset Peripherals
    disableInterrupts();
    resetPeripherals();

    // Set Vector Table Offset Register
    uint32_t resetVector = *(volatile uint32_t *)(applicationAddress + 0x04);
    SCB->VTOR = (volatile uint32_t)(applicationAddress);

    // Set Stack Pointer & Jump To Reset Vector
    asm volatile("msr msp, %0"::"g"(*(volatile uint32_t *)applicationAddress));
    asm volatile("bx %0"::"r" (resetVector));
}

int main()
{
    // Jump To Start Of Application
    uint32_t applicationAddress = *((uint32_t *)(XIP_BASE + (12 * 1024)));
    startUserApplication(applicationAddress);

    while(1)
    {
        tight_loop_contents();
    }
}