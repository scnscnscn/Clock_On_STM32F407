/********************************************************************************
 * @file    USART.c
 * @brief   STM32F4xx USART3串口驱动实现文件
 * @author  WLQVincent@gmail.com
 * @date    2025-12-24
 * @note    1. 串口3硬件引脚为PC10(TX)、PC11(RX)，采用复用推挽输出+上拉输入；
 *          2. 支持中断接收和轮询发送，重定向fputc实现printf输出；
 *          3. 接收缓冲区大小为300字节，接收状态通过USARTDATA结构体管理。
 ********************************************************************************/
#include "stm32f4xx.h"
#include "USART.h"

/********************************************************************************
 * @brief   串口3接收缓冲区
 * @details 用于存储中断接收到的串口数据，大小为300字节，初始化为0
 ********************************************************************************/
unsigned char ReceiveBuffer[300] = {0};

/********************************************************************************
 * @brief   全局串口3数据结构体实例
 * @details 管理串口3的接收缓冲区指针、接收长度、超时计数和接收完成标志
 ********************************************************************************/
USARTDATA Uart3;

/********************************************************************************
 * @brief   初始化USART3的GPIO、串口参数和中断配置
 * @param   无
 * @retval  无
 * @note    1. 串口参数：115200bps、8位数据位、1位停止位、无校验、无硬件流控；
 *          2. 中断优先级分组为2，抢占优先级0，子优先级0；
 *          3. GPIO速率配置为2MHz，上拉模式，复用功能绑定USART3。
 ********************************************************************************/
void UART3_Configuration(void)
{
    // 定义GPIO、串口、中断初始化结构体
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /************************* 初始化串口接收状态 *************************/
    Uart3.ReceiveFinish = 0;             // 接收完成标志置0：未完成
    Uart3.RXlenth       = 0;             // 已接收数据长度置0
    Uart3.Time          = 0;             // 接收超时计数器置0
    Uart3.Rxbuf         = ReceiveBuffer; // 绑定接收缓冲区指针

    /************************* 使能外设时钟 *************************/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);  // 使能GPIOC时钟（AHB1总线）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE); // 使能USART3时钟（APB1总线）

    /************************* 配置GPIO复用功能 *************************/
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;    // GPIO模式：复用功能
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;   // 输出类型：推挽输出（TX引脚）
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;    // 上下拉：上拉（防止引脚浮空）
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // GPIO速率：2MHz

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11; // 配置PC11为USART3_RX
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; // 配置PC10为USART3_TX
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // 将GPIO引脚与USART3复用功能绑定
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3); // PC10 -> USART3_TX
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3); // PC11 -> USART3_RX

    /************************* 配置USART3串口参数 *************************/
    USART_InitStructure.USART_BaudRate            = 115200;                         // 波特率：115200
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;            // 数据位：8位
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;               // 停止位：1位
    USART_InitStructure.USART_Parity              = USART_Parity_No;                // 校验位：无
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 硬件流控：无
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;  // 工作模式：收发双工

    USART_Init(USART3, &USART_InitStructure); // 应用串口配置

    /************************* 使能串口和接收中断 *************************/
    USART_Cmd(USART3, ENABLE);                     // 使能USART3外设
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 使能接收非空中断（RXNE）

    /************************* 配置中断优先级 *************************/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 中断优先级分组：组2（2位抢占+2位子优先级）

    NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn; // 中断通道：USART3
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;           // 抢占优先级：0
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;           // 子优先级：0
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;      // 使能中断通道
    NVIC_Init(&NVIC_InitStructure);                                     // 应用中断配置

    USART_Cmd(USART3, ENABLE); // 再次使能USART3
}

/********************************************************************************
 * @brief   重定向fputc函数，支持printf输出到USART3
 * @param   ch: 要输出的字符
 * @param   f: 文件指针（标准输出，忽略）
 * @retval  输出的字符
 * @note    1. 需在编译器中开启"Use MicroLIB"才能使用printf；
 *          2. 采用轮询方式发送，阻塞直到字符发送完成。
 ********************************************************************************/
int fputc(int ch, FILE *f)
{
    USART3->SR; // 读取SR寄存器（冗余操作，可清除无关标志）

    USART_SendData(USART3, (u8)ch); // 发送字符到USART3
    // 等待发送完成标志置位
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET) {
        ;
    }

    return (ch); // 返回发送的字符
}

/********************************************************************************
 * @brief   USART3发送字节流数据
 * @param   Data: 待发送数据的起始地址指针
 * @param   length: 待发送数据的字节长度
 * @retval  无
 * @note    1. 采用轮询方式发送，阻塞直到所有数据发送完成；
 *          2. 数据发送完成后等待TC标志置位，确保数据完全发送。
 ********************************************************************************/
void USART3_Senddata(unsigned char *Data, unsigned int length)
{
    // 循环发送每个字节，长度递减至0
    while (length--) {
        USART_SendData(USART3, *Data++); // 发送当前字节，指针后移
        // 等待发送完成标志置位
        while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
    }
}