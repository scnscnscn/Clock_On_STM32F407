/********************************************************************************
 * @file    LED.c
 * @brief   LED与蜂鸣器的GPIO初始化实现（STM32F4xx）
 * @author  WLQVincent@gmail.com
 * @date    2025-12-23
 * @note    1. 先初始化GPIOF（LED）再初始化GPIOC（蜂鸣器），避免参数覆盖；
 *          2. 所有输出引脚默认配置为推挽输出、100MHz速率、无上下拉；
 *          3. 初始化后强制关闭所有LED和蜂鸣器，防止上电误动作。
 ********************************************************************************/
#include "DEVICE.h"

/********************************************************************************
 * @brief   初始化LED和蜂鸣器的GPIO引脚
 * @param   无
 * @retval  无
 * @note    1. LED1~8对应GPIOF0~7，蜂鸣器对应GPIOC13；
 *          2. 需确保RCC时钟使能函数与引脚所属总线匹配（GPIOF/GPIOC均为AHB1总线）。
 ********************************************************************************/
void LEDGpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure; /* GPIO初始化结构体 */

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOC, ENABLE);

    /************************* 第一步：初始化GPIOF（LED1~8） *************************/
    /* 配置GPIOF0~7为推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
                                  GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7; /* LED引脚范围 */
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;                                   /* 输出模式 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;                                   /* 推挽输出（适合驱动LED） */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;                               /* 高速率（100MHz） */
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;                                /* 无上下拉（LED为外部上拉/下拉） */
    GPIO_Init(GPIOF, &GPIO_InitStructure);                                           /* 应用配置到GPIOF */

    /************************* 第二步：初始化GPIOC（蜂鸣器） *************************/
    /* 配置GPIOC13为推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13; /* 蜂鸣器引脚，覆盖原有Pin配置 */
    GPIO_Init(GPIOC, &GPIO_InitStructure); /* 应用配置到GPIOC */

    /************************* 第三步：初始化后默认状态 *************************/
    GPIO_SetBits(GPIOF, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
                            GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);
    BEEP_OFF; 
}