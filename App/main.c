#include "gd32h7xx.h"
#include "stdint.h"

/* ---------- 1. 地址定义 ---------- */
#define APP_ADDRESS     0x08020000U
#define FLASH_END       0x083C0000U      // GD32H759 最大 Flash
#define SRAM_SIZE       0xE0000U         // 512 KB AXI SRAM
#define SRAM_END        (SRAM_BASE + SRAM_SIZE)

/* ---------- 2. 函数指针类型 ---------- */
typedef void (*pFunction)(void);

/* ---------- 3. 主函数 ---------- */
int main(void)
{
    volatile uint32_t app_stack_ptr;
    volatile uint32_t jump_address;
    pFunction JumpToApp;

    __disable_irq();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    /* 1. 先关闭缓存 */
    SCB_DisableICache();
    SCB_DisableDCache();

    /* 2. 再读 Flash（volatile 强制读内存） */
    app_stack_ptr = *(__IO uint32_t *)APP_ADDRESS;
    jump_address  = *(__IO uint32_t *)(APP_ADDRESS + 4);

    /* 3. 有效性检查 */
    if ((app_stack_ptr >= SRAM_BASE && app_stack_ptr < SRAM_END) &&
        (jump_address >= APP_ADDRESS && jump_address < FLASH_END) &&
        (jump_address & 1))  // 必须是 Thumb 地址（奇数）
    {
        SysTick->CTRL = 0;
        SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

        SCB->VTOR = APP_ADDRESS & 0x1FFFFF80U;
        __set_MSP(app_stack_ptr);

				jump_address  = *(__IO uint32_t *)(APP_ADDRESS + 4);
        /* 跳转：清除 LSB */
        JumpToApp = (pFunction)(jump_address);
        JumpToApp();
    }
    else
    {
        while (1);  // App 无效
    }
}