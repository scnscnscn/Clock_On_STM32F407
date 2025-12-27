/********************************************************************************
 * @file    TIMER.c
 * @brief   基于TIM10的多闹钟定时器管理实现文件（STM32F4xx）
 * @author  开发者名称
 * @date    2025-12-23
 * @version V1.0
 * @note    1. 静态函数仅在本文件内可见，避免全局命名冲突；
 *          2. 优先级队列采用插入排序实现，保证短时间闹钟优先处理；
 *          3. 中断处理函数需在it.c中实现，仅调用AlarmTimer_Process接口。
 ********************************************************************************/
#include "TIMER.h"

/********************************************************************************
 * @brief   全局变量实际定义（头文件中用extern声明）
 * @note    alarm_queue为优先级队列，queue_size为当前有效节点数，next_id为ID分配计数器
 ********************************************************************************/
AlarmNode_T alarm_queue[MAX_ALARM_NUM];
uint8_t queue_size = 0;
uint8_t next_id    = 1;

/********************************************************************************
 * @brief   TIM10定时器初始化函数
 * @param   无
 * @retval  无
 * @note    1. TIM10挂载在APB2总线，时钟频率为84MHz；
 *          2. 配置为10ms中断：预分频83（84MHz/(83+1)=1MHz），自动重装9999（1MHz*10ms=10000次）；
 *          3. NVIC优先级可根据项目调整，避免与其他中断冲突。
 ********************************************************************************/
void TIM10_TimeSliceInit(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    // 1. 使能TIM10时钟（APB2总线）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);

    // 2. 配置TIM10时基参数：1000ms中断
    TIM_TimeBaseStruct.TIM_Prescaler     = 7999;               // 预分频系数：84MHz/8400=10kHz
    TIM_TimeBaseStruct.TIM_CounterMode   = TIM_CounterMode_Up; // 向上计数模式
    TIM_TimeBaseStruct.TIM_Period        = 10499;              // 自动重装值：10kHz*10ms=100次
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;       // 时钟分频：1分频
    TIM_TimeBaseInit(TIM10, &TIM_TimeBaseStruct);              // 应用时基配置

    // 3. 使能TIM10更新中断（时间片中断触发源）
    TIM_ITConfig(TIM10, TIM_IT_Update, ENABLE);

    // 4. 配置NVIC中断优先级
    NVIC_InitStruct.NVIC_IRQChannel                   = TIM1_UP_TIM10_IRQn; // TIM10中断通道
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;                  // 抢占优先级：2（数值越小优先级越高）
    NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 3;                  // 子优先级：3
    NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;             // 使能中断通道
    NVIC_Init(&NVIC_InitStruct);

    // 5. 启动TIM10定时器
    TIM_Cmd(TIM10, ENABLE);
}

/********************************************************************************
 * @brief   闹钟管理器初始化函数
 * @param   无
 * @retval  无
 * @note    初始化时清空队列所有节点，重置ID分配计数器，防止上电后数据乱码
 ********************************************************************************/
void AlarmManager_Init(void)
{
    // 遍历队列，初始化所有节点为无效状态
    for (uint8_t i = 0; i < MAX_ALARM_NUM; i++) {
        alarm_queue[i].state = ALARM_INVALID; // 标记为无效
        alarm_queue[i].id    = 0;             // 重置ID
        alarm_queue[i].cb    = NULL;          // 清空回调函数指针
    }
    queue_size = 0; // 队列长度置0
    next_id    = 1; // ID计数器重置为1
}

/********************************************************************************
 * @brief   内部静态函数：获取有效闹钟ID（优先复用已删除的ID）
 * @param   无
 * @retval  有效ID：1~MAX_ALARM_NUM；0表示无可用ID（队列满）
 * @note    复用ID避免ID溢出，提升ID利用率
 ********************************************************************************/
static uint8_t GetValidID(void)
{
    // 第一步：遍历查找未被使用的ID（优先复用）
    for (uint8_t i = 1; i <= MAX_ALARM_NUM; i++) {
        uint8_t id_used = 0;
        // 检查当前ID是否已被占用
        for (uint8_t j = 0; j < MAX_ALARM_NUM; j++) {
            if (alarm_queue[j].state == ALARM_VALID && alarm_queue[j].id == i) {
                id_used = 1;
                break;
            }
        }
        if (!id_used) return i; // 返回未被使用的ID
    }
    // 第二步：无复用ID时，自增分配新ID（不超过最大值）
    if (next_id <= MAX_ALARM_NUM) return next_id++;
    return 0; // 队列满，无可用ID
}

/********************************************************************************
 * @brief   内部静态函数：插入闹钟到优先级队列
 * @param   new_alarm 待插入的闹钟节点
 * @retval  1：插入成功；0：插入失败（队列满）
 * @note    采用插入排序，按remain_time升序排列，保证最短时间闹钟在队首
 ********************************************************************************/
static uint8_t InsertAlarm(AlarmNode_T new_alarm)
{
    if (queue_size >= MAX_ALARM_NUM) return 0; // 队列已满，插入失败

    // 查找插入位置：找到第一个比新节点剩余时间长的位置
    uint8_t insert_pos = queue_size; // 默认插入到队列末尾
    for (uint8_t i = 0; i < queue_size; i++) {
        if (new_alarm.remain_time < alarm_queue[i].remain_time) {
            insert_pos = i;
            break;
        }
    }

    // 后移元素，腾出插入位置
    for (uint8_t i = queue_size; i > insert_pos; i--) {
        alarm_queue[i] = alarm_queue[i - 1];
    }

    // 插入新节点并更新队列长度
    alarm_queue[insert_pos] = new_alarm;
    queue_size++;
    return 1;
}

/********************************************************************************
 * @brief   添加闹钟函数（对外接口）
 * @param   duration_ms 定时时长（单位：ms，需≥TIME_SLICE_MS）
 * @param   mode        闹钟重复模式（ALARM_ONCE/ALARM_REPEAT）
 * @param   cb          闹钟触发回调函数（不可为NULL）
 * @retval  闹钟ID：1~MAX_ALARM_NUM；0表示添加失败
 * @note    1. 建议在调用前关闭总中断（__disable_irq()），防止中断与主循环竞争；
 *          2. 示例：__disable_irq(); Alarm_Add(1000, ALARM_ONCE, cb); __enable_irq();
 ********************************************************************************/
uint8_t Alarm_Add(uint32_t duration_ms, AlarmMode mode, AlarmCallback cb)
{
    // 参数合法性检查：回调函数非空、时长≥时间片、队列未满
    if (cb == NULL || duration_ms < TIME_SLICE_MS || queue_size >= MAX_ALARM_NUM) {
        return 0;
    }

    // 获取有效ID
    uint8_t new_id = GetValidID();
    if (new_id == 0) return 0; // 无可用ID

    // 构造新闹钟节点
    AlarmNode_T new_alarm;
    new_alarm.id          = new_id;
    new_alarm.duration    = duration_ms / TIME_SLICE_MS; // 转换为时间片数
    new_alarm.remain_time = new_alarm.duration;          // 初始剩余时间=总时长
    new_alarm.mode        = mode;
    new_alarm.state       = ALARM_VALID; // 标记为有效
    new_alarm.cb          = cb;          // 绑定回调函数

    // 插入到优先级队列
    if (!InsertAlarm(new_alarm)) return 0;

    return new_id; // 返回闹钟ID，供后续删除使用
}

/********************************************************************************
 * @brief   删除闹钟函数（对外接口）
 * @param   alarm_id 待删除的闹钟ID（由Alarm_Add返回）
 * @retval  1：删除成功；0：删除失败（ID无效/队列空）
 * @note    1. 建议在调用前关闭总中断，防止中断与主循环竞争；
 *          2. 删除后队列自动前移，保证连续性。
 ********************************************************************************/
uint8_t Alarm_Delete(uint8_t alarm_id)
{
    // 参数合法性检查：ID非0、队列非空
    if (alarm_id == 0 || queue_size == 0) return 0;

    // 查找目标闹钟在队列中的位置
    uint8_t target_pos = MAX_ALARM_NUM; // 初始化为无效位置
    for (uint8_t i = 0; i < queue_size; i++) {
        if (alarm_queue[i].state == ALARM_VALID && alarm_queue[i].id == alarm_id) {
            target_pos = i;
            break;
        }
    }
    if (target_pos == MAX_ALARM_NUM) return 0; // 未找到目标闹钟

    // 前移元素，移除目标节点
    for (uint8_t i = target_pos; i < queue_size - 1; i++) {
        alarm_queue[i] = alarm_queue[i + 1];
    }

    // 队列长度减1，并重置最后一个节点为无效状态
    queue_size--;
    alarm_queue[queue_size].state = ALARM_INVALID;
    alarm_queue[queue_size].id    = 0;
    alarm_queue[queue_size].cb    = NULL;

    return 1;
}

/********************************************************************************
 * @brief   闹钟核心处理逻辑（供中断函数调用）
 * @param   无
 * @retval  无
 * @note    1. 仅处理队首闹钟（剩余时间最短），保证定时精度；
 *          2. 中断中执行，禁止耗时操作（回调函数需轻量化）；
 *          3. 重复闹钟触发后会重新插入队列，单次闹钟触发后直接移除。
 ********************************************************************************/
void AlarmTimer_Process(void)
{
    if (queue_size > 0) {
        // 取队首闹钟（剩余时间最短）
        AlarmNode_T *front_alarm = &alarm_queue[0];
        if (front_alarm->state == ALARM_VALID) {
            front_alarm->remain_time--; // 时间片减1

            // 闹钟到期：执行回调并处理模式
            if (front_alarm->remain_time == 0) {
                // 执行用户自定义回调函数（需非空）
                if (front_alarm->cb != NULL) {
                    front_alarm->cb();
                }

                // 重复闹钟：重置时间并重新插入队列
                if (front_alarm->mode == ALARM_REPEAT) {
                    front_alarm->remain_time = front_alarm->duration; // 重置剩余时间
                    AlarmNode_T temp         = *front_alarm;

                    // 移除原节点
                    for (uint8_t i = 0; i < queue_size - 1; i++) {
                        alarm_queue[i] = alarm_queue[i + 1];
                    }
                    queue_size--;

                    // 重新插入队列（保持优先级排序）
                    InsertAlarm(temp);
                }
                // 单次闹钟：直接移除
                else {
                    for (uint8_t i = 0; i < queue_size - 1; i++) {
                        alarm_queue[i] = alarm_queue[i + 1];
                    }
                    queue_size--;
                }
            }
        }
    }
}