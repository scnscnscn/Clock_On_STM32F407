/********************************************************************************
 * @file    KEY.c
 * @brief   STM32F407按键驱动实现
 * @author  qinyx / 项目团队
 * @date    2025-12-27
 * @version 2.0
 * 
 * @description
 * 本文件实现了3个独立按键的GPIO初始化和扫描检测功能。
 * 
 * @hardware
 * - KEY_S1: PF8
 * - KEY_S2: PF9
 * - KEY_S3: PF10
 * - 按键模式: 上拉输入（按下时为低电平）
 * 
 * @features
 * 1. 软件防抖处理（100ms去抖时间）
 * 2. 边沿触发检测（只在按键按下瞬间触发一次）
 * 3. 支持单键检测（不支持组合键）
 ********************************************************************************/

#include "stm32f4xx.h"
#include "KEY.h"
#include "delay.h"

/********************************************************************************
 * @brief   按键GPIO初始化函数
 * @param   无
 * @retval  无
 * @note    配置PF8/PF9/PF10为上拉输入模式
 *          按键未按下时为高电平，按下时为低电平
 ********************************************************************************/
void KEYGpio_Init(void)
{    
    GPIO_InitTypeDef  GPIO_InitStructure;
    
    /* 使能GPIOF时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    
    /* 配置按键引脚 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;  /* KEY_S1/S2/S3 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;         /* 输入模式 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;   /* 速率100MHz */
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;         /* 上拉模式（按键未按下时为高） */
    GPIO_Init(GPIOF, &GPIO_InitStructure);
}

/********************************************************************************
 * @brief   按键扫描函数
 * @param   无
 * @retval  按键值
 *          - 0: 无按键按下
 *          - 1: KEY_S1 被按下
 *          - 2: KEY_S2 被按下
 *          - 3: KEY_S3 被按下
 * @note    采用边沿触发方式，只在按键按下的瞬间返回一次按键值
 *          包含软件防抖处理（100ms）
 *          连续按住不会重复触发，必须松开再按才会再次触发
 ********************************************************************************/
unsigned char KeyScan(void)
{		 
    unsigned char buf[4] = {0};   /* 按键状态缓冲区 */
    unsigned char temp = 0;       /* 返回值（按键编号） */
    static u8 key_up=1;           /* 按键松开标志（静态变量，保持状态） */
    
    /* 第一次读取按键状态 */
    buf[0] = KEY_S1_READ();
    buf[1] = KEY_S2_READ();
	buf[2] = KEY_S3_READ();
    
    /* 检测到按键按下（任意一个为低电平）且上次按键已松开 */
    if(key_up && (buf[0] == 0 || buf[1] == 0 || buf[2] == 0))
    {
        key_up = 0;  /* 清除松开标志，防止重复触发 */
        
        delay_ms(100);  /* 延时100ms进行软件防抖 */
    
        /* 防抖后再次读取按键状态 */
        buf[0] = KEY_S1_READ();
		buf[1] = KEY_S2_READ();
		buf[2] = KEY_S3_READ();
        
        /* 判断具体是哪个按键按下（单键检测） */
        /* KEY_S1: PF8按下，其他未按下 */
        if ( (buf[0] == 0) && (buf[1] == 1) && (buf[2] == 1))
        {
            temp = 1;
        }
        
        /* KEY_S2: PF9按下，其他未按下 */
        if ( (buf[0] == 1) && (buf[1] == 0) && (buf[2] == 1))
        {
            temp = 2;
        }
		
		/* KEY_S3: PF10按下，其他未按下 */
        if ( (buf[0] == 1) && (buf[1] == 1) && (buf[2] == 0))
        {
            temp = 3;
        }
    }
    /* 检测到所有按键都松开（都为高电平） */
    else if (buf[0] == 1 && buf[1] == 1 && buf[2] == 1)
    {
        key_up = 1;  /* 设置松开标志，允许下次按键触发 */
    }
    
    return temp;  /* 返回按键值 */
}
