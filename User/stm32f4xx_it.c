/**
 ******************************************************************************
 * @file    SysTick/SysTick_Example/stm32f4xx_it.c
 * @author  MCD Application Team
 * @version V1.0.1
 * @date    13-April-2012
 * @brief   Main Interrupt Service Routines.
 *          This file provides template for all exceptions handler and
 *          peripherals interrupt service routine.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
 *
 * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *        http://www.st.com/software_license_agreement_liberty_v2
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "delay.h"
#include "DEVICE.h"
#include "USART.h"
extern u8 Update_Flag;
/** @addtogroup STM32F4xx_StdPeriph_Examples
 * @{
 */

/** @addtogroup SysTick_Example
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
 * @brief   This function handles NMI exception.
 * @param  None
 * @retval None
 */
void NMI_Handler(void)
{
}

/**
 * @brief  This function handles Hard Fault exception.
 * @param  None
 * @retval None
 */
void HardFault_Handler(void)
{
    /* Go to infinite loop when Hard Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Memory Manage exception.
 * @param  None
 * @retval None
 */
void MemManage_Handler(void)
{
    /* Go to infinite loop when Memory Manage exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Bus Fault exception.
 * @param  None
 * @retval None
 */
void BusFault_Handler(void)
{
    /* Go to infinite loop when Bus Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles Usage Fault exception.
 * @param  None
 * @retval None
 */
void UsageFault_Handler(void)
{
    /* Go to infinite loop when Usage Fault exception occurs */
    while (1) {
    }
}

/**
 * @brief  This function handles SVCall exception.
 * @param  None
 * @retval None
 */
void SVC_Handler(void)
{
}

/**
 * @brief  This function handles Debug Monitor exception.
 * @param  None
 * @retval None
 */
void DebugMon_Handler(void)
{
}

/**
 * @brief  This function handles PendSVC exception.
 * @param  None
 * @retval None
 */
void PendSV_Handler(void)
{
}
extern u32 g_systick_ms_counter;
/**
 * @brief  This function handles SysTick Handler.
 * @param  None
 * @retval None
 */
void SysTick_Handler(void)
{
    // 只有当有数据接收时，才开始计时
    if (Uart3.RXlenth > 0) {
        Uart3.Time++;
        // 超时时间设为10ms（可根据需求调整）
        if (Uart3.Time >= 10) {
            Uart3.ReceiveFinish = 1; // 置1，标识接收完成
            Uart3.Time          = 0; // 重置超时计数
        }
    }
    TimingDelay_Decrement();
}


void USART3_IRQHandler(void)
{
    unsigned char rec_data;
    // 检查接收非空中断标志
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        rec_data = USART_ReceiveData(USART3);           // 读取接收的数据
        USART_ClearITPendingBit(USART3, USART_IT_RXNE); // 清除中断标志

        // 防止缓冲区溢出（300是缓冲区大小）
        if (Uart3.RXlenth < 300) {
            Uart3.Rxbuf[Uart3.RXlenth++] = rec_data; // 数据存入缓冲区
            Uart3.Time                   = 0;        // 有新数据，重置超时计数（关键）
        }
        Uart3.ReceiveFinish = 0; // 接收中，置为未完成
    }

    // 处理溢出错误（可选，防止数据丢失）
    if (USART_GetITStatus(USART3, USART_IT_ORE) != RESET) {
        USART_ClearITPendingBit(USART3, USART_IT_ORE);
        (void)USART_ReceiveData(USART3); // 清除溢出错误
    }
}
extern unsigned char Int_flag;
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line8) != RESET) {
        Int_flag = 1;
        EXTI_ClearITPendingBit(EXTI_Line8);
    }
}

void TIM1_UP_TIM10_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM10, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM10, TIM_IT_Update);
        Update_Flag = 1; // 触发 Main 循环中的时间片逻辑
    }
}