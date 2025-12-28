/********************************************************************************
 * @file    Timer.c
 * @brief   STM32F407定时器和闹钟管理系统实现
 * @author  项目开发团队
 * @date    2025-12-27
 * @version 2.0
 * 
 * @description
 * 本模块实现了基于TIM10的精确时间片管理系统和优先级闹钟队列。
 * 
 * @features
 * 1. **时间片管理**:
 *    - TIM10产生10ms精确中断
 *    - 无舍入误差配置（168MHz / 168 / 10000 = 100Hz）
 * 
 * 2. **闹钟队列系统**:
 *    - 支持最多9个并发闹钟
 *    - 按剩余时间排序（最短作业优先）
 *    - 支持单次(ALARM_ONCE)和重复(ALARM_REPEAT)两种模式
 *    - O(1)时间复杂度的队首处理
 * 
 * 3. **中断优先级**:
 *    - 抢占优先级1（高优先级，仅次于关键中断）
 *    - 确保时间片不被其他中断阻塞
 * 
 * @usage
 * ```c
 * // 初始化
 * TIM10_TimeSliceInit();
 * AlarmManager_Init();
 * 
 * // 注册5分钟后执行的单次闹钟
 * Alarm_Register(30000, ALARM_ONCE, My_Callback);
 * 
 * // 注册每30秒重复的闹钟
 * Alarm_Register(3000, ALARM_REPEAT, Periodic_Task);
 * 
 * // 主循环中处理（每10ms调用一次）
 * if (Update_Flag) {
 *     AlarmTimer_Process();
 *     Update_Flag = 0;
 * }
 * ```
 * 
 * @note
 * - 时间单位为10ms时间片（ticks）
 * - 回调函数必须快速执行，避免阻塞时间片
 * - 最大定时时间: 2^32 ticks = 497天
 ********************************************************************************/

#include "Timer.h"

/********************************************************************************
 * @section 全局变量
 ********************************************************************************/

/* 闹钟队列数组（按剩余时间升序排列） */
AlarmNode_T alarm_queue[MAX_ALARM_NUM];

/* 队列当前大小 */
uint8_t queue_size = 0;

/* 下一个闹钟的唯一ID */
uint8_t next_id    = 1;

/********************************************************************************
 * @brief   TIM10定时器初始化为10ms时间片
 * @param   无
 * @retval  无
 * @note    定时器配置详解:
 *          - 时钟源: APB2 (168MHz)
 *          - 预分频: 167 (168MHz / 168 = 1MHz，无舍入误差)
 *          - 计数周期: 9999 (1MHz / 10000 = 100Hz，即10ms周期)
 *          - 中断优先级: 抢占1，子优先级0（高优先级）
 *          
 *          精度验证:
 *          - 168,000,000 / (167+1) / (9999+1) = 100.0 Hz ✓
 *          - 完全无舍入误差，保证长时间运行的时间精度
 ********************************************************************************/
void TIM10_TimeSliceInit(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);

    // 配置为10ms中断（严格无舍入误差，适配TIM10时钟=168MHz）
    TIM_TimeBaseStruct.TIM_Prescaler     = 167; // 168MHz / (167+1) = 1MHz（无舍入）
    TIM_TimeBaseStruct.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseStruct.TIM_Period        = 9999; // 1MHz / (9999+1) = 100Hz → 10ms周期（无舍入）
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM10, &TIM_TimeBaseStruct);

    TIM_ITConfig(TIM10, TIM_IT_Update, ENABLE);

    NVIC_InitStruct.NVIC_IRQChannel                   = TIM1_UP_TIM10_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1; // 提高优先级，避免被阻塞（关键优化）
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    TIM_Cmd(TIM10, ENABLE);
}

/********************************************************************************
 * @brief   闹钟管理器初始化
 * @param   无
 * @retval  无
 * @note    清空闹钟队列，将所有闹钟状态设为ALARM_INVALID
 ********************************************************************************/
void AlarmManager_Init(void)
{
    for (uint8_t i = 0; i < MAX_ALARM_NUM; i++) {
        alarm_queue[i].state = ALARM_INVALID;
        alarm_queue[i].id    = 0;
        alarm_queue[i].cb    = NULL;
    }
    queue_size = 0;
    next_id    = 1;
}

/********************************************************************************
 * @brief   插入闹钟到队列（内部函数）
 * @param   new_alarm 新闹钟节点
 * @retval  1-成功, 0-失败（队列已满）
 * @note    采用插入排序算法，按剩余时间升序插入
 *          时间复杂度: O(n)，n为队列大小
 *          队列满时拒绝插入
 ********************************************************************************/
static uint8_t InsertAlarm(AlarmNode_T new_alarm)
{
    if (queue_size >= MAX_ALARM_NUM) return 0;

    uint8_t insert_pos = queue_size;
    for (uint8_t i = 0; i < queue_size; i++) {
        if (new_alarm.remain_time < alarm_queue[i].remain_time) {
            insert_pos = i;
            break;
        }
    }

    for (uint8_t i = queue_size; i > insert_pos; i--) {
        alarm_queue[i] = alarm_queue[i - 1];
    }

    alarm_queue[insert_pos] = new_alarm;
    queue_size++;
    return 1;
}

/********************************************************************************
 * @brief   注册新闹钟（公开API）
 * @param   duration_ticks 持续时间（单位:10ms时间片）
 * @param   mode 闹钟模式 (ALARM_ONCE:单次, ALARM_REPEAT:重复)
 * @param   cb 回调函数指针
 * @retval  1-成功, 0-失败
 * @note    示例:
 *          - Alarm_Register(100, ALARM_ONCE, func) -> 1秒后执行一次
 *          - Alarm_Register(6000, ALARM_REPEAT, func) -> 每1分钟执行一次
 *          - Alarm_Register(30000, ALARM_REPEAT, func) -> 每5分钟执行一次
 ********************************************************************************/
// 新增：公开的注册函数
uint8_t Alarm_Register(uint32_t duration_ticks, AlarmMode mode, AlarmCallback cb)
{
    AlarmNode_T new_node;
    new_node.remain_time = duration_ticks;
    new_node.duration    = duration_ticks;
    new_node.mode        = mode;
    new_node.cb          = cb;
    new_node.state       = ALARM_VALID;
    new_node.id          = next_id++;

    return InsertAlarm(new_node);
}

/********************************************************************************
 * @brief   闹钟定时器处理函数（每10ms调用一次）
 * @param   无
 * @retval  无
 * @note    工作流程:
 *          1. 队首闹钟剩余时间减1
 *          2. 如果减到0，执行回调函数
 *          3. 单次模式: 从队列中删除
 *          4. 重复模式: 重置时间并重新插入队列（保持排序）
 *          
 *          优化点:
 *          - 只处理队首元素，O(1)时间复杂度
 *          - 防止下溢保护（remain_time > 0时才减）
 *          - 重复闹钟采用"先删除后插入"策略维护有序性
 ********************************************************************************/
void AlarmTimer_Process(void)
{
    if (queue_size > 0) {
        AlarmNode_T *front_alarm = &alarm_queue[0];
        if (front_alarm->state == ALARM_VALID) {
            // 防止下溢
            if (front_alarm->remain_time > 0) {
                front_alarm->remain_time--;
            }

            if (front_alarm->remain_time == 0) {
                if (front_alarm->cb != NULL) {
                    front_alarm->cb();
                }

                if (front_alarm->mode == ALARM_REPEAT) {
                    front_alarm->remain_time = front_alarm->duration;
                    AlarmNode_T temp         = *front_alarm;

                    for (uint8_t i = 0; i < queue_size - 1; i++) {
                        alarm_queue[i] = alarm_queue[i + 1];
                    }
                    queue_size--;

                    InsertAlarm(temp);
                } else {
                    for (uint8_t i = 0; i < queue_size - 1; i++) {
                        alarm_queue[i] = alarm_queue[i + 1];
                    }
                    queue_size--;
                }
            }
        }
    }
}