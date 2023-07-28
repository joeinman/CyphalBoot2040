// ------------------------------------------------------------ I N C L U D E S ------------------------------------------------------------

// Pico SDK
#include <hardware/resets.h>
#include <hardware/flash.h>
#include <pico/stdlib.h>
#include <RP2040.h>

// CRC32C Library
#include <CRC32/CRC32.h>

// Standard Library
#include <cstring>
#include <cstdio>

// -----------------------------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------- D E F I N I T I O N S ---------------------------------------------------------

const uint32_t XIP_USER_BASE = XIP_BASE + (12 * 1024);
const uint32_t PAGE_SIZE = 256;

// -----------------------------------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------- H E L P E R    F U N C T I O N S ---------------------------------------------------

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
        RESETS_RESET_PLL_SYS_BITS));
}

static void startUserApplication()
{
    // Disable Interrupts & Reset Peripherals
    disableInterrupts();
    resetPeripherals();

    // Set Vector Table Offset Register
    uint32_t resetVector = *(volatile uint32_t *)(XIP_USER_BASE + 0x04);
    SCB->VTOR = (volatile uint32_t)(XIP_USER_BASE);

    // Set Stack Pointer & Jump To Reset Vector
    asm volatile("msr msp, %0" ::"g"(*(volatile uint32_t *)XIP_USER_BASE));
    asm volatile("bx %0" ::"r"(resetVector));
}

static void loadUserApplication()
{
    printf("UPDATEME\n"); // Notify server we're ready for update

    uint32_t currentAddress = XIP_USER_BASE;

    while (1)
    {
        uint8_t data[PAGE_SIZE] = {0};
        uint32_t remoteChecksum = 0;

        // Read page data and checksum from USB
        for (int i = 0; i < PAGE_SIZE; i++)
        {
            data[i] = getchar();
        }
        fread(&remoteChecksum, sizeof(remoteChecksum), 1, stdin);

        // Check for EOF message
        if (memcmp(data, "EOF", 3) == 0)
        {
            printf("DONE\n"); // Notify server that update is complete
            break;
        }

        uint32_t localChecksum = CRC32::crc32c(data, sizeof(data));

        if (remoteChecksum == localChecksum)
        {
            flash_range_program(currentAddress - XIP_BASE, data, PAGE_SIZE);
            currentAddress += PAGE_SIZE;

            printf("GOOD\n"); // Notify server page was received correctly
        }
        else
        {
            printf("BAD\n"); // Notify server to resend page
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------- M A I N    L O O P -----------------------------------------------------------

int main()
{
    stdio_init_all();

    // Load User Application
    loadUserApplication();

    // Start User Application
    startUserApplication();

    while (1)
    {
        tight_loop_contents();
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
