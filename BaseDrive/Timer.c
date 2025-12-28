#include "Timer.h"

AlarmNode_T alarm_queue[MAX_ALARM_NUM];
uint8_t queue_size = 0;
uint8_t next_id    = 1;

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

// 内部静态函数：插入排序
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