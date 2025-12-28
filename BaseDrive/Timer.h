#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f4xx.h"
#include <stdint.h>
#include <stddef.h>

// 闹钟重复模式枚举
typedef enum {
    ALARM_ONCE = 0, // 单次闹钟
    ALARM_REPEAT    // 重复闹钟
} AlarmMode;

// 闹钟状态枚举
typedef enum {
    ALARM_INVALID = 0,
    ALARM_VALID
} AlarmState;

// 闹钟回调函数类型
typedef void (*AlarmCallback)(void);

// 闹钟节点结构体
typedef struct {
    uint32_t remain_time;
    uint32_t duration;
    AlarmMode mode;
    AlarmState state;
    AlarmCallback cb;
    uint8_t id;
} AlarmNode_T;

#define MAX_ALARM_NUM 9
// 定义时间片为10ms (100Hz)
#define TIME_SLICE_MS 10

extern AlarmNode_T alarm_queue[MAX_ALARM_NUM];
extern uint8_t queue_size;
extern uint8_t next_id;

// ================= API =================
void TIM10_TimeSliceInit(void);
void AlarmManager_Init(void);
void AlarmTimer_Process(void);
// 新增：注册闹钟函数
uint8_t Alarm_Register(uint32_t duration_ticks, AlarmMode mode, AlarmCallback cb);

#endif /* __TIMER_H */