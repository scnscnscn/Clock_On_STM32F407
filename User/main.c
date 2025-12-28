/********************************************************************************
 * @file    main.c
 * @brief   STM32F407智能时钟与天气显示系统 - 主程序
 * @author  项目开发团队
 * @date    2025-12-27
 * @version 2.0
 * 
 * @description
 * 本程序实现了一个基于STM32F407的智能时钟系统，具备以下功能：
 * 1. 实时时钟显示与管理
 * 2. 天气信息显示（温度、湿度、降水量、天气图标）
 * 3. 多闹钟设置与管理（支持4个独立闹钟）
 * 4. 触摸屏交互界面（支持双页面切换）
 * 5. 串口通信获取天气数据
 * 6. LED和蜂鸣器指示
 * 
 * @hardware
 * - MCU: STM32F407ZGT6
 * - LCD: 带触摸屏的彩色液晶显示屏
 * - 通信: USART3串口
 * - 定时器: TIM10 (10ms时间片)
 * - LED: GPIOF0-7
 * - 蜂鸣器: GPIOC13
 * - 按键: GPIOF8
 * 
 * @note
 * - 系统时钟频率: 168MHz
 * - 时间片周期: 10ms (TIM10中断)
 * - 串口波特率: 115200
 * - LCD分辨率: 480x800
 ********************************************************************************/

#include "stm32f4xx.h"
#include "USART.h"
#include "delay.h"
#include "DEVICE.h"
#include "lcd.h"
#include "Timer.h" 
#include "string.h"
#include "stdlib.h"
#include "weather_icons.h"
#include "touch.h"
#include "KEY.h"
#include "EXIT.h"
#include "kokomi.h"

/* 外部中断标志（由EXIT中断服务程序设置） */
unsigned char Int_flag = 0;

/********************************************************************************
 * @section 函数声明区
 ********************************************************************************/
void Alarm0_Callback(void);
void Alarm1_Callback(void);
void Page_Switch(u8 page);
void Handle_Touch_Func(u8 func_id);
void Draw_Page1_Alarm_List(void);
void Draw_Page2_Alarm_Set(void);
u8 str_split(char *src, char delim, char **result, u8 max_len);

/********************************************************************************
 * @section 页面和功能定义
 ********************************************************************************/

/* 页面编号定义 */
#define PAGE_1            0  /**< 主界面（天气和闹钟列表） */
#define PAGE_2            1  /**< 闹钟设置界面 */

/* 触摸功能ID定义 - 用于识别不同的触摸按钮 */
#define FUNC_ALARM_1      0  /**< 闹钟1按钮 */
#define FUNC_ALARM_2      1  /**< 闹钟2按钮 */
#define FUNC_ALARM_3      2  /**< 闹钟3按钮 */
#define FUNC_ALARM_4      3  /**< 闹钟4按钮 */
#define FUNC_HOUR_ADD     4  /**< 小时增加按钮 */
#define FUNC_HOUR_SUB     5  /**< 小时减少按钮 */
#define FUNC_MIN_ADD      6  /**< 分钟增加按钮 */
#define FUNC_MIN_SUB      7  /**< 分钟减少按钮 */
#define FUNC_ALARM_EN     8  /**< 闹钟启用/禁用切换按钮 */
#define FUNC_SAVE_ALARM   9  /**< 保存闹钟设置按钮 */
#define FUNC_CANCEL_ALARM 10 /**< 取消编辑返回按钮 */

/********************************************************************************
 * @section 全局状态变量
 ********************************************************************************/

/* 触摸和刷新控制变量 */
u8 g_last_touch_flag          = 0;   /**< 上次触摸状态标志 */
u8 g_alarm_data_changed       = 0;   /**< 闹钟数据变化标志，用于触发UI更新 */
u32 g_touch_debounce_cnt      = 0;   /**< 触摸防抖动计数器 */
const u16 REFRESH_THRESHOLD   = 50;  /**< 主界面刷新阈值（循环次数） */
const u16 TOUCH_DEBOUNCE_TIME = 2;   /**< 触摸防抖时间（单位：循环次数） */

/* 页面2（闹钟设置界面）控制变量 */
u8 g_page2_last_page = PAGE_1;  /**< 页面2上次所在的页面，用于检测页面切换 */
u8 g_page2_btn_drawn = 0;       /**< 页面2按钮是否已绘制标志 */
u8 g_current_page    = PAGE_1;  /**< 当前显示的页面编号 */

// 天气代码转为中文描述
const char *WeatherCodeToName(uint16_t code)
{
    switch (code) {
        case 100:
            return "晴";
        case 101:
            return "多云";
        case 102:
            return "少云";
        case 103:
            return "晴间多云";
        case 104:
            return "阴";
        case 150:
            return "晴";
        case 151:
            return "多云";
        case 152:
            return "少云";
        case 153:
            return "晴间多云";
        case 300:
            return "阵雨";
        case 301:
            return "强阵雨";
        case 302:
            return "雷阵雨";
        case 303:
            return "强雷阵雨";
        case 304:
            return "雷阵雨伴有冰雹";
        case 305:
            return "小雨";
        case 306:
            return "中雨";
        case 307:
            return "大雨";
        case 308:
            return "极端降雨";
        case 309:
            return "毛毛雨/细雨";
        case 310:
            return "暴雨";
        case 311:
            return "大暴雨";
        case 312:
            return "特大暴雨";
        case 313:
            return "冻雨";
        case 314:
            return "小到中雨";
        case 315:
            return "中到大雨";
        case 316:
            return "大到暴雨";
        case 317:
            return "暴雨到大暴雨";
        case 318:
            return "大暴雨到特大暴雨";
        case 350:
            return "阵雨";
        case 351:
            return "强阵雨";
        case 399:
            return "雨";
        case 400:
            return "小雪";
        case 401:
            return "中雪";
        case 402:
            return "大雪";
        case 403:
            return "暴雪";
        case 404:
            return "雨夹雪";
        case 405:
            return "雨雪天气";
        case 406:
            return "阵雨夹雪";
        case 407:
            return "阵雪";
        case 408:
            return "小到中雪";
        case 409:
            return "中到大雪";
        case 410:
            return "大到暴雪";
        case 456:
            return "阵雨夹雪";
        case 457:
            return "阵雪";
        case 499:
            return "雪";
        case 500:
            return "薄雾";
        case 501:
            return "雾";
        case 502:
            return "霾";
        case 503:
            return "扬沙";
        case 507:
            return "沙尘暴";
        case 508:
            return "强沙尘暴";
        case 509:
            return "浓雾";
        case 510:
            return "强浓雾";
        case 511:
            return "中度霾";
        case 512:
            return "重度霾";
        case 513:
            return "严重霾";
        case 514:
            return "大雾";
        case 515:
            return "特强浓雾";
        default:
            return "未知";
    }
}

typedef struct {
    u8 hour;
    u8 minute;
    u8 second;
} RealTime;
RealTime g_real_time = {0};

// 最大闹钟数量（1个占位 + 4个可用闹钟，对应页面按钮和定时器）
#define MAX_ALARM_CNT 5
typedef struct {
    u8 hour;
    u8 minute;
    u8 enable;
    u8 show;
    void (*callback)(void);
    char name[16];
} AlarmInfo;

// Alarm0 为占位，Alarm1-4 对应 UI 上的四个闹钟
AlarmInfo g_alarm_list[MAX_ALARM_CNT] = {
    {0, 0, 0, 0, NULL, "保留位"},             // 占位，不使用
    {7, 30, 0, 1, Alarm1_Callback, "闹钟1"},  // 闹钟1
    {0, 0, 1, 1, Alarm1_Callback, "闹钟2"},   // 闹钟2
    {12, 30, 1, 1, Alarm1_Callback, "闹钟3"}, // 闹钟3
    {18, 0, 0, 1, Alarm1_Callback, "闹钟4"}   // 闹钟4
};

// 触摸按钮配置
typedef struct {
    u16 x1;
    u16 y1;
    u16 x2;
    u16 y2;
    u8 func_id;
    char name[16];
} Touch_Button_t;

Touch_Button_t g_touch_btns_page1[] = {
    {20, 235, 450, 265, FUNC_ALARM_1, "闹钟1"},
    {20, 270, 450, 300, FUNC_ALARM_2, "闹钟2"},
    {20, 305, 450, 335, FUNC_ALARM_3, "闹钟3"},
    {20, 340, 450, 370, FUNC_ALARM_4, "闹钟4"},
};

Touch_Button_t g_touch_btns_page2[] = {
    {100, 150, 180, 180, FUNC_HOUR_ADD, "小时+"},
    {280, 150, 360, 180, FUNC_HOUR_SUB, "小时-"},
    {100, 200, 180, 230, FUNC_MIN_ADD, "分钟+"},
    {280, 200, 360, 230, FUNC_MIN_SUB, "分钟-"},
    {180, 250, 300, 280, FUNC_ALARM_EN, "开/关"},
    {80, 320, 220, 350, FUNC_SAVE_ALARM, "保存"},
    {260, 320, 400, 350, FUNC_CANCEL_ALARM, "取消"},
};

#define BTN_CNT_PAGE1 (sizeof(g_touch_btns_page1) / sizeof(Touch_Button_t))
#define BTN_CNT_PAGE2 (sizeof(g_touch_btns_page2) / sizeof(Touch_Button_t))

// 设置闹钟属性
u8 Alarm_Set(u8 alarm_idx, u8 hour, u8 minute, u8 enable, u8 show, void (*callback)(void), const char *name)
{
    if (alarm_idx >= MAX_ALARM_CNT) return 0;
    g_alarm_list[alarm_idx].hour     = hour;
    g_alarm_list[alarm_idx].minute   = minute;
    g_alarm_list[alarm_idx].enable   = enable;
    g_alarm_list[alarm_idx].show     = show;
    g_alarm_list[alarm_idx].callback = callback;
    if (name != NULL) {
        strncpy(g_alarm_list[alarm_idx].name, name, sizeof(g_alarm_list[alarm_idx].name) - 1);
    }
    g_alarm_data_changed = 1;
    return 1;
}

// 检查闹钟（遍历 g_alarm_list 中有效的闹钟）
void Alarm_CheckAndTrigger(void)
{
    static u8 last_second = 0xFF;
    // 秒未变化直接返回，避免重复处理
    if (g_real_time.second == last_second) return;
    last_second = g_real_time.second;

    // 仅在每分钟的第 0 秒检查闹钟
    if (g_real_time.second == 0) {
        for (u8 i = 1; i < MAX_ALARM_CNT; i++) { // 跳过占位的 0 号
            if (g_alarm_list[i].enable &&
                g_alarm_list[i].hour == g_real_time.hour &&
                g_alarm_list[i].minute == g_real_time.minute &&
                g_alarm_list[i].callback != NULL) {

                g_alarm_list[i].callback();
                g_alarm_data_changed = 1;
            }
        }
    }
}

// 闹钟信息复制
void Alarm_Copy(AlarmInfo *dst, const AlarmInfo *src)
{
    if (dst == NULL || src == NULL) return;
    dst->hour     = src->hour;
    dst->minute   = src->minute;
    dst->enable   = src->enable;
    dst->show     = src->show; // 是否在 UI 显示
    dst->callback = src->callback;
    strncpy(dst->name, src->name, sizeof(dst->name) - 1);
}

// 天气数据结构
typedef struct {
    char time_str[25];
    u8 temp;
    u8 feels_like;
    u8 precip;
    u8 icons;
    u8 humidity;
} WeatherData;

WeatherData g_weather;

// 将实时数据格式化为字符串
void format_real_time_to_str(void)
{
    static char date_part[12] = "2025-12-27";
    sprintf((char *)g_weather.time_str, "%s %02d:%02d:%02d", date_part, g_real_time.hour, g_real_time.minute, g_real_time.second);
}

// 解析字符串时间到实时结构
void parse_time_str_to_realtime(char *time_str)
{
    char *date_time[2];
    if (str_split(time_str, ' ', date_time, 2) != 2) return;
    static char date_part[12] = {0};
    strncpy(date_part, date_time[0], sizeof(date_part) - 1);
    char *hms[3];
    if (str_split(date_time[1], ':', hms, 3) != 3) return;
    g_real_time.hour   = (u8)atoi(hms[0]);
    g_real_time.minute = (u8)atoi(hms[1]);
    g_real_time.second = (u8)atoi(hms[2]);
}

// 分割字符串
u8 str_split(char *src, char delim, char **result, u8 max_len)
{
    u8 count          = 0;
    char delim_str[2] = {delim, '\0'};
    char *token       = strtok(src, delim_str);
    while (token != NULL && count < max_len) {
        result[count++] = token;
        token           = strtok(NULL, delim_str);
    }
    return count;
}

// 解析天气串口数据
void weather_parse(char *recv_buf)
{
    char *split_result[6];
    u8 split_count;
    char temp_buf[300] = {0};
    char echo_buf[32]  = {0};
    strncpy(temp_buf, recv_buf, sizeof(temp_buf) - 1);
    split_count = str_split(temp_buf, '|', split_result, 6);
    if (split_count != 6) {
        LED1_ON;
        return;
    }
    strncpy(g_weather.time_str, split_result[0], sizeof(g_weather.time_str) - 1);
    parse_time_str_to_realtime(split_result[0]);
    g_weather.temp       = (u8)atoi(split_result[1]);
    g_weather.feels_like = (u8)atoi(split_result[2]);
    g_weather.precip     = (u8)atoi(split_result[3]);
    g_weather.icons      = (u8)atoi(split_result[4]);
    g_weather.humidity   = (u8)atoi(split_result[5]);
    sprintf(echo_buf, "收到天气图标代码=%d\r\n", g_weather.icons);
    USART3_Senddata((uint8_t *)echo_buf, strlen(echo_buf));
    LED1_OFF;
}

// 在 LCD 上显示天气图标
void ShowWeatherIcon(u16 x, u16 y, u16 code)
{
    const u8 *icon_data = weather_icon_map[code];
    if (icon_data == NULL) return;
    LCD_DrawPicture(x, y, x + WEATHER_ICON_WIDTH, y + WEATHER_ICON_HEIGHT, (u8 *)icon_data);
}

#define LCD_BUF_SIZE 64
u8 lcd_show_buf[LCD_BUF_SIZE];

// 天气信息显示
void weather_lcd_show(void)
{
#define SCREEN_MARGIN_X  20
#define LINE_SPACING_Y   50
#define COLUMN_SPACING_X 200
#define BASE_Y           30

    static WeatherData g_last_weather = {0};
    if (memcmp(&g_weather, &g_last_weather, sizeof(WeatherData)) != 0) {
        LCD_Fill(0, BASE_Y, 479, BASE_Y + 4 * LINE_SPACING_Y, WHITE);
        memcpy(&g_last_weather, &g_weather, sizeof(WeatherData));
    }

    LCD_Fill(120, BASE_Y, 479, BASE_Y + 20, WHITE);
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "更新时间: %s", g_weather.time_str);
    LCD_ShowString(120, BASE_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "当前温度: %d℃", g_weather.temp);
    LCD_ShowString(SCREEN_MARGIN_X, BASE_Y + LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "体感温度: %d℃", g_weather.feels_like);
    LCD_ShowString(SCREEN_MARGIN_X + COLUMN_SPACING_X, BASE_Y + LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "空气湿度: %d%%", g_weather.humidity);
    LCD_ShowString(SCREEN_MARGIN_X, BASE_Y + 2 * LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "降水量: %d mm", g_weather.precip);
    LCD_ShowString(SCREEN_MARGIN_X + COLUMN_SPACING_X, BASE_Y + 2 * LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    const char *weather_name = WeatherCodeToName(g_weather.icons);
    if (g_weather.icons == 0) weather_name = "未获取天气";
    sprintf((char *)lcd_show_buf, "天气状况: %s", weather_name);
    LCD_ShowString(SCREEN_MARGIN_X, BASE_Y + 3 * LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    if (g_weather.icons != 0 && g_weather.icons != g_last_weather.icons) {
        ShowWeatherIcon(SCREEN_MARGIN_X + COLUMN_SPACING_X - 20, BASE_Y + 3 * LINE_SPACING_Y - 10, g_weather.icons);
    }

#undef SCREEN_MARGIN_X
#undef LINE_SPACING_Y
#undef COLUMN_SPACING_X
#undef BASE_Y
}

// 绘制 PAGE1 闹钟列表
void Draw_Page1_Alarm_List(void)
{
#define ALARM_LIST_X     20
#define ALARM_LIST_Y     220
#define ALARM_LINE_SPACE 35

    if (!g_alarm_data_changed) return;

    // 清空列表区域
    LCD_Fill(0, ALARM_LIST_Y, 479, ALARM_LIST_Y + BTN_CNT_PAGE1 * ALARM_LINE_SPACE + 20, WHITE);

    // 仅绘制 show=1 的闹钟，映射到页面按钮 1~4
    u8 show_idx = 0; // 对应 FUNC_ALARM_1~4
    for (u8 i = 0; i < MAX_ALARM_CNT; i++) {
        if (g_alarm_list[i].show != 1) continue;

        // 将 show_idx 映射到 FUNC_ALARM_1~4
        if (show_idx >= BTN_CNT_PAGE1) break;
        u16 bg_color = g_alarm_list[i].enable ? LIGHT_GREEN : LIGHT_GRAY;
        LCD_Fill(g_touch_btns_page1[show_idx].x1, g_touch_btns_page1[show_idx].y1, g_touch_btns_page1[show_idx].x2, g_touch_btns_page1[show_idx].y2, bg_color);

        memset(lcd_show_buf, 0, LCD_BUF_SIZE);
        sprintf((char *)lcd_show_buf, "%s  %02d:%02d [%s]", g_alarm_list[i].name, g_alarm_list[i].hour, g_alarm_list[i].minute, g_alarm_list[i].enable ? "已启用" : "已关闭");
        u16 text_color = g_alarm_list[i].enable ? DARK_BLUE : DARK_GRAY;
        LCD_ShowString(g_touch_btns_page1[show_idx].x1 + 10, g_touch_btns_page1[show_idx].y1 + 5, lcd_show_buf, text_color, bg_color);

        show_idx++;
    }
    
    g_alarm_data_changed = 0;

#undef ALARM_LIST_X
#undef ALARM_LIST_Y
#undef ALARM_LINE_SPACE
}

u8 g_selected_alarm = 1;
AlarmInfo g_edit_alarm;

// 绘制 PAGE2 闹钟设置界面
void Draw_Page2_Alarm_Set(void)
{
    // 1. 处理页面切换时的全屏刷新
    if (g_page2_last_page != PAGE_2) {
        LCD_Fill(0, 0, 479, 479, WHITE); // 清空屏幕
        g_page2_last_page = PAGE_2;
        g_page2_btn_drawn = 0; // 页面切换后重新绘制按钮
    }

    // 2. 显示标题
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "编辑闹钟: %s", g_edit_alarm.name);
    LCD_ShowString(120, 30, lcd_show_buf, RED, WHITE);

    // 3. 显示时间和开关状态
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "闹钟时间: %02d : %02d", g_edit_alarm.hour, g_edit_alarm.minute);
    LCD_ShowString(150, 90, lcd_show_buf, DARK_BLUE, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "当前状态: %s", g_edit_alarm.enable ? "启用" : "关闭");
    LCD_ShowString(150, 120, lcd_show_buf, BROWN, WHITE);

    // 4. 初始化绘制按钮
    if (!g_page2_btn_drawn) {
        for (u8 i = 0; i < BTN_CNT_PAGE2; i++) {
            LCD_Fill(g_touch_btns_page2[i].x1, g_touch_btns_page2[i].y1, g_touch_btns_page2[i].x2, g_touch_btns_page2[i].y2, LIGHT_BLUE);
            LCD_ShowString(g_touch_btns_page2[i].x1 + 10, g_touch_btns_page2[i].y1 + 5, (u8 *)g_touch_btns_page2[i].name, WHITE, LIGHT_BLUE);
        }
        g_page2_btn_drawn = 1;
    }
}

// 页面切换
void Page_Switch(u8 page)
{
    g_current_page = page;
    // 页面切换时清屏，避免残影
    LCD_Fill(0, 0, 479, 479, WHITE);
    delay_ms(10); // 留一点时间给 LCD 刷新

    if (page == PAGE_1) {
        // 返回 PAGE1 时重置 PAGE2 状态
        g_page2_last_page = PAGE_1;
        g_page2_btn_drawn = 0;
        // 绘制 PAGE1
        g_alarm_data_changed = 1;
        weather_lcd_show();
        Draw_Page1_Alarm_List();
        LCD_DrawPicture(10, 380, 457, 730, (u8 *)kokomi);
    } else if (page == PAGE_2) {
        // 进入 PAGE2 前先清屏
        LCD_Clear(WHITE);
        Alarm_Copy(&g_edit_alarm, &g_alarm_list[g_selected_alarm]);
        Draw_Page2_Alarm_Set();
    }
}

void Handle_Touch_Func(u8 func_id)
{
    switch (func_id) {
        case FUNC_ALARM_1:
            g_selected_alarm = 1; // 选择闹钟1并进入编辑
            Page_Switch(PAGE_2);
            break;
        case FUNC_ALARM_2:
            g_selected_alarm = 2; // 选择闹钟2并进入编辑
            Page_Switch(PAGE_2);
            break;
        case FUNC_ALARM_3:
            g_selected_alarm = 3; // 选择闹钟3并进入编辑
            Page_Switch(PAGE_2);
            break;
        case FUNC_ALARM_4:
            g_selected_alarm = 4; // 选择闹钟4并进入编辑
            Page_Switch(PAGE_2);
            break;
        case FUNC_HOUR_ADD:
            g_edit_alarm.hour = (g_edit_alarm.hour + 1) % 24;
            Draw_Page2_Alarm_Set();
            break;
        case FUNC_HOUR_SUB:
            g_edit_alarm.hour = (g_edit_alarm.hour - 1 + 24) % 24;
            Draw_Page2_Alarm_Set();
            break;
        case FUNC_MIN_ADD:
            g_edit_alarm.minute = (g_edit_alarm.minute + 1) % 60;
            Draw_Page2_Alarm_Set();
            break;
        case FUNC_MIN_SUB:
            g_edit_alarm.minute = (g_edit_alarm.minute - 1 + 60) % 60;
            Draw_Page2_Alarm_Set();
            break;
        case FUNC_ALARM_EN:
            g_edit_alarm.enable = !g_edit_alarm.enable;
            Draw_Page2_Alarm_Set();
            break;
        case FUNC_SAVE_ALARM:
            Alarm_Copy(&g_alarm_list[g_selected_alarm], &g_edit_alarm);
            g_alarm_data_changed = 1;
            Page_Switch(PAGE_1);
            break;
        case FUNC_CANCEL_ALARM:
            Page_Switch(PAGE_1);
            break;
        default:
            break;
    }
}

// 触摸扫描并匹配功能区域
void Touch_Scan_And_Match(void)
{
    u16 tx, ty;
    u8 current_touch_flag = 0;

    tp_dev.scan(0);
    for (u8 i = 0; i < 5; i++) {
        if (tp_dev.sta & (1 << i)) {
            tx = tp_dev.x[i];
            ty = tp_dev.y[i];
            if (tx < lcddev.width && ty < lcddev.height) {
                current_touch_flag = 1;
                break;
            }
        }
    }

    if (current_touch_flag) {
        g_touch_debounce_cnt++;
        if (g_touch_debounce_cnt < TOUCH_DEBOUNCE_TIME) return;
    } else {
        g_touch_debounce_cnt = 0;
    }

    if (current_touch_flag && !g_last_touch_flag) {
        if (g_current_page == PAGE_1) {
            for (u8 j = 0; j < BTN_CNT_PAGE1; j++) {
                if (tx >= g_touch_btns_page1[j].x1 && tx <= g_touch_btns_page1[j].x2 && ty >= g_touch_btns_page1[j].y1 && ty <= g_touch_btns_page1[j].y2) {
                    Handle_Touch_Func(g_touch_btns_page1[j].func_id);
                    break;
                }
            }
        } else if (g_current_page == PAGE_2) {
            for (u8 j = 0; j < BTN_CNT_PAGE2; j++) {
                if (tx >= g_touch_btns_page2[j].x1 && tx <= g_touch_btns_page2[j].x2 && ty >= g_touch_btns_page2[j].y1 && ty <= g_touch_btns_page2[j].y2) {
                    Handle_Touch_Func(g_touch_btns_page2[j].func_id);
                    break;
                }
            }
        }
    }
    g_last_touch_flag = current_touch_flag;
}

// 闹钟0：定期请求天气
void Alarm0_Callback(void)
{
    USART3_Senddata((uint8_t *)" GET_WEATHER", 12); // 通过串口请求天气数据
}

void Alarm1_Callback(void)
{
    for (u8 i = 0; i < 3; i++) {
        LED2_ON;
        delay_ms(200);
        LED2_OFF;
        delay_ms(200);
    }
    BEEP_ON;
}
volatile u32 g_systick_ms_counter = 0;
u8 g_time_updated_flag            = 0;
// 标记是否需要刷新时间 UI（仅 PAGE1 刷新）
u8 g_need_refresh_time_ui = 0;
volatile u8 Update_Flag   = 0;
// 主函数
int main(void)
{
    EXTI_Config();
    SysTick_Init();
    LCD_Init();
    UART3_Configuration();

    // 1. 初始化 Timer (TIM10, 10ms 中断)
    TIM10_TimeSliceInit();
    AlarmManager_Init();

    LEDGpio_Init();
    KEYGpio_Init();
    tp_dev.init();
    LCD_Clear(WHITE);

    // 初始化天气默认值
    memset(&g_weather, 0, sizeof(WeatherData));
    strncpy(g_weather.time_str, "2025-12-27 00:00:00", sizeof(g_weather.time_str) - 1);
    g_weather.temp       = 25;
    g_weather.precip     = 0;
    g_weather.feels_like = 26;
    g_weather.icons      = 100;
    g_weather.humidity   = 60;

    g_real_time.hour   = 0;
    g_real_time.minute = 0;
    g_real_time.second = 0;

    // 初始化 UI 默认闹钟（Alarm0 为占位）
    Alarm_Set(1, 7, 30, 0, 1, Alarm1_Callback, "闹钟1");
    Alarm_Set(2, 0, 0, 1, 1, Alarm1_Callback, "闹钟2");
    Alarm_Set(3, 12, 30, 1, 1, Alarm1_Callback, "闹钟3");
    Alarm_Set(4, 18, 0, 0, 1, Alarm1_Callback, "闹钟4");

    // 2. 注册周期闹钟（定期拉取天气）
    // 5分钟 = 300秒 = 30000 个 10ms 周期
    // ALARM_REPEAT 表示循环触发
    Alarm_Register(30000, ALARM_REPEAT, Alarm0_Callback);

    Alarm_Copy(&g_edit_alarm, &g_alarm_list[1]);
    Page_Switch(PAGE_1);
    LED2_OFF;

    u16 refresh_count        = 0;
    u16 timer_1s_accumulator = 0; // 累计 10ms 节拍到 1s

    // 首次上电主动请求一次天气
    USART3_Senddata((uint8_t *)" GET_WEATHER", 12);



    while (1) {
        // ============== 定时片任务（10ms） ==============
        // 3. 定时心跳任务
        // Update_Flag 由 TIM1_UP_TIM10_IRQHandler 置 1（周期 10ms）
        if (Update_Flag == 1) {
            Update_Flag = 0; // 清零标志，避免重复处理

            // 累计 10ms 到 1s
            timer_1s_accumulator++;

            // 约 76 次（≈1s）后更新时间与闹钟
            if (timer_1s_accumulator >= 76) {
                timer_1s_accumulator = 0;

                // 更新时间计数
                g_real_time.second++;
                if (g_real_time.second >= 60) {
                    g_real_time.second = 0;
                    g_real_time.minute++;
                    if (g_real_time.minute >= 60) {
                        g_real_time.minute = 0;
                        g_real_time.hour++;
                        if (g_real_time.hour >= 24) g_real_time.hour = 0;
                    }
                }

                // 刷新时间字符串
                format_real_time_to_str();

                // 检查闹钟触发
                Alarm_CheckAndTrigger();

                // 在 PAGE1 时刷新时间 UI
                if (g_current_page == PAGE_1) {
                    g_need_refresh_time_ui = 1;
                }
            }

            // 调度闹钟定时器队列
            AlarmTimer_Process();
        }

        // ============== 其他事件处理 ==============
        // 外部中断标志（蜂鸣器关闭）
        if (Int_flag == 1) {
            delay_ms(20);
            if (GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_8) == 0) {
                BEEP_OFF;
            }
            Int_flag = 0;
        }

        Touch_Scan_And_Match();

        // 串口接收完成
        if (Uart3.ReceiveFinish == 1) {
            // 补上字符串结束符
            if (Uart3.RXlenth < 300)
                Uart3.Rxbuf[Uart3.RXlenth] = '\0';
            else
                Uart3.Rxbuf[299] = '\0';
            weather_parse((char *)Uart3.Rxbuf);
            weather_lcd_show();
            refresh_count       = 0;
            Uart3.ReceiveFinish = 0;
            Uart3.RXlenth       = 0;
            Uart3.Time          = 0;
            LED2_ON;
            delay_ms(200);
            LED2_OFF;
        }

        // 更新时间 UI（仅 PAGE1）
        if (g_need_refresh_time_ui == 1) {
            weather_lcd_show();
            g_need_refresh_time_ui = 0; // 已更新
            
        }

        refresh_count++;
        if (refresh_count >= REFRESH_THRESHOLD && g_current_page == PAGE_1) {
            if (g_weather.icons != 0) weather_lcd_show();
            Draw_Page1_Alarm_List();
            refresh_count = 0;
        }
    }
}