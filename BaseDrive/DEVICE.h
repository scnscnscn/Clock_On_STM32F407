#ifndef _DEVICE_H_
#define _DEVICE_H_

/********************************************************************************
 * @file    DEVICE.h
 * @brief   LED与蜂鸣器的硬件驱动头文件（STM32F4xx）
 * @author  WLQVincent@gmail.com
 * @date    2025-12-23
 * @note    1. LED1~8对应GPIOF0~F7，低电平点亮；
 *          2. 蜂鸣器对应GPIOC13，低电平触发；
 *          3. 所有外设采用推挽输出模式，无上下拉配置。
 ********************************************************************************/
#include "stm32f4xx.h" // 包含STM32核心头文件，减少外部依赖

/* 硬件资源引脚定义 - 与硬件原理图严格对应 */
// LED1~8 对应 GPIOF0~F7（共阴极，低电平点亮）
// 蜂鸣器BEEP 对应 GPIOC13（有源蜂鸣器，低电平发声）

/************************* LED操作宏定义 *************************/
// LED1 (PF0) 操作宏
#define LED1_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_0)                                                       /* 点亮LED1（低电平） */
#define LED1_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_0)                                                         /* 关闭LED1（高电平） */
#define LED1_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_0, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_0)) /* 翻转LED1状态 */

// LED2 (PF1) 操作宏
#define LED2_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_1)                                                       /* 点亮LED2（低电平） */
#define LED2_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_1)                                                         /* 关闭LED2（高电平） */
#define LED2_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_1, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_1)) /* 翻转LED2状态 */

// LED3 (PF2) 操作宏
#define LED3_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_2)                                                       /* 点亮LED3（低电平） */
#define LED3_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_2)                                                         /* 关闭LED3（高电平） */
#define LED3_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_2, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_2)) /* 翻转LED3状态 */

// LED4 (PF3) 操作宏
#define LED4_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_3)                                                       /* 点亮LED4（低电平） */
#define LED4_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_3)                                                         /* 关闭LED4（高电平） */
#define LED4_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_3, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_3)) /* 翻转LED4状态 */

// LED5 (PF4) 操作宏
#define LED5_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_4)                                                       /* 点亮LED5（低电平） */
#define LED5_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_4)                                                         /* 关闭LED5（高电平） */
#define LED5_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_4, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_4)) /* 翻转LED5状态 */

// LED6 (PF5) 操作宏
#define LED6_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_5)                                                       /* 点亮LED6（低电平） */
#define LED6_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_5)                                                         /* 关闭LED6（高电平） */
#define LED6_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_5, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_5)) /* 翻转LED6状态 */

// LED7 (PF6) 操作宏
#define LED7_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_6)                                                       /* 点亮LED7（低电平） */
#define LED7_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_6)                                                         /* 关闭LED7（高电平） */
#define LED7_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_6, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_6)) /* 翻转LED7状态 */

// LED8 (PF7) 操作宏
#define LED8_ON      GPIO_ResetBits(GPIOF, GPIO_Pin_7)                                                       /* 点亮LED8（低电平） */
#define LED8_OFF     GPIO_SetBits(GPIOF, GPIO_Pin_7)                                                         /* 关闭LED8（高电平） */
#define LED8_REVERSE GPIO_WriteBit(GPIOF, GPIO_Pin_7, (BitAction)!GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_7)) /* 翻转LED8状态 */

/************************* 蜂鸣器操作宏定义 *************************/
#define BEEP_OFF     GPIO_ResetBits(GPIOC, GPIO_Pin_13)                                                        /* 关闭蜂鸣器（低电平） */
#define BEEP_ON     GPIO_SetBits(GPIOC, GPIO_Pin_13)                                                          /* 开启蜂鸣器（高电平） */
#define BEEP_REVERSE GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13)) /* 翻转蜂鸣器状态 */

/************************* 函数声明 *************************/
void LEDGpio_Init(void); /* 初始化LED和蜂鸣器的GPIO引脚 */

#endif /* _DEVICE_H_ */ 