/********************************************************************************
 * @file    EXIT.c
 * @brief   STM32F407外部中断配置实现
 * @author  项目开发团队
 * @date    2025-12-27
 * @version 2.0
 * 
 * @description
 * 本文件实现了外部中断EXTI8的配置，用于按键中断检测。
 * 
 * @hardware
 * - 中断引脚: PF8 (EXTI Line 8)
 * - 触发方式: 下降沿触发（按键按下）
 * - 中断组: EXTI9_5_IRQn (包含EXTI5-EXTI9)
 * 
 * @note
 * - 中断优先级较低（抢占15，子优先级15），确保不干扰关键任务
 * - 中断服务程序在stm32f4xx_it.c中实现（EXTI9_5_IRQHandler）
 ********************************************************************************/

#include "stm32f4xx.h"
#include "EXIT.h"

/********************************************************************************
 * @brief   外部中断EXTI8配置函数
 * @param   无
 * @retval  无
 * @note    配置PF8引脚为外部中断输入，下降沿触发
 *          用途: 蜂鸣器控制按键（按下时关闭蜂鸣器）
 ********************************************************************************/
void EXTI_Config(void)
{
    EXTI_InitTypeDef   EXTI_InitStructure;
    GPIO_InitTypeDef   GPIO_InitStructure;
    NVIC_InitTypeDef   NVIC_InitStructure;

    /* 使能GPIOF和SYSCFG时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);  /* GPIOF时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE); /* SYSCFG时钟（EXTI配置需要） */

    /* 配置PF8为上拉输入 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;          /* 输入模式 */
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;          /* 上拉（按键未按下时为高电平） */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;             /* PF8引脚 */
    GPIO_Init(GPIOF, &GPIO_InitStructure);

    /* 将PF8连接到EXTI Line 8 */
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOF, EXTI_PinSource8);

    /* 配置EXTI Line 8 */
    EXTI_InitStructure.EXTI_Line = EXTI_Line8;             /* EXTI线8 */
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;    /* 中断模式 */
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;/* 下降沿触发（按键按下） */
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;              /* 使能EXTI线 */
    EXTI_Init(&EXTI_InitStructure);

    /* 配置NVIC - EXTI9_5中断 */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;                 /* EXTI5-9中断通道 */
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;      /* 抢占优先级15（最低） */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;             /* 子优先级15（最低） */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                    /* 使能中断 */
    NVIC_Init(&NVIC_InitStructure);
}
