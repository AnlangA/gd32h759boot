#include "console_usart.h"
#include "log.h"
#include "stdbool.h"
#include <stdio.h>

/**
 * @brief 串口 初始化。
 * 设置接收寄存器非空中断：用于启动接收超时中断。
 * 设置接收超时中断，用于获取本次dma接收的数据长度
 *
 */
void console_uart_init(void)
{
    rcu_periph_clock_enable(CONSOLE_USART_GPIO_CLK_TX);
    rcu_periph_clock_enable(CONSOLE_USART_GPIO_CLK_RX);
    rcu_periph_clock_enable(CONSOLE_USART_CLK);

    gpio_af_set(CONSOLE_USART_TX_GPIO_PORT, CONSOLE_USART_TX_RX_GPIO_AF, CONSOLE_USART_TX_GPIO_PIN);
    gpio_mode_set(CONSOLE_USART_TX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, CONSOLE_USART_TX_GPIO_PIN);
    gpio_output_options_set(CONSOLE_USART_TX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, CONSOLE_USART_TX_GPIO_PIN);

    gpio_af_set(CONSOLE_USART_RX_GPIO_PORT, CONSOLE_USART_TX_RX_GPIO_AF, CONSOLE_USART_RX_GPIO_PIN);
    gpio_mode_set(CONSOLE_USART_RX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, CONSOLE_USART_RX_GPIO_PIN);
    gpio_output_options_set(CONSOLE_USART_RX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, CONSOLE_USART_RX_GPIO_PIN);

    usart_deinit(CONSOLE_USARTX);

    usart_baudrate_set(CONSOLE_USARTX, CONSOLE_USART_BAUDRATE);
    usart_word_length_set(CONSOLE_USARTX, USART_WL_8BIT);
    usart_stop_bit_set(CONSOLE_USARTX, USART_STB_1BIT);
    usart_parity_config(CONSOLE_USARTX, USART_PM_NONE);

    usart_hardware_flow_rts_config(CONSOLE_USARTX, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(CONSOLE_USARTX, USART_CTS_DISABLE);

    usart_receive_config(CONSOLE_USARTX, USART_RECEIVE_DISABLE);
    usart_transmit_config(CONSOLE_USARTX, USART_TRANSMIT_ENABLE);

    usart_interrupt_enable(CONSOLE_USARTX, USART_INT_RBNE);

    nvic_irq_enable(CONSOLE_USART_IRQ, 1U, 0U);

    usart_enable(CONSOLE_USARTX);
}

/**
 * @brief  重定向printf输出到串口
 */
int fputc(int ch, FILE* f)
{
    // 直接输出
    usart_data_transmit(CONSOLE_USARTX, (uint8_t) ch);
    while (RESET == usart_flag_get(CONSOLE_USARTX, USART_FLAG_TBE));
    return ch;
}
/**
 * @brief  重定向scanf输入到串口
 */
int fgetc(FILE* f)
{
    while (RESET == usart_flag_get(CONSOLE_USARTX, USART_FLAG_RBNE));
    return (int) usart_data_receive(CONSOLE_USARTX);
}

/**
 * @brief console初始化
 *
 */
void console_init(void)
{
    console_uart_init();
}
