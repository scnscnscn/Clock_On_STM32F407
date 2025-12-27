#include "stm32f4xx.h"
#include "EXIT.h"

void EXTI_Config(void)
{
    EXTI_InitTypeDef   EXTI_InitStructure;
    GPIO_InitTypeDef   GPIO_InitStructure;
    NVIC_InitTypeDef   NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);  
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;          
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;           
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;              
    GPIO_Init(GPIOF, &GPIO_InitStructure);                

    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOF, EXTI_PinSource8);  

    EXTI_InitStructure.EXTI_Line = EXTI_Line8;             
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;    
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;              
    EXTI_Init(&EXTI_InitStructure);                         

    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;      
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0F;    
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0F;    
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;         
    NVIC_Init(&NVIC_InitStructure);                         
}
