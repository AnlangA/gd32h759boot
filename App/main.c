#include "main.h"
#include "gd32h7xx.h"


void cache_enable(void)
{
    /* enable I-Cache */
    SCB_EnableICache();
    /* enable D-Cache */
    SCB_EnableDCache();
}

int main(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    SCB_EnableICache();     // 使能 I-Cache
    SCB_EnableDCache();     // 使能 D-Cache

    //cache_enable();

    while (1);
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
