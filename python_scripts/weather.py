import os
import re
import time
import json
import requests
import jwt
from dotenv import load_dotenv

load_dotenv()


def load_private_key(file_path: str) -> str:
    """加载私钥文件"""
    base_dir = os.path.dirname(os.path.abspath(__file__))
    key_path = os.path.join(base_dir, file_path)
    with open(key_path, "r", encoding="utf-8") as f:
        return f.read().strip()


def generate_jwt(private_key: str) -> str:
    """生成JWT令牌"""
    headers = {"alg": "EdDSA", "kid": os.getenv("JWT_KID"), "typ": None}
    now = int(time.time())
    payload = {
        "sub": os.getenv("PROJECT_ID"),
        "iat": now - 30,
        "exp": now + int(os.getenv("JWT_EXPIRE", 86000)),
    }
    return jwt.encode(payload, private_key, algorithm="EdDSA", headers=headers)


def extract_city_name(fx_link: str) -> str:
    """从fx_link中提取城市名称"""
    city_en = re.search(r"weather/([a-zA-Z]+)-\d+", fx_link).group(1)
    return {"hangzhou": "杭州"}.get(city_en, city_en)


def get_weather_and_save(jwt_token: str) -> None:
    """获取天气数据并保存，同时打印原始响应"""
    url = f"https://{os.getenv('API_HOST')}/v7/weather/now"
    headers = {"Authorization": f"Bearer {jwt_token}"}
    params = {"location": os.getenv("LOCATION_ID", "101210101"), "lang": "en"}

    # 发送请求并获取响应
    response = requests.get(url, headers=headers, params=params, timeout=10)
    # 打印接口回传的原始数据（新增核心逻辑）
    print("\n" + "=" * 60)
    print("接口回传的原始响应数据：")
    print("=" * 60)
    # 方式1：格式化打印JSON数据，更易读
    try:
        raw_data = response.json()
        print(json.dumps(raw_data, ensure_ascii=False, indent=4))
    except json.JSONDecodeError:
        # 若返回非JSON数据，直接打印文本
        print(f"非JSON响应：{response.text}")
    print("=" * 60 + "\n")

    data = response.json()
    # 构造精简的天气信息
    weather_info = {
        "record_time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "temp": data["now"]["temp"],
        "feels_like": data["now"]["feelsLike"],
        "weather_text": data["now"]["text"],
        "precip": data["now"]["precip"],
        "icons": data["now"]["icon"],
    }

    # 保存数据到JSON文件
    json_file = "hangzhou_weather_history.json"
    if os.path.exists(json_file):
        with open(json_file, "r", encoding="utf-8") as f:
            history_data = json.load(f)
        history_data.append(weather_info)
    else:
        history_data = [weather_info]

    with open(json_file, "w", encoding="utf-8") as f:
        json.dump(history_data, f, ensure_ascii=False, indent=4)

    # 打印精简的天气信息
    print("杭州实时天气数据（精简版）：")
    print("=" * 60)
    for key, value in weather_info.items():
        print(f"{key.replace('_', ' ')}：{value}")
    print("=" * 60)
    print(f"数据已追加写入 {json_file}\n")


def main():
    """主函数"""
    try:
        private_key = load_private_key("ed25519-private.pem")
        jwt_token = generate_jwt(private_key)
        get_weather_and_save(jwt_token)
    except Exception as e:
        print(f"程序执行出错：{str(e)}")


if __name__ == "__main__":
    main()
