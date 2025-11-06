#include "main.h"
#include "gd32h7xx.h"
#include <stdint.h>
#include <stdbool.h>


void cache_enable(void)
{
    /* enable I-Cache */
    SCB_EnableICache();
    /* enable D-Cache */
    SCB_EnableDCache();
}

void system_reset(void)
{
    /* 禁用全局中断 */
    __disable_irq();
    
    /* 复位外设 */
    rcu_deinit();
    
    /* 系统复位 */
    NVIC_SystemReset();
}

int main(void)
{
    /* 定义应用程序地址 */
    #define APP_ADDRESS     0x08020000
    
    /* 定义中断向量表大小 */
    #define VECTOR_SIZE     0x100
    
    /* 检查地址是否在有效RAM区域的函数 */
    bool is_valid_ram_address(uint32_t addr)
    {
        /* 检查SRAM0区域 (512KB) */
        if ((addr >= 0x20000000) && (addr < 0x20080000))
            return true;
        
        /* 检查SRAM1区域 (512KB) */
        if ((addr >= 0x20080000) && (addr < 0x20100000))
            return true;
        
        /* 检查ITCM区域 (1024KB) */
        if ((addr >= 0x24000000) && (addr < 0x24100000))
            return true;
        
        /* 检查DTCM区域 (512KB) */
        if ((addr >= 0x30000000) && (addr < 0x30080000))
            return true;
        
        return false;
    }
    
    
    
    /* 检查地址是否在有效RAM区域的函数 */
    bool is_valid_ram_address(uint32_t addr)
    {
        /* 检查SRAM0区域 */
        if (addr >= SRAM0_BASE && addr < (SRAM0_BASE + SRAM0_SIZE))
            return true;
        /* 检查SRAM1区域 */
        if (addr >= SRAM1_BASE && addr < (SRAM1_BASE + SRAM1_SIZE))
            return true;
        /* 检查ITCM区域 */
        if (addr >= ITCM_BASE && addr < (ITCM_BASE + ITCM_SIZE))
            return true;
        /* 检查DTCM区域 */
        if (addr >= DTCM_BASE && addr < (DTCM_BASE + DTCM_SIZE))
            return true;
        
        return false;
    }
    
    /* 函数指针定义 */
    typedef void (*app_func)(void);
    uint32_t jump_address;
    app_func jump_to_app;
    uint32_t stack_ptr;
    
    /* 禁用全局中断 */
    __disable_irq();
    
    /* 关闭缓存 */
    SCB_DisableICache();
    SCB_DisableDCache();
    
    /* 清除缓存 */
    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();
    
    /* 设置NVIC优先级组 */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    
    /* 使能缓存 */
    SCB_EnableICache();     // 使能 I-Cache
    SCB_EnableDCache();     // 使能 D-Cache

    /* 获取应用程序栈指针 */
    stack_ptr = *(uint32_t*)APP_ADDRESS;
    
    /* 检查栈顶地址是否有效 - 检查应用程序是否已烧录 */
    if (is_valid_ram_address(stack_ptr))
    {
        /* 获取应用程序复位向量 */
        jump_address = *(uint32_t*)(APP_ADDRESS + 4);
        jump_to_app = (app_func)jump_address;
        
        /* 初始化应用程序的栈指针 */
        __set_MSP(*(uint32_t*)APP_ADDRESS);
        
        /* 设置主堆栈指针 */
        __set_MSP(stack_ptr);
        
        /* 设置进程堆栈指针 */
        __set_PSP(stack_ptr);
        
        /* 刷新缓存 */
        SCB_CleanInvalidateDCache();
        SCB_InvalidateICache();
        
        /* 设置中断向量表偏移 */
        SCB->VTOR = APP_ADDRESS;
        
        /* 使能全局中断 */
        __enable_irq();
        
        /* 跳转到应用程序 */
        jump_to_app();
    }
    
    /* 如果没有有效的应用程序，停留在bootloader */
    while (1)
    {
        /* 可以在这里添加LED闪烁等指示代码 */
        /* 延时一段时间后尝试重新检查 */
        for (uint32_t i = 0; i < 1000000; i++);
        
        /* 重新检查应用程序 */
        if ((*(uint32_t*)APP_ADDRESS & STACK_MASK) == RAM_BASE)
        {
            system_reset();
        }
    }
}

#if (configCHECK_FOR_STACK_OVERFLOW)
/**
 * @brief 栈溢出钩子函数。输出栈溢出任务信息。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    for (;;)
    {
        /* 可以在这里添加LED闪烁等指示代码 */
    }
}
#endif
