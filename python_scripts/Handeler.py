# -*- coding: utf-8 -*-
"""
整合功能：
1. 串口通信：监听单片机的天气请求信号（GET_WEATHER）
2. 天气API：调用接口获取最新天气数据，存储到JSON文件
3. 数据传输：将格式化后的天气数据通过串口发送给单片机
依赖库：pyserial, requests, pyjwt, python-dotenv, cryptography
环境配置：需创建.env文件，放置API相关配置；需准备ed25519-private.pem私钥文件
"""
import os
import re
import json
import time
import serial
import sys
import jwt
import requests
from dotenv import load_dotenv
from typing import Optional

# ====================== 路径与环境变量初始化（核心修改） ======================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
load_dotenv(os.path.join(BASE_DIR, ".env"))

# ====================== 全局配置 ======================
# 串口配置
SERIAL_PORT = os.getenv("SERIAL_PORT", "COM3")
BAUDRATE = int(os.getenv("BAUDRATE", 115200))
TIMEOUT = 2
REQ_SIGNAL_STR = "GET_WEATHER"
POLL_INTERVAL = 0.01
ALLOWED_SUFFIXES = ("\r\n", "\n", "\r")

# 天气API与JSON配置
JSON_FILE = os.path.join(BASE_DIR, "hangzhou_weather_history.json")
ENCODING = "utf-8"
PRIVATE_KEY_FILE = os.path.join(BASE_DIR, "ed25519-private.pem")

# 全局变量
ser: Optional[serial.Serial] = None


# ====================== 串口通信模块（核心修改：增强打印） ======================
def init_serial() -> bool:
    """初始化串口，失败则返回False"""
    global ser
    try:
        if ser is not None and ser.is_open:
            ser.close()

        ser = serial.Serial(
            port=SERIAL_PORT,
            baudrate=BAUDRATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=TIMEOUT,
        )
        if ser.is_open:
            print(f"✅ 串口初始化成功：{SERIAL_PORT}（波特率：{BAUDRATE}）")
            return True
    except serial.SerialException as e:
        print(f"❌ 串口初始化失败：{e}")
        print("请检查：1.COM口是否正确 2.串口是否被其他程序占用 3.单片机是否正常连接")
    except Exception as e:
        print(f"❌ 串口未知错误：{e}")
    return False


def check_request_signal(recv_str: str) -> bool:
    """检查接收的字符串是否为单片机的有效请求信号"""
    for suffix in ALLOWED_SUFFIXES:
        if recv_str.endswith(suffix):
            recv_str = recv_str[: -len(suffix)]
            break
    return recv_str.strip() == REQ_SIGNAL_STR


def send_data_to_mcu(data: str) -> bool:
    """向单片机发送数据（带帧尾\r\n）"""
    global ser
    if ser is None or not ser.is_open:
        print("❌ 串口未打开，发送失败")
        return False

    try:
        send_data = data + "\r\n"
        ser.write(send_data.encode(ENCODING))
        print(f"\n📤 已向单片机发送数据：{send_data.strip()}")
        return True
    except serial.SerialException as e:
        print(f"❌ 串口发送失败：{e}")
    except UnicodeEncodeError as e:
        print(f"❌ 数据编码失败（请确保为UTF-8）：{e}")
    except Exception as e:
        print(f"❌ 发送异常：{e}")
    return False


# ====================== 天气API与JSON模块（无修改） ======================
def load_private_key(file_path: str = PRIVATE_KEY_FILE) -> str:
    """加载Ed25519私钥文件"""
    key_path = (
        os.path.join(BASE_DIR, file_path) if not os.path.isabs(file_path) else file_path
    )
    if not os.path.exists(key_path):
        raise FileNotFoundError(f"私钥文件 {key_path} 不存在")
    with open(key_path, "r", encoding=ENCODING) as f:
        return f.read().strip()


def generate_jwt(private_key: str) -> str:
    """生成JWT令牌"""
    try:
        headers = {"alg": "EdDSA", "kid": os.getenv("JWT_KID"), "typ": None}
        now = int(time.time())
        payload = {
            "sub": os.getenv("PROJECT_ID"),
            "iat": now - 30,
            "exp": now + int(os.getenv("JWT_EXPIRE", 86000)),
        }
        if not all([headers["kid"], payload["sub"]]):
            raise ValueError("环境变量JWT_KID/PROJECT_ID未配置")
        return jwt.encode(payload, private_key, algorithm="EdDSA", headers=headers)
    except Exception as e:
        raise Exception(f"JWT生成失败：{e}")


def extract_city_name(fx_link: str) -> str:
    """从fx_link中提取城市名称"""
    city_en = re.search(r"weather/([a-zA-Z]+)-\d+", fx_link).group(1)
    return {"hangzhou": "杭州"}.get(city_en, city_en)


def get_weather_from_api() -> dict:
    """调用天气API获取最新数据"""
    try:
        private_key = load_private_key()
        jwt_token = generate_jwt(private_key)

        url = f"https://{os.getenv('API_HOST')}/v7/weather/now"
        headers = {"Authorization": f"Bearer {jwt_token}"}
        params = {
            "location": os.getenv("LOCATION_ID", "101210101"),
            "lang": "en",
        }
        response = requests.get(url, headers=headers, params=params, timeout=10)
        response.raise_for_status()

        print("\n" + "=" * 60)
        print("接口回传的原始响应数据：")
        print("=" * 60)
        try:
            raw_data = response.json()
            print(json.dumps(raw_data, ensure_ascii=False, indent=4))
        except json.JSONDecodeError:
            print(f"非JSON响应：{response.text}")
        print("=" * 60 + "\n")

        data = response.json()
        weather_info = {
            "record_time": time.strftime("%Y-%m-%d %H:%M:%S"),
            "temp": data["now"]["temp"],
            "feels_like": data["now"]["feelsLike"],
            "precip": data["now"]["precip"],
            "icons": data["now"]["icon"],
            "humidity": data["now"].get("humidity", "0"),
        }
        print("\n🌤 从API获取最新天气数据（精简版）：")
        for k, v in weather_info.items():
            print(f"  {k.replace('_', ' ')}：{v}")
        return weather_info
    except requests.exceptions.RequestException as e:
        raise Exception(f"API请求失败：{e}")
    except KeyError as e:
        raise Exception(f"API返回数据格式错误，缺失字段：{e}")
    except Exception as e:
        raise Exception(f"获取天气数据失败：{e}")


def save_weather_to_json(weather_data: dict) -> None:
    """将天气数据保存到JSON文件"""
    json_file = os.path.join(BASE_DIR, "hangzhou_weather_history.json")
    if os.path.exists(json_file):
        with open(json_file, "r", encoding=ENCODING) as f:
            history_data = json.load(f)
        if not isinstance(history_data, list):
            history_data = []
    else:
        history_data = []

    history_data.append(weather_data)
    with open(json_file, "w", encoding=ENCODING) as f:
        json.dump(history_data, f, ensure_ascii=False, indent=4)
    print(f"💾 天气数据已保存到 {json_file}（共{len(history_data)}条记录）")


def format_weather_data(weather_data: dict) -> str:
    """格式化天气数据为单片机可解析的竖线分隔字符串"""
    # 注意：修正字段顺序！原脚本顺序是 时间|温度|体感|图标|湿度|降水量
    # 对应单片机解析的：0时间 1温度 2体感 3降水量 4图标 5湿度 → 这里顺序错误，是核心问题！
    data_str = (
        f"{weather_data['record_time']}|"  # 0: 时间
        f"{weather_data['temp']}|"  # 1: 温度
        f"{weather_data['feels_like']}|"  # 2: 体感温度
        f"{weather_data['precip']}|"  # 3: 降水量（原脚本错把icons放这）
        f"{weather_data['icons']}|"  # 4: 图标（原脚本错把humidity放这）
        f"{weather_data['humidity']}"  # 5: 湿度（原脚本错把precip放这）
    )
    return data_str


# ====================== 核心业务逻辑（无修改） ======================
def process_mcu_request() -> None:
    """处理单片机的天气请求：API获取→存JSON→格式化发送"""
    try:
        weather_data = get_weather_from_api()
        save_weather_to_json(weather_data)
        formatted_data = format_weather_data(weather_data)
        send_data_to_mcu(formatted_data)
    except Exception as e:
        print(f"\n❌ 处理请求失败：{e}")
        send_data_to_mcu("ERROR:获取天气数据失败")


def listen_mcu_request() -> None:
    """持续监听单片机的请求信号（核心修改：显示所有串口数据）"""
    print(f"\n🔍 开始监听单片机请求（信号：{REQ_SIGNAL_STR}），按Ctrl+C退出")
    print(f"📢 实时显示所有串口接收数据（波特率：{BAUDRATE}）")
    print("-" * 80)
    while True:
        try:
            # 串口断连重连
            if ser is None or not ser.is_open:
                print("\n🔌 串口断开，尝试重新连接...")
                if not init_serial():
                    time.sleep(1)
                    continue

            # 读取所有缓冲区数据（核心：显示每一条串口信息）
            if ser.in_waiting > 0:
                # 1. 读取原始字节并打印（避免转义丢失）
                recv_bytes = ser.read(ser.in_waiting)
                print(f"\n📥 【原始字节】：{repr(recv_bytes)}")

                # 2. 解码为字符串（忽略无效字符）
                recv_str = recv_bytes.decode(ENCODING, errors="ignore")
                print(f"📥 【解码后】：{repr(recv_str)}")  # repr保留换行/回车符

                # 3. 处理并检测请求信号
                clean_recv_str = recv_str.strip()
                print(f"📥 【处理后】：{clean_recv_str}")

                # 4. 区分有效请求和普通数据
                if check_request_signal(recv_str):
                    print(f"\n✅ 检测到有效请求信号：{REQ_SIGNAL_STR}，开始处理...")
                    process_mcu_request()
                elif clean_recv_str:  # 非空普通数据
                    print(f"ℹ️  收到普通串口数据：{clean_recv_str}（非请求信号）")
                else:  # 空数据（仅换行/回车）
                    print(f"ℹ️  收到空数据（仅换行/回车符）")

                print("-" * 80)  # 分隔线，便于区分每条数据

            time.sleep(POLL_INTERVAL)

        except KeyboardInterrupt:
            print("\n\n🛑 用户中断程序")
            break
        except Exception as e:
            print(f"\n❌ 监听异常：{e}")
            print("-" * 80)
            time.sleep(1)


# ====================== 主函数（无修改） ======================
def main() -> None:
    """程序入口：初始化串口→监听请求"""
    print(f"📂 脚本所在目录：{BASE_DIR}")
    print(f"🔑 私钥文件路径：{PRIVATE_KEY_FILE}")
    print(f"🔧 .env文件路径：{os.path.join(BASE_DIR, '.env')}")

    if not init_serial():
        sys.exit(1)

    try:
        listen_mcu_request()
    finally:
        if ser is not None and ser.is_open:
            ser.close()
            print(f"\n🔌 串口 {SERIAL_PORT} 已关闭")
        sys.exit(0)


if __name__ == "__main__":
    main()
