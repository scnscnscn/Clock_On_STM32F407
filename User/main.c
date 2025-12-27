#include "stm32f4xx.h"
#include "USART.h"
#include "delay.h"
#include "DEVICE.h"
#include "lcd.h"
#include"Timer.h"
#include "string.h"
#include "stdlib.h"
#include "weather_icons.h"
#include "touch.h"
#include "KEY.h"
#include "EXIT.h"
// ====================== 颜色常量定义 ======================
#ifndef LIGHT_GREEN
#define LIGHT_GREEN 0x07E0
#endif
#ifndef LIGHT_GRAY
#define LIGHT_GRAY 0xD69A
#endif
#ifndef DARK_BLUE
#define DARK_BLUE 0x001F
#endif
#ifndef DARK_GRAY
#define DARK_GRAY 0x7BEF
#endif
#ifndef LIGHT_BLUE
#define LIGHT_BLUE 0x877F
#endif
#ifndef WHITE
#define WHITE 0xFFFF
#endif
#ifndef RED
#define RED 0xF800
#endif
#ifndef BROWN
#define BROWN 0xBC40
#endif
#ifndef BLACK
#define BLACK 0x0000
#endif

// ====================== 函数声明 ======================
void Alarm0_Callback(void);
void Alarm1_Callback(void);
void Page_Switch(u8 page);
void Handle_Touch_Func(u8 func_id);
void Draw_Page1_Alarm_List(void);
void Draw_Page2_Alarm_Set(void);

// ====================== 页面与功能枚举 ======================
#define PAGE_1 0
#define PAGE_2 1
u8 g_current_page = PAGE_1;

// 注意：FUNC_ALARM_1~4 对应显示的闹钟1~4，天气请求闹钟无对应func_id（不显示）
#define FUNC_ALARM_1      0
#define FUNC_ALARM_2      1
#define FUNC_ALARM_3      2
#define FUNC_ALARM_4      3
#define FUNC_HOUR_ADD     4
#define FUNC_HOUR_SUB     5
#define FUNC_MIN_ADD      6
#define FUNC_MIN_SUB      7
#define FUNC_ALARM_EN     8
#define FUNC_SAVE_ALARM   9
#define FUNC_CANCEL_ALARM 10

// ====================== 全局状态变量（核心解决残留+闪烁+误触） ======================
u8 g_last_touch_flag          = 0;
u8 g_alarm_data_changed       = 0;
u32 g_touch_debounce_cnt      = 0;
const u16 REFRESH_THRESHOLD   = 50;
const u16 TOUCH_DEBOUNCE_TIME = 2;

// ====================== PAGE2绘制标记（必须全局，否则无法重置） ======================
u8 g_page2_last_page = PAGE_1; // 全局变量，记录PAGE2上次页面状态
u8 g_page2_btn_drawn = 0;      // 全局变量，记录PAGE2按键是否绘制

// 天气编码转名称
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

extern u8 Update_Flag;

// 字符串分割函数声明
u8 str_split(char *src, char delim, char **result, u8 max_len);

// 实时时间结构体
typedef struct {
    u8 hour;
    u8 minute;
    u8 second;
} RealTime;
RealTime g_real_time = {0};

// 闹钟结构体（新增：show字段，1=显示在列表，0=隐藏不显示）
#define MAX_ALARM_CNT 5 // 0=天气请求（隐藏），1~4=闹钟1~4（显示）
typedef struct {
    u8 hour;
    u8 minute;
    u8 enable;
    u8 show; // 新增：是否显示在闹钟列表 1=显示 0=隐藏
    void (*callback)(void);
    char name[16];
} AlarmInfo;

// 初始化：0=天气请求（隐藏），1~4=闹钟1~4（显示，命名简化为闹钟1/2/3/4）
AlarmInfo g_alarm_list[MAX_ALARM_CNT] = {
    {0, 5, 1, 0, Alarm0_Callback, "天气请求"}, // 隐藏：每5分钟发送一次天气请求
    {7, 30, 0, 1, Alarm1_Callback, "闹钟1"},   // 显示：闹钟1
    {0, 0, 1, 1, Alarm1_Callback, "闹钟2"},    // 显示：闹钟2
    {12, 30, 1, 1, Alarm1_Callback, "闹钟3"},  // 显示：闹钟3
    {18, 0, 0, 1, Alarm1_Callback, "闹钟4"}    // 显示：闹钟4
};

u8 g_selected_alarm = 1; // 默认选中显示的第一个闹钟（闹钟1）
AlarmInfo g_edit_alarm;

// 触摸按键结构体（仅对应显示的闹钟1~4，移除原闹钟5）
typedef struct {
    u16 x1;
    u16 y1;
    u16 x2;
    u16 y2;
    u8 func_id;
    char name[16];
} Touch_Button_t;

// 触摸按键仅绑定显示的闹钟1~4，名称对应简化
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
    {180, 250, 300, 280, FUNC_ALARM_EN, "启用/禁用"},
    {80, 320, 220, 350, FUNC_SAVE_ALARM, "保存"},
    {260, 320, 400, 350, FUNC_CANCEL_ALARM, "取消"},
};

#define BTN_CNT_PAGE1 (sizeof(g_touch_btns_page1) / sizeof(Touch_Button_t))
#define BTN_CNT_PAGE2 (sizeof(g_touch_btns_page2) / sizeof(Touch_Button_t))

// 闹钟设置函数（新增：show参数，设置是否显示）
u8 Alarm_Set(u8 alarm_idx, u8 hour, u8 minute, u8 enable, u8 show, void (*callback)(void), const char *name)
{
    if (alarm_idx >= MAX_ALARM_CNT) return 0;
    g_alarm_list[alarm_idx].hour     = hour;
    g_alarm_list[alarm_idx].minute   = minute;
    g_alarm_list[alarm_idx].enable   = enable;
    g_alarm_list[alarm_idx].show     = show; // 赋值是否显示标记
    g_alarm_list[alarm_idx].callback = callback;
    if (name != NULL) {
        strncpy(g_alarm_list[alarm_idx].name, name, sizeof(g_alarm_list[alarm_idx].name) - 1);
    }
    g_alarm_data_changed = 1;
    return 1;
}

// 闹钟触发检查（不变，仍会触发隐藏的天气请求闹钟）
void Alarm_CheckAndTrigger(void)
{
    static u8 last_minute = 0xFF;
    if (g_real_time.minute == last_minute) return;
    last_minute = g_real_time.minute;
    for (u8 i = 0; i < MAX_ALARM_CNT; i++) {
        // 隐藏闹钟仅不显示，触发逻辑正常（enable=1即触发）
        if (g_alarm_list[i].enable && g_alarm_list[i].hour == g_real_time.hour && g_alarm_list[i].minute == g_real_time.minute && g_alarm_list[i].callback != NULL) {
            g_alarm_list[i].callback();
            g_alarm_data_changed = 1;
        }
    }
}

// 闹钟数据复制（新增：复制show字段）
void Alarm_Copy(AlarmInfo *dst, const AlarmInfo *src)
{
    if (dst == NULL || src == NULL) return;
    dst->hour     = src->hour;
    dst->minute   = src->minute;
    dst->enable   = src->enable;
    dst->show     = src->show; // 复制显示标记
    dst->callback = src->callback;
    strncpy(dst->name, src->name, sizeof(dst->name) - 1);
}

// 气象数据结构体
typedef struct {
    char time_str[25];
    u8 temp;
    u8 feels_like;
    u8 precip;
    u8 icons;
    u8 humidity;
} WeatherData;

WeatherData g_weather;

// 格式化实时时间
void format_real_time_to_str(void)
{
    static char date_part[12] = "2025-12-27";
    sprintf((char *)g_weather.time_str, "%s %02d:%02d:%02d", date_part, g_real_time.hour, g_real_time.minute, g_real_time.second);
}

// 解析时间字符串
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

// 字符串分割函数
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

// 气象数据解析
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
    sprintf(echo_buf, "收到icons：%d\r\n", g_weather.icons);
    USART3_Senddata((uint8_t *)echo_buf, strlen(echo_buf));
    LED1_OFF;
}

// 显示天气图标
void ShowWeatherIcon(u16 x, u16 y, u16 code)
{
    const u8 *icon_data = weather_icon_map[code];
    if (icon_data == NULL) return;
    LCD_DrawPicture(x, y, x + WEATHER_ICON_WIDTH, y + WEATHER_ICON_HEIGHT, (u8 *)icon_data);
}

#define LCD_BUF_SIZE 64
u8 lcd_show_buf[LCD_BUF_SIZE];

// 天气显示
void weather_lcd_show(void)
{
#define SCREEN_MARGIN_X  20
#define LINE_SPACING_Y   50
#define COLUMN_SPACING_X 200
#define BASE_Y           30

    static WeatherData g_last_weather = {0};
    // 仅气象数据（温度、湿度等）变化时，清屏并更新g_last_weather（保留原有优化）
    if (memcmp(&g_weather, &g_last_weather, sizeof(WeatherData)) != 0) {
        LCD_Fill(0, BASE_Y, 479, BASE_Y + 4 * LINE_SPACING_Y, WHITE);
        memcpy(&g_last_weather, &g_weather, sizeof(WeatherData));
    }

    // 单独绘制时间，不受memcmp限制，确保按秒刷新
    LCD_Fill(120, BASE_Y, 479, BASE_Y + 20, WHITE);
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "当前时间：%s", g_weather.time_str);
    LCD_ShowString(120, BASE_Y, lcd_show_buf, BLACK, WHITE);

    // 以下为原有气象数据绘制逻辑，保持不变
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "当前温度：%d摄氏度", g_weather.temp);
    LCD_ShowString(SCREEN_MARGIN_X, BASE_Y + LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "体感温度：%d摄氏度", g_weather.feels_like);
    LCD_ShowString(SCREEN_MARGIN_X + COLUMN_SPACING_X, BASE_Y + LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "环境湿度：%d%%", g_weather.humidity);
    LCD_ShowString(SCREEN_MARGIN_X, BASE_Y + 2 * LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "降水量：%dmm", g_weather.precip);
    LCD_ShowString(SCREEN_MARGIN_X + COLUMN_SPACING_X, BASE_Y + 2 * LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    const char *weather_name = WeatherCodeToName(g_weather.icons);
    if (g_weather.icons == 0) weather_name = "未获取数据";
    sprintf((char *)lcd_show_buf, "当前天气：%s", weather_name);
    LCD_ShowString(SCREEN_MARGIN_X, BASE_Y + 3 * LINE_SPACING_Y, lcd_show_buf, BLACK, WHITE);

    if (g_weather.icons != 0 && g_weather.icons != g_last_weather.icons) {
        ShowWeatherIcon(SCREEN_MARGIN_X + COLUMN_SPACING_X - 20, BASE_Y + 3 * LINE_SPACING_Y - 10, g_weather.icons);
    }

#undef SCREEN_MARGIN_X
#undef LINE_SPACING_Y
#undef COLUMN_SPACING_X
#undef BASE_Y
}

// 绘制PAGE1闹钟列表（核心修改：仅绘制show=1的闹钟，过滤隐藏的天气请求闹钟）
void Draw_Page1_Alarm_List(void)
{
#define ALARM_LIST_X     20
#define ALARM_LIST_Y     220
#define ALARM_LINE_SPACE 35

    if (!g_alarm_data_changed) return;

    // 清空闹钟列表区域
    LCD_Fill(0, ALARM_LIST_Y, 479, ALARM_LIST_Y + BTN_CNT_PAGE1 * ALARM_LINE_SPACE + 20, WHITE);

    // 绘制列表标题
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "===== 闹钟列表（点击编辑）=====");
    LCD_ShowString(80, ALARM_LIST_Y, lcd_show_buf, RED, WHITE);

    // 遍历闹钟列表，仅绘制show=1的闹钟（与触摸按键对应）
    u8 show_idx = 0; // 显示索引（对应触摸按键1~4）
    for (u8 i = 0; i < MAX_ALARM_CNT; i++) {
        if (g_alarm_list[i].show != 1) continue; // 过滤隐藏闹钟（天气请求）

        // 匹配触摸按键（show_idx对应FUNC_ALARM_1~4）
        if (show_idx >= BTN_CNT_PAGE1) break;
        u16 bg_color = g_alarm_list[i].enable ? LIGHT_GREEN : LIGHT_GRAY;
        LCD_Fill(g_touch_btns_page1[show_idx].x1, g_touch_btns_page1[show_idx].y1, g_touch_btns_page1[show_idx].x2, g_touch_btns_page1[show_idx].y2, bg_color);

        memset(lcd_show_buf, 0, LCD_BUF_SIZE);
        sprintf((char *)lcd_show_buf, "%s：%02d:%02d [%s]", g_alarm_list[i].name, g_alarm_list[i].hour, g_alarm_list[i].minute, g_alarm_list[i].enable ? "启用" : "禁用");
        u16 text_color = g_alarm_list[i].enable ? DARK_BLUE : DARK_GRAY;
        LCD_ShowString(g_touch_btns_page1[show_idx].x1 + 10, g_touch_btns_page1[show_idx].y1 + 5, lcd_show_buf, text_color, bg_color);

        show_idx++;
    }

    g_alarm_data_changed = 0;

#undef ALARM_LIST_X
#undef ALARM_LIST_Y
#undef ALARM_LINE_SPACE
}

// 绘制PAGE2设置界面（保持不变，仅显示选中闹钟的名称）
void Draw_Page2_Alarm_Set(void)
{
    // 1. 页面切换时强制清屏（彻底清除PAGE2内容）
    if (g_page2_last_page != PAGE_2) {
        LCD_Fill(0, 0, 479, 479, WHITE); // 全屏清屏
        g_page2_last_page = PAGE_2;
        g_page2_btn_drawn = 0; // 切换页面后重置按键绘制标记
    }

    // 2. 显示标题（对应简化后的闹钟名称）
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "编辑闹钟：%s", g_edit_alarm.name);
    LCD_ShowString(120, 30, lcd_show_buf, RED, WHITE);

    // 3. 显示时间和状态
    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "当前设置：%02d : %02d", g_edit_alarm.hour, g_edit_alarm.minute);
    LCD_ShowString(150, 90, lcd_show_buf, DARK_BLUE, WHITE);

    memset(lcd_show_buf, 0, LCD_BUF_SIZE);
    sprintf((char *)lcd_show_buf, "当前状态：%s", g_edit_alarm.enable ? "启用" : "禁用");
    LCD_ShowString(150, 120, lcd_show_buf, BROWN, WHITE);

    // 4. 仅首次进入页面时绘制按键
    if (!g_page2_btn_drawn) {
        for (u8 i = 0; i < BTN_CNT_PAGE2; i++) {
            LCD_Fill(g_touch_btns_page2[i].x1, g_touch_btns_page2[i].y1, g_touch_btns_page2[i].x2, g_touch_btns_page2[i].y2, LIGHT_BLUE);
            LCD_ShowString(g_touch_btns_page2[i].x1 + 10, g_touch_btns_page2[i].y1 + 5, (u8 *)g_touch_btns_page2[i].name, WHITE, LIGHT_BLUE);
        }
        g_page2_btn_drawn = 1;
    }
}

// 页面切换函数（保持不变）
void Page_Switch(u8 page)
{
    g_current_page = page;
    // 关键：无论切换到哪个页面，先全屏清屏，彻底清除残留
    LCD_Fill(0, 0, 479, 479, WHITE);
    delay_ms(10); // 延时确保清屏完成

    if (page == PAGE_1) {
        // 重置PAGE2所有标记，下次进入PAGE2时重新绘制
        g_page2_last_page = PAGE_1;
        g_page2_btn_drawn = 0;
        // 绘制PAGE1内容
        g_alarm_data_changed = 1;
        weather_lcd_show();
        Draw_Page1_Alarm_List();
    } else if (page == PAGE_2) {
        // 进入PAGE2，复制闹钟数据
        Alarm_Copy(&g_edit_alarm, &g_alarm_list[g_selected_alarm]);
        Draw_Page2_Alarm_Set();
    }
}

// 触摸功能处理（核心修改：对应简化后的闹钟1~4，无天气请求闹钟的触摸入口）
void Handle_Touch_Func(u8 func_id)
{
    switch (func_id) {
        case FUNC_ALARM_1:
            g_selected_alarm = 1; // 对应闹钟1（索引1）
            Page_Switch(PAGE_2);
            break;
        case FUNC_ALARM_2:
            g_selected_alarm = 2; // 对应闹钟2（索引2）
            Page_Switch(PAGE_2);
            break;
        case FUNC_ALARM_3:
            g_selected_alarm = 3; // 对应闹钟3（索引3）
            Page_Switch(PAGE_2);
            break;
        case FUNC_ALARM_4:
            g_selected_alarm = 4; // 对应闹钟4（索引4）
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

// 触摸扫描与匹配（保持不变）
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

// 回调函数（天气请求回调保持不变，自动触发无界面）
void Alarm0_Callback(void)
{
    USART3_Senddata((uint8_t *)" GET_WEATHER", 12); // 自动发送天气请求，无界面显示
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

// 主函数（修改：Alarm_Set调用新增show参数，对应显示/隐藏）
int main(void)
{
    EXTI_Config();
    SysTick_Init();
    LCD_Init();
    UART3_Configuration();
    AlarmManager_Init();
    TIM10_TimeSliceInit();
    LEDGpio_Init();
    KEYGpio_Init();
    tp_dev.init();
    LCD_Clear(WHITE);

    memset(&g_weather, 0, sizeof(WeatherData));
    strncpy(g_weather.time_str, "2025-12-27 00:00:00", sizeof(g_weather.time_str) - 1);
    g_weather.temp       = 25;
    g_weather.feels_like = 26;
    g_weather.precip     = 0;
    g_weather.icons      = 100;
    g_weather.humidity   = 60;

    g_real_time.hour   = 0;
    g_real_time.minute = 0;
    g_real_time.second = 0;
    format_real_time_to_str();

    // 初始化闹钟：新增show参数，0=隐藏（天气请求），1=显示（闹钟1~4）
    Alarm_Set(0, 0, 5, 1, 0, Alarm0_Callback, "天气请求"); // 隐藏：每5分钟发送天气请求
    Alarm_Set(1, 7, 30, 0, 1, Alarm1_Callback, "闹钟1");   // 显示：闹钟1
    Alarm_Set(2, 0, 0, 1, 1, Alarm1_Callback, "闹钟2");    // 显示：闹钟2
    Alarm_Set(3, 12, 30, 1, 1, Alarm1_Callback, "闹钟3");  // 显示：闹钟3
    Alarm_Set(4, 18, 0, 0, 1, Alarm1_Callback, "闹钟4");   // 显示：闹钟4

    Alarm_Copy(&g_edit_alarm, &g_alarm_list[1]); // 默认复制闹钟1的数据

    Page_Switch(PAGE_1);
    LED2_OFF;

    u16 refresh_count = 0;
    u16 second_count  = 0;
    USART3_Senddata((uint8_t *)" GET_WEATHER", 12); // 启动时发送一次天气请求
    while (1) {
        if (Int_flag == 1) {
            delay_ms(20);
            if (GPIO_ReadOutputDataBit(GPIOF, GPIO_Pin_8) == 0) {
                BEEP_OFF;
            }

            Int_flag = 0;
        }
        Touch_Scan_And_Match();

        if (Update_Flag == 1) {
            AlarmTimer_Process();
            Update_Flag = 0;
        }

        if (Uart3.ReceiveFinish == 1) {
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
            delay_ms(500);
            LED2_OFF;
        }

        refresh_count++;
        if (refresh_count >= REFRESH_THRESHOLD && g_current_page == PAGE_1) {
            if (g_weather.icons != 0) weather_lcd_show();
            Draw_Page1_Alarm_List();
            refresh_count = 0;
        }
        second_count++;
        if (second_count >= 10) {
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
            format_real_time_to_str();
            Alarm_CheckAndTrigger();
            // 时间更新后立即重绘天气界面
            if (g_current_page == PAGE_1) {
                weather_lcd_show();
            }
            second_count = 0;
        }

        delay_ms(100);
    }
}