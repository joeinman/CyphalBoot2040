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
#include <optional>
#include <cstring>
#include <cstdio>
#include <vector>

// -----------------------------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------- D E F I N I T I O N S ---------------------------------------------------------

const uint32_t FLASHKV_BASE = XIP_BASE + (1024 * 88);
const uint32_t FLASHKV_SIZE = 2 * FLASH_SECTOR_SIZE;

const uint32_t XIP_USER_BASE = FLASHKV_BASE + FLASHKV_SIZE;
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
    printf("UPDATE_ME\n"); // Notify server we're ready for update

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
    while (!tud_cdc_connected())
        ;
    sleep_ms(1000);

    // Create a FlashKV object.
    FlashKV::FlashKV flashKV(
        [](uint32_t flashAddress, const uint8_t *data, size_t count) -> bool
        {
            flash_range_program(flashAddress - XIP_BASE, data, count);
            return true;
        },
        [](uint32_t flashAddress, uint8_t *data, size_t count) -> bool
        {
            memcpy(data, reinterpret_cast<uint8_t *>(flashAddress), count);
            return true;
        },
        [](uint32_t flashAddress, size_t count) -> bool
        {
            sleep_ms(100);
            flash_range_erase(flashAddress - XIP_BASE, count);
            return true;
        },
        FLASH_PAGE_SIZE,
        FLASH_SECTOR_SIZE,
        FLASHKV_BASE,
        FLASHKV_SIZE);

    // Load The Store From File
    int loadStatus = flashKV.loadStore();

    // Read Node ID From The Store
    uint8_t nodeID = 0;
    auto nodeIDKey = flashKV.readKey("NODE_ID");
    if (nodeIDKey)
        nodeID = nodeIDKey.value()[0];

    // Read Node Version Major From The Store
    uint8_t nodeVersionMajor = 0;
    auto nodeVersionMajorKey = flashKV.readKey("NODE_VERSION_MAJOR");
    if (nodeVersionMajorKey)
        nodeVersionMajor = nodeVersionMajorKey.value()[0];

    // Read Node Version Minor From The Store
    uint8_t nodeVersionMinor = 0;
    auto nodeVersionMinorKey = flashKV.readKey("NODE_VERSION_MINOR");
    if (nodeVersionMinorKey)
        nodeVersionMinor = nodeVersionMinorKey.value()[0];

    // Read Node Version Patch From The Store
    uint8_t nodeVersionPatch = 0;
    auto nodeVersionPatchKey = flashKV.readKey("NODE_VERSION_PATCH");
    if (nodeVersionPatchKey)
        nodeVersionPatch = nodeVersionPatchKey.value()[0];

    // If Node ID Is 0, We Need To Request One From The Server
    if (nodeID == 0)
    {
        printf("REQUEST_NODE_ID\n");
        flashKV.writeKey("NODE_ID", std::vector<uint8_t>(1, getchar()));

        // Read nodeName from the serial port
        std::string nodeName;
        char ch;
        while ((ch = getchar()) != '\n' && ch != '\r')
            nodeName.push_back(ch);
        flashKV.writeKey("NODE_NAME", std::vector<uint8_t>(nodeName.begin(), nodeName.end()));
    }

    // Request Node Version From The Server
    printf("REQUEST_NODE_VERSION\n");
    uint8_t remoteVersionMajor = getchar();
    uint8_t remoteVersionMinor = getchar();
    uint8_t remoteVersionPatch = getchar();

    if (nodeVersionMajor < remoteVersionMajor ||
        (nodeVersionMajor == remoteVersionMajor && nodeVersionMinor < remoteVersionMinor) ||
        (nodeVersionMajor == remoteVersionMajor && nodeVersionMinor == remoteVersionMinor && nodeVersionPatch < remoteVersionPatch))
    {
        // Request Update
        loadUserApplication();

        // Update Node Version
        flashKV.writeKey("NODE_VERSION_MAJOR", std::vector<uint8_t>(1, remoteVersionMajor));
        flashKV.writeKey("NODE_VERSION_MINOR", std::vector<uint8_t>(1, remoteVersionMinor));
        flashKV.writeKey("NODE_VERSION_PATCH", std::vector<uint8_t>(1, remoteVersionPatch));
        flashKV.saveStore();
    }

    // Start User Applications
    startUserApplication();

    while (1)
    {
        tight_loop_contents();
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
