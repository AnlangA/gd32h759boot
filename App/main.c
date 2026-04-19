#include "gd32h7xx.h"
#include "stdint.h"
#include "console_usart.h"
#include "mcuboot_config/mcuboot_config.h"
#include "mcuboot_config/mcuboot_logging.h"
#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"
#include <bootutil/bootutil.h>
#include <bootutil/image.h>

/* ---------- 1. Address definitions ---------- */
#define FLASH_END       0x083C0000U
#define SRAM_SIZE       0xE0000U         /* 512 KB AXI SRAM */
#define SRAM_END        (SRAM_BASE + SRAM_SIZE)

/* ---------- 2. Function pointer type ---------- */
typedef void (*pFunction)(void);

void check_reset_source(void)
{
    if (rcu_flag_get(RCU_FLAG_BORRST)) {
        MCUBOOT_LOG_SIM("BOR reset");
    }
    if (rcu_flag_get(RCU_FLAG_EPRST)) {
        MCUBOOT_LOG_SIM("External reset");
    }
    if (rcu_flag_get(RCU_FLAG_PORRST)) {
        MCUBOOT_LOG_SIM("Power-on reset");
    }
    if (rcu_flag_get(RCU_FLAG_SWRST)) {
        MCUBOOT_LOG_SIM("Software reset");
    }
    if (rcu_flag_get(RCU_FLAG_FWDGTRST)) {
        MCUBOOT_LOG_SIM("free watchdog timer reset flag");
    }
    if (rcu_flag_get(RCU_FLAG_WWDGTRST)) {
        MCUBOOT_LOG_SIM("Watchdog reset");
    }
    if (rcu_flag_get(RCU_FLAG_LPRST)) {
        MCUBOOT_LOG_SIM("Low-power reset");
    }

    rcu_all_reset_flag_clear();
}

/* ---------- 3. Main function ---------- */
int main(void)
{
    struct boot_rsp rsp;
    fih_ret boot_rc;
    uint32_t app_addr;
    volatile uint32_t app_stack_ptr;
    volatile uint32_t jump_address;
    pFunction JumpToApp;

    __disable_irq();

    console_init();

    check_reset_source();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    /* Let MCUBoot validate and select the boot image.
     * boot_go() handles:
     *   - Checking secondary slot for upgrade candidates
     *   - Copying new image to primary (overwrite mode)
     *   - Verifying ECDSA-P256 image signature
     *   - Returning the selected image location
     */
    boot_rc = boot_go(&rsp);
    if (boot_rc != 0) {
        MCUBOOT_LOG_ERR("boot_go failed: no bootable image");
        while (1);
    }

    /* Compute application base address.
     * br_image_off is the flash offset of the slot (from flash base).
     * The MCUBoot image header sits at the start of the slot;
     * the application vector table follows at ih_hdr_size bytes in.
     */
    app_addr = FLASH_BASE_ADDR + rsp.br_image_off;
    if (rsp.br_hdr != NULL && rsp.br_hdr->ih_hdr_size != 0) {
        app_addr += rsp.br_hdr->ih_hdr_size;
    } else {
        app_addr += 32U;  /* default IMAGE_HEADER_SIZE */
    }

    MCUBOOT_LOG_INF("Booting image at 0x%08lX", (unsigned long)app_addr);

    /* Disable caches before reading vector table */
    SCB_DisableICache();
    SCB_DisableDCache();

    app_stack_ptr = *(__IO uint32_t *)app_addr;
    jump_address  = *(__IO uint32_t *)(app_addr + 4);

    /* Validate SP and reset handler */
    if ((app_stack_ptr >= SRAM_BASE && app_stack_ptr < SRAM_END) &&
        (jump_address >= app_addr && jump_address < FLASH_END) &&
        (jump_address & 1))
    {
        SysTick->CTRL = 0;
        SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

        SCB->VTOR = app_addr & 0x1FFFFF80U;
        __set_MSP(app_stack_ptr);

        jump_address = *(__IO uint32_t *)(app_addr + 4);
        JumpToApp = (pFunction)(jump_address);
        JumpToApp();
    }
    else
    {
        MCUBOOT_LOG_ERR("Invalid image at 0x%08lX (SP=0x%08lX PC=0x%08lX)",
                        (unsigned long)app_addr,
                        (unsigned long)app_stack_ptr,
                        (unsigned long)jump_address);
        while (1);
    }
}
