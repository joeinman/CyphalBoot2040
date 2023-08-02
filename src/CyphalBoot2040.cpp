// ------------------------------------------------------------ I N C L U D E S ------------------------------------------------------------

// Pico SDK
#include <hardware/resets.h>
#include <hardware/flash.h>
#include <pico/stdlib.h>
#include <RP2040.h>
#include <tusb.h>

// My Libraries
#include <FlashKV/FlashKV.h>
#include <CRC32/CRC32.h>

// Standard Library
#include <cstring>
#include <cstdio>

// -----------------------------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------- D E F I N I T I O N S ---------------------------------------------------------

const uint32_t CYPHAL_BOOT_SIZE = 28 * 1024;
const uint32_t FLASHKV_SIZE = 2 * FLASH_SECTOR_SIZE;

const uint32_t XIP_USER_BASE = XIP_BASE + CYPHAL_BOOT_SIZE + FLASHKV_SIZE;
const uint32_t PAGE_SIZE = 256;

// -----------------------------------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------- H E L P E R    F U N C T I O N S ---------------------------------------------------

static void disableInterrupts()
{
    SysTick->CTRL &= ~1;

    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;

    NVIC->ICER[1] = 0xFFFFFFFF;
    NVIC->ICPR[1] = 0xFFFFFFFF;
}

static void resetPeripherals()
{
    reset_block(~(
        RESETS_RESET_IO_QSPI_BITS |
        RESETS_RESET_PADS_QSPI_BITS |
        RESETS_RESET_SYSCFG_BITS |
        RESETS_RESET_PLL_SYS_BITS |
        RESETS_RESET_USBCTRL_BITS |
        RESETS_RESET_DMA_BITS |
        RESETS_RESET_RTC_BITS |
        RESETS_RESET_I2C0_BITS |
        RESETS_RESET_I2C1_BITS |
        RESETS_RESET_SPI0_BITS |
        RESETS_RESET_SPI1_BITS |
        RESETS_RESET_UART0_BITS |
        RESETS_RESET_UART1_BITS |
        RESETS_RESET_ADC_BITS |
        RESETS_RESET_PWM_BITS));
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
        uint8_t data[FLASH_SECTOR_SIZE] = {0};
        uint32_t remoteChecksum = 0;

        // Read sector data and checksum from USB
        for (int i = 0; i < FLASH_SECTOR_SIZE; i++)
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
            // Erase the sector and write the data to flash
            flash_range_erase(currentAddress - XIP_BASE, FLASH_SECTOR_SIZE);
            flash_range_program(currentAddress - XIP_BASE, data, FLASH_SECTOR_SIZE);
            currentAddress += FLASH_SECTOR_SIZE;

            printf("GOOD\n"); // Notify server sector was received correctly
        }
        else
        {
            printf("BAD\n"); // Notify server to resend sector
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------

// ---------------------------------------------------------- M A I N    L O O P -----------------------------------------------------------

int main()
{
    stdio_init_all();
    while (!tud_cdc_connected());
    sleep_ms(1000);

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
