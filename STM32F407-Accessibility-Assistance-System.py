import requests
import requests
import base64
import json
import os
import time
import tkinter as tk
from tkinter import filedialog, ttk, messagebox, scrolledtext
from PIL import Image, ImageTk
import io
import paho.mqtt.client as mqtt
import threading
import uuid
from datetime import datetime
import asyncio
import tempfile
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import urllib.parse
import queue

# 安装edge-tts库: pip install edge-tts
try:
    import edge_tts
except ImportError:
    pass  # 稍后会处理这个错误


# ===== MyMemory翻译服务 =====
def translate_text(text, source_lang="zh", target_lang="en"):
    """
    使用MyMemory翻译服务将文本从源语言翻译到目标语言

    参数:
    - text: 要翻译的文本
    - source_lang: 源语言代码 (默认为中文'zh')
    - target_lang: 目标语言代码 (默认为英文'en')

    返回:
    - 翻译后的文本，如果失败则返回原文本
    """
    # 去掉可能存在的括号和置信度
    if " (" in text:
        text = text.split(" (")[0]

    try:
        print(f"使用MyMemory翻译: {text}")

        # 使用MyMemory API
        url = "https://api.mymemory.translated.net/get"

        params = {
            "q": text,
            "langpair": f"{source_lang}|{target_lang}",
            "de": "admin@example.com"  # 可以提供一个邮箱以增加使用限额
        }

        print(f"发送请求到MyMemory API: {url}")
        response = requests.get(url, params=params, timeout=10)

        print(f"MyMemory响应状态码: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            if "responseData" in result and "translatedText" in result["responseData"]:
                translated_text = result["responseData"]["translatedText"]
                print(f"MyMemory翻译结果: {translated_text}")
                return translated_text

        print("MyMemory翻译失败，返回原文")
        return text

    except Exception as e:
        print(f"翻译过程中出错: {str(e)}")
        return text


# 翻译管理器类，只使用MyMemory
class TranslationManager:
    def __init__(self):
        self.enable_translation = True  # 默认启用翻译
        self.source_lang = "zh"  # 源语言：中文
        self.target_lang = "en"  # 目标语言：英文

    def translate(self, text):
        """
        根据当前设置翻译文本

        参数:
        - text: 要翻译的文本

        返回:
        - 如果启用了翻译，则返回翻译后的文本；否则返回原文本
        """
        if not self.enable_translation:
            return text

        # 去掉置信度数值部分，只保留识别结果
        if " (" in text:
            text = text.split(" (")[0]

        try:
            print(f"开始翻译: {text}")
            result = translate_text(text, self.source_lang, self.target_lang)
            print(f"翻译结果: {result}")
            return result
        except Exception as e:
            print(f"翻译出错: {str(e)}")
            return text


def get_access_token(api_key, secret_key):
    """
    获取百度AI开放平台的access_token
    """
    url = f"https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id={api_key}&client_secret={secret_key}"

    payload = ""
    headers = {
        'Content-Type': 'application/json',
        'Accept': 'application/json'
    }

    response = requests.request("POST", url, headers=headers, data=payload)

    if response.status_code == 200:
        result = response.json()
        return result.get('access_token')
    else:
        print(f"获取access_token失败: {response.text}")
        return None


def image_recognition(access_token, image_data, recognition_type="general"):
    """
    发送图片到百度AI进行识别

    参数:
    - access_token: 百度AI的访问令牌
    - image_data: 图片数据，可以是文件路径(字符串)或图片二进制数据(bytes)
    - recognition_type: 识别类型
    """

    # 根据识别类型选择API端点
    api_url_map = {
        "general": "https://aip.baidubce.com/rest/2.0/image-classify/v2/advanced_general",
        "animal": "https://aip.baidubce.com/rest/2.0/image-classify/v1/animal",
        "plant": "https://aip.baidubce.com/rest/2.0/image-classify/v1/plant",
        "car": "https://aip.baidubce.com/rest/2.0/image-classify/v1/car",
        "logo": "https://aip.baidubce.com/rest/2.0/image-classify/v2/logo",
        "dish": "https://aip.baidubce.com/rest/2.0/image-classify/v2/dish"
    }

    url = f"{api_url_map.get(recognition_type, api_url_map['general'])}?access_token={access_token}"

    # 读取图片并进行base64编码
    try:
        # 判断image_data是文件路径还是二进制数据
        if isinstance(image_data, str):  # 文件路径
            # 始终将图片转换为JPEG格式
            img = Image.open(image_data)

            # 将图片转换为RGB模式（去除透明通道，确保兼容性）
            if img.mode == 'RGBA':
                img = img.convert('RGB')

            # 创建一个内存缓冲区
            img_byte_arr = io.BytesIO()

            # 将图片保存为JPEG格式
            img.save(img_byte_arr, format='JPEG', quality=95)
            img_byte_arr.seek(0)

            # 获取base64编码
            image_base64 = base64.b64encode(img_byte_arr.read()).decode('utf-8')
        else:  # 二进制数据
            # 尝试将二进制数据加载为PIL图像
            img = Image.open(io.BytesIO(image_data))

            # 将图片转换为RGB模式（去除透明通道，确保兼容性）
            if img.mode == 'RGBA':
                img = img.convert('RGB')

            # 创建一个内存缓冲区
            img_byte_arr = io.BytesIO()

            # 将图片保存为JPEG格式
            img.save(img_byte_arr, format='JPEG', quality=95)
            img_byte_arr.seek(0)

            # 获取base64编码
            image_base64 = base64.b64encode(img_byte_arr.read()).decode('utf-8')

        # 检查图片大小，如果太大则压缩
        if len(image_base64) > 4 * 1024 * 1024:  # 如果大于4MB
            # 重新打开图片并降低尺寸和质量
            img_byte_arr = io.BytesIO()
            max_size = (800, 800)  # 设置更小的最大尺寸
            img.thumbnail(max_size, Image.LANCZOS)
            img.save(img_byte_arr, format='JPEG', quality=80)  # 降低质量
            img_byte_arr.seek(0)
            image_base64 = base64.b64encode(img_byte_arr.read()).decode('utf-8')

    except Exception as e:
        print(f"处理图片失败: {str(e)}")
        return None

    # 使用data参数而不是直接构建payload字符串
    data = {"image": image_base64}

    headers = {
        'Content-Type': 'application/x-www-form-urlencoded',
        'Accept': 'application/json'
    }

    try:
        response = requests.post(url, data=data, headers=headers)

        if response.status_code == 200:
            result = response.json()
            return result
        else:
            print(f"API请求失败: {response.status_code} - {response.text}")
            return None
    except Exception as e:
        print(f"请求过程中出错: {str(e)}")
        return None


# ===== Edge-TTS相关功能 - 修复命名冲突问题 =====
async def _edge_tts_async(text, voice="zh-CN-XiaoxiaoNeural", output_file=None):
    """
    使用Edge-TTS将文本转换为语音的异步函数
    """
    try:
        # 如果没有指定输出文件，则创建临时文件
        if not output_file:
            temp_file = tempfile.NamedTemporaryFile(suffix=".mp3", delete=False)
            output_file = temp_file.name
            temp_file.close()

        # 创建通信对象 - 使用正确的API方式
        communicate = edge_tts.Communicate(text, voice)

        # 保存到文件
        await communicate.save(output_file)

        return output_file
    except Exception as e:
        print(f"Edge-TTS async error: {str(e)}")
        import traceback
        print(traceback.format_exc())
        return None


# 将函数名从edge_tts改为convert_text_to_speech以避免命名冲突
def convert_text_to_speech(text, voice="zh-CN-XiaoxiaoNeural", output_file=None):
    """
    使用Edge-TTS将文本转换为语音的同步函数 - 使用线程池避免阻塞主线程
    """

    # 创建一个新的事件循环在一个新的线程中运行
    def run_async_in_thread():
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            return loop.run_until_complete(_edge_tts_async(text, voice, output_file))
        finally:
            loop.close()

    try:
        # 使用线程池执行器来运行异步函数
        with ThreadPoolExecutor() as executor:
            future = executor.submit(run_async_in_thread)
            file_path = future.result(timeout=30)  # 30秒超时

        if not file_path:
            return None

        # 读取生成的音频文件
        with open(file_path, 'rb') as f:
            audio_data = f.read()

        # 如果使用的是临时文件，则删除它
        if not output_file:
            try:
                Path(file_path).unlink(missing_ok=True)
            except:
                pass  # 忽略删除文件的错误

        return audio_data

    except Exception as e:
        print(f"TTS处理失败: {str(e)}")
        import traceback
        print(traceback.format_exc())
        return None


async def _get_voices_async():
    """
    获取Edge-TTS可用的语音列表 - 添加错误处理
    """
    try:
        voices = await edge_tts.list_voices()
        return voices
    except Exception as e:
        print(f"获取语音列表异步错误: {str(e)}")
        import traceback
        print(traceback.format_exc())
        return []


def get_voices():
    """
    获取Edge-TTS可用的语音列表的同步包装函数 - 使用线程池运行
    """

    # 创建一个新的事件循环在一个新的线程中运行
    def run_async_in_thread():
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            return loop.run_until_complete(_get_voices_async())
        finally:
            loop.close()

    try:
        # 使用线程池执行器来运行异步函数
        with ThreadPoolExecutor() as executor:
            future = executor.submit(run_async_in_thread)
            voices = future.result(timeout=30)  # 30秒超时

        return voices
    except Exception as e:
        print(f"获取语音列表失败: {str(e)}")
        import traceback
        print(traceback.format_exc())
        return []


# 测试Edge-TTS安装状态的函数
def test_edge_tts_installation():
    """测试Edge-TTS是否正确安装和可用"""
    try:
        import edge_tts
        print("Edge-TTS模块已成功导入")

        # 尝试获取语音列表
        voices = get_voices()
        if voices:
            print(f"Edge-TTS语音列表获取成功: 共有 {len(voices)} 种语音可用")
            return True
        else:
            print("无法获取Edge-TTS语音列表")
            return False
    except ImportError:
        print("未找到Edge-TTS模块。请使用以下命令安装: pip install edge-tts")
        return False
    except Exception as e:
        print(f"测试Edge-TTS时出错: {str(e)}")
        import traceback
        print(traceback.format_exc())
        return False


def prepare_audio_for_stm32(audio_data, output_format="mp3"):
    """
    处理音频数据，准备发送到STM32

    参数:
    - audio_data: 原始音频数据
    - output_format: 输出格式，默认为mp3

    返回:
    - 处理后的音频数据
    """
    # 对于MP3格式，通常不需要特殊处理，可以直接返回
    # 如果需要转换格式，可以在这里添加转换代码
    return audio_data


def send_audio_via_mqtt(client, audio_data, chunk_size=1024):
    """
    通过MQTT发送音频数据到STM32

    参数:
    - client: MQTT客户端
    - audio_data: 音频数据
    - chunk_size: 分块大小

    返回:
    - 是否发送成功
    """
    try:
        # 发送音频信息
        audio_info = {
            "size": len(audio_data),
            "format": "mp3",
            "chunks": (len(audio_data) + chunk_size - 1) // chunk_size
        }

        client.publish("stm32/audio/info", json.dumps(audio_info))

        # 分块发送音频数据
        for i in range(0, len(audio_data), chunk_size):
            chunk = audio_data[i:i + chunk_size]
            client.publish(f"stm32/audio/data/{i // chunk_size}", chunk)

        # 发送结束信号
        client.publish("stm32/audio/end", "")

        return True

    except Exception as e:
        print(f"发送音频失败: {str(e)}")
        return False


class BaiduImageRecognitionApp:
    def __init__(self, root):
        self.root = root
        self.root.title("百度AI图像识别 + MQTT + Edge-TTS")
        self.root.geometry("900x700")
        self.root.resizable(True, True)

        # 创建UI更新队列
        self.ui_queue = queue.Queue()

        # 设置百度AI凭证
        self.api_key = tk.StringVar(value="")# 请在""之间输入百度AI提供的api_key
        self.secret_key = tk.StringVar(value="")# 请在""之间输入百度AI提供的secret_key
        self.recognition_type = tk.StringVar(value="general")
        self.image_path = None
        self.access_token = None
        self.debug_mode = tk.BooleanVar(value=False)

        # MQTT设置
        self.mqtt_broker = tk.StringVar(value="")# 请在""之间输入MQTT服务器
        self.mqtt_port = tk.IntVar(value=1883)
        self.mqtt_connected = False
        self.mqtt_client = None

        # 添加用于存储最近识别结果的变量
        self.last_recognition_result = None

        # TTS语音设置
        self.tts_voice = tk.StringVar(value="zh-CN-XiaoxiaoNeural")

        # ===== 翻译设置 =====
        self.enable_translation = tk.BooleanVar(value=True)  # 默认启用翻译
        self.translator = TranslationManager()  # 创建翻译管理器实例
        self.test_translation_input = tk.StringVar(value="测试翻译文本")  # 用于测试翻译的输入

        # 接收图片数据的变量
        self.received_image_info = None
        self.received_image_data = {}
        self.received_image_complete = False
        self.reconstructed_image_data = None
        self.reconstructed_image_path = None
        self.expected_total_chunks = 0

        # 自动重试token获取的相关变量
        self.token_retry_count = 0
        self.max_token_retries = 3
        self.token_retry_interval = 5000  # 5秒

        # 创建UI组件
        self.create_widgets()

        # 设置定期检查UI队列的任务
        self.setup_ui_queue_check()

        # 启动MQTT服务
        self.connect_mqtt()

        # 初始化TTS功能
        self.initialize_tts()

        # 初始化翻译功能
        self.initialize_translation()

        # 启动接收消息的线程
        self.message_thread = threading.Thread(target=self.mqtt_message_loop, daemon=True)
        self.message_thread.start()

        # 自动获取Access Token
        self.auto_get_token()

    def setup_ui_queue_check(self):
        """设置定期检查UI更新队列的任务"""

        def check_queue():
            try:
                # 非阻塞检查队列
                while True:
                    callback = self.ui_queue.get_nowait()
                    callback()
                    self.ui_queue.task_done()
            except queue.Empty:
                pass
            finally:
                # 每100毫秒检查一次队列
                self.root.after(100, check_queue)

        # 启动队列检查
        self.root.after(100, check_queue)

    # ===== 新增：安全更新UI的辅助方法 =====
    def safe_update_ui(self, callback):
        """安全地将UI更新放入队列"""
        self.ui_queue.put(callback)

    # ===== 新增：保存结果到文件的功能 =====
    def save_result_to_file(self, result):
        """将识别结果保存到文件，以便STM32可以从文件系统中读取"""
        try:
            # 检查结果是否有效
            if not result:
                self.log_mqtt("结果为空，不保存到文件")
                return

            # 保存到SD卡目录（假设Python在同一系统或有网络共享访问权限）
            self.log_mqtt(f"正在将结果保存到文件: {result}")

            # 可以根据实际部署情况修改文件路径
            with open("result.txt", "w", encoding="utf-8") as f:
                f.write(result)

            self.log_mqtt("结果已成功保存到文件")
        except Exception as e:
            self.log_mqtt(f"保存结果到文件失败: {str(e)}")

    # ===== 初始化翻译功能 =====
    def initialize_translation(self):
        """初始化翻译功能"""
        self.log_mqtt("正在初始化翻译功能...")

        # 设置翻译参数
        self.translator.enable_translation = self.enable_translation.get()

        if self.translator.enable_translation:
            self.log_mqtt(f"翻译功能已启用，将使用MyMemory服务将中文结果翻译为英文")
        else:
            self.log_mqtt("翻译功能已禁用")

    # ===== 翻译功能开关回调 =====
    def toggle_translation(self):
        """切换翻译功能的启用状态"""
        self.translator.enable_translation = self.enable_translation.get()

        if self.translator.enable_translation:
            self.log_mqtt("翻译功能已启用")
        else:
            self.log_mqtt("翻译功能已禁用")

    # ===== 新增：测试翻译功能 =====
    def test_translation(self):
        """测试翻译功能"""
        try:
            input_text = self.test_translation_input.get()
            if not input_text:
                messagebox.showwarning("警告", "请输入要翻译的文本")
                return

            self.log_mqtt(f"测试翻译文本: {input_text}")

            # 开始翻译
            translated_text = self.translator.translate(input_text)

            # 显示结果
            self.log_mqtt(f"翻译结果: {translated_text}")
            messagebox.showinfo("翻译结果", f"原文: {input_text}\n\n译文: {translated_text}")

        except Exception as e:
            self.log_mqtt(f"翻译测试失败: {str(e)}")
            messagebox.showerror("翻译错误", f"翻译过程中出错: {str(e)}")

    def initialize_tts(self):
        """初始化TTS功能"""
        self.log_mqtt("正在初始化Edge-TTS...")

        # 检查Edge-TTS是否正确安装
        try:
            import edge_tts
            self.edge_tts_available = True
            self.log_mqtt("Edge-TTS模块已成功导入")
        except ImportError:
            self.edge_tts_available = False
            self.log_mqtt("错误: 未安装Edge-TTS模块，请运行 'pip install edge-tts' 安装")
            messagebox.showwarning("警告", "未安装Edge-TTS模块，语音功能将不可用。\n请运行 'pip install edge-tts' 安装。")
            return

        # 测试获取语音列表
        try:
            voices = self.get_voices_with_fallback()
            if voices:
                self.log_mqtt(f"Edge-TTS语音列表获取成功: 共有 {len(voices)} 种语音可用")
            else:
                self.log_mqtt("警告: 无法获取Edge-TTS语音列表，将使用默认语音")
        except Exception as e:
            self.log_mqtt(f"初始化TTS时出错: {str(e)}")
            import traceback
            self.log_mqtt(traceback.format_exc())

    def get_voices_with_fallback(self):
        """获取语音列表，并提供备用选项"""
        try:
            voices = get_voices()
            if voices:
                return voices
            else:
                self.log_mqtt("无法获取在线语音列表，使用备用语音列表")
                # 备用语音列表
                return [
                    {"ShortName": "zh-CN-XiaoxiaoNeural", "Gender": "Female", "Locale": "zh-CN"},
                    {"ShortName": "zh-CN-YunxiNeural", "Gender": "Male", "Locale": "zh-CN"},
                    {"ShortName": "zh-CN-YunyangNeural", "Gender": "Male", "Locale": "zh-CN"},
                    {"ShortName": "zh-CN-XiaohanNeural", "Gender": "Female", "Locale": "zh-CN"},
                    {"ShortName": "zh-CN-XiaomoNeural", "Gender": "Female", "Locale": "zh-CN"},
                    {"ShortName": "en-US-AriaNeural", "Gender": "Female", "Locale": "en-US"},
                    {"ShortName": "en-US-GuyNeural", "Gender": "Male", "Locale": "en-US"}
                ]
        except Exception as e:
            self.log_mqtt(f"获取语音列表失败: {str(e)}")
            # 返回最小备用列表
            return [{"ShortName": "zh-CN-XiaoxiaoNeural", "Gender": "Female", "Locale": "zh-CN"}]

    def create_widgets(self):
        # 创建主框架，使用Notebook创建选项卡
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # 主页面 - 文件选择和识别
        main_frame = ttk.Frame(notebook, padding="10")
        notebook.add(main_frame, text="本地文件识别")

        # MQTT页面 - MQTT设置和消息日志
        mqtt_frame = ttk.Frame(notebook, padding="10")
        notebook.add(mqtt_frame, text="MQTT远程识别")

        # 新增：翻译测试页面
        translation_test_frame = ttk.Frame(notebook, padding="10")
        notebook.add(translation_test_frame, text="翻译测试")

        # ===== 主页面布局 =====

        # 创建凭证输入区域
        cred_frame = ttk.LabelFrame(main_frame, text="百度AI凭证", padding="10")
        cred_frame.pack(fill=tk.X, padx=5, pady=5)

        ttk.Label(cred_frame, text="API Key:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
        ttk.Entry(cred_frame, textvariable=self.api_key, width=50).grid(row=0, column=1, sticky=tk.W, padx=5, pady=5)

        ttk.Label(cred_frame, text="Secret Key:").grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)
        ttk.Entry(cred_frame, textvariable=self.secret_key, width=50, show="*").grid(row=1, column=1, sticky=tk.W,
                                                                                     padx=5, pady=5)

        ttk.Button(cred_frame, text="获取Access Token", command=self.get_token).grid(row=2, column=0, columnspan=2,
                                                                                     pady=10)

        # 创建图片上传区域
        upload_frame = ttk.LabelFrame(main_frame, text="图片上传", padding="10")
        upload_frame.pack(fill=tk.X, padx=5, pady=5)

        ttk.Button(upload_frame, text="选择图片", command=self.select_image).grid(row=0, column=0, padx=5, pady=5)
        self.image_label = ttk.Label(upload_frame, text="未选择图片")
        self.image_label.grid(row=0, column=1, padx=5, pady=5)

        # 添加调试模式选项
        ttk.Checkbutton(upload_frame, text="调试模式", variable=self.debug_mode).grid(row=0, column=2, padx=5, pady=5)

        # 创建预览图片区域
        self.preview_frame = ttk.LabelFrame(main_frame, text="图片预览", padding="10")
        self.preview_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.preview_label = ttk.Label(self.preview_frame)
        self.preview_label.pack(fill=tk.BOTH, expand=True)

        # 创建识别类型选择区域
        type_frame = ttk.LabelFrame(main_frame, text="识别类型", padding="10")
        type_frame.pack(fill=tk.X, padx=5, pady=5)

        types = [
            ("通用物体识别", "general"),
            ("动物识别", "animal"),
            ("植物识别", "plant"),
            ("车辆识别", "car"),
            ("Logo识别", "logo"),
            ("菜品识别", "dish")
        ]

        for i, (text, value) in enumerate(types):
            ttk.Radiobutton(type_frame, text=text, value=value, variable=self.recognition_type).grid(
                row=i // 3, column=i % 3, sticky=tk.W, padx=20, pady=5
            )

        # 创建识别按钮
        ttk.Button(main_frame, text="开始识别", command=self.recognize_image).pack(pady=10)

        # 创建结果显示区域
        result_frame = ttk.LabelFrame(main_frame, text="识别结果", padding="10")
        result_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.result_text = tk.Text(result_frame, wrap=tk.WORD, height=10)
        self.result_text.pack(fill=tk.BOTH, expand=True)

        # ===== MQTT页面布局 =====

        # MQTT设置区域
        mqtt_settings_frame = ttk.LabelFrame(mqtt_frame, text="MQTT设置", padding="10")
        mqtt_settings_frame.pack(fill=tk.X, padx=5, pady=5)

        ttk.Label(mqtt_settings_frame, text="代理服务器:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
        ttk.Entry(mqtt_settings_frame, textvariable=self.mqtt_broker, width=30).grid(row=0, column=1, sticky=tk.W,
                                                                                     padx=5, pady=5)

        ttk.Label(mqtt_settings_frame, text="端口:").grid(row=0, column=2, sticky=tk.W, padx=5, pady=5)
        ttk.Entry(mqtt_settings_frame, textvariable=self.mqtt_port, width=10).grid(row=0, column=3, sticky=tk.W, padx=5,
                                                                                   pady=5)

        self.mqtt_status_label = ttk.Label(mqtt_settings_frame, text="状态: 未连接")
        self.mqtt_status_label.grid(row=1, column=0, columnspan=2, sticky=tk.W, padx=5, pady=5)

        self.mqtt_connect_button = ttk.Button(mqtt_settings_frame, text="连接", command=self.connect_mqtt)
        self.mqtt_connect_button.grid(row=1, column=2, padx=5, pady=5)

        self.mqtt_disconnect_button = ttk.Button(mqtt_settings_frame, text="断开", command=self.disconnect_mqtt,
                                                 state=tk.DISABLED)
        self.mqtt_disconnect_button.grid(row=1, column=3, padx=5, pady=5)

        # 翻译设置区域 - 使用MyMemory翻译
        translation_frame = ttk.LabelFrame(mqtt_frame, text="翻译设置", padding="10")
        translation_frame.pack(fill=tk.X, padx=5, pady=5)

        ttk.Checkbutton(translation_frame, text="启用翻译（中文→英文，使用MyMemory服务）",
                        variable=self.enable_translation,
                        command=self.toggle_translation).pack(anchor=tk.W, padx=5, pady=5)

        # 添加TTS语音设置
        self.add_tts_voice_selection(mqtt_frame)

        # MQTT接收区域
        mqtt_receive_frame = ttk.LabelFrame(mqtt_frame, text="MQTT接收", padding="10")
        mqtt_receive_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.mqtt_log = scrolledtext.ScrolledText(mqtt_receive_frame, wrap=tk.WORD, height=20)
        self.mqtt_log.pack(fill=tk.BOTH, expand=True)
        self.mqtt_log.config(state=tk.DISABLED)

        # MQTT接收到的图片预览
        mqtt_preview_frame = ttk.LabelFrame(mqtt_frame, text="接收图片预览", padding="10")
        mqtt_preview_frame.pack(fill=tk.X, padx=5, pady=5)

        self.mqtt_preview_label = ttk.Label(mqtt_preview_frame, text="未接收图片")
        self.mqtt_preview_label.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # MQTT识别按钮区域
        mqtt_recognize_frame = ttk.Frame(mqtt_frame, padding="10")
        mqtt_recognize_frame.pack(fill=tk.X, padx=5, pady=5)

        self.mqtt_recognize_button = ttk.Button(mqtt_recognize_frame, text="识别接收到的图片",
                                                command=self.recognize_received_image, state=tk.DISABLED)
        self.mqtt_recognize_button.pack(pady=10)

        # ===== 翻译测试页面布局 =====
        test_input_frame = ttk.LabelFrame(translation_test_frame, text="翻译测试", padding="10")
        test_input_frame.pack(fill=tk.X, padx=5, pady=5)

        ttk.Label(test_input_frame, text="输入中文文本:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
        ttk.Entry(test_input_frame, textvariable=self.test_translation_input, width=50).grid(row=0, column=1,
                                                                                             sticky=tk.W, padx=5,
                                                                                             pady=5)

        ttk.Button(test_input_frame, text="测试翻译", command=self.test_translation).grid(row=1, column=0, columnspan=2,
                                                                                          pady=10)

        # 翻译服务设置 - 使用MyMemory
        translation_service_frame = ttk.LabelFrame(translation_test_frame, text="翻译服务设置", padding="10")
        translation_service_frame.pack(fill=tk.X, padx=5, pady=5)

        ttk.Checkbutton(translation_service_frame, text="启用翻译（中文→英文，使用MyMemory服务）",
                        variable=self.enable_translation,
                        command=self.toggle_translation).pack(anchor=tk.W, padx=5, pady=5)

        # 翻译日志区域
        translation_log_frame = ttk.LabelFrame(translation_test_frame, text="翻译日志", padding="10")
        translation_log_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # 使用MQTT的日志区域来显示翻译日志
        ttk.Label(translation_log_frame, text="翻译过程会记录在MQTT标签页的日志区域").pack(pady=10)

    def add_tts_voice_selection(self, tab_frame):
        """
        在指定标签页添加TTS语音选择功能 - 改进版
        """
        # TTS设置区域
        tts_frame = ttk.LabelFrame(tab_frame, text="语音合成设置", padding="10")
        tts_frame.pack(fill=tk.X, padx=5, pady=5)

        # 语音选择下拉框
        ttk.Label(tts_frame, text="语音选择:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)

        # 创建下拉框
        self.voice_combobox = ttk.Combobox(tts_frame, textvariable=self.tts_voice, width=30)
        self.voice_combobox.grid(row=0, column=1, sticky=tk.W, padx=5, pady=5)

        # 默认添加一些常用的中文语音
        default_voices = [
            "zh-CN-XiaoxiaoNeural",  # 女声
            "zh-CN-YunxiNeural",  # 男声
            "zh-CN-YunyangNeural",  # 男声
            "zh-CN-XiaohanNeural",  # 女声
            "zh-CN-XiaomoNeural"  # 女声
        ]
        self.voice_combobox['values'] = default_voices

        # 添加获取所有语音的按钮
        ttk.Button(tts_frame, text="获取所有语音", command=self.fetch_all_voices).grid(row=0, column=2, padx=5, pady=5)

        # 添加测试TTS的按钮
        ttk.Button(tts_frame, text="测试语音", command=self.test_tts_voice).grid(row=1, column=0, columnspan=3, padx=5,
                                                                                 pady=5)

    def fetch_all_voices(self):
        """获取所有可用的Edge-TTS语音并更新下拉框 - 改进版"""
        if not hasattr(self, 'edge_tts_available') or not self.edge_tts_available:
            messagebox.showwarning("警告", "Edge-TTS模块未安装或不可用")
            return

        self.log_mqtt("正在获取Edge-TTS可用语音列表...")

        try:
            # 创建进度指示器
            progress_window = tk.Toplevel(self.root)
            progress_window.title("获取语音列表")
            progress_window.geometry("300x100")
            progress_window.transient(self.root)
            progress_window.grab_set()

            ttk.Label(progress_window, text="正在获取语音列表，请稍候...").pack(pady=10)
            progress = ttk.Progressbar(progress_window, mode="indeterminate")
            progress.pack(fill=tk.X, padx=20, pady=10)
            progress.start()

            # 使用线程获取语音列表
            def get_voices_thread():
                try:
                    voices = get_voices()

                    # 更新UI必须在主线程中进行
                    self.safe_update_ui(lambda: self.update_voices_combobox(voices, progress_window))
                except Exception as e:
                    self.log_mqtt(f"获取语音列表时出错: {str(e)}")
                    # 清理UI必须在主线程中进行
                    self.safe_update_ui(lambda: progress_window.destroy())
                    self.safe_update_ui(lambda: messagebox.showerror("错误", f"获取语音列表失败: {str(e)}"))

            # 启动线程
            threading.Thread(target=get_voices_thread, daemon=True).start()

        except Exception as e:
            self.log_mqtt(f"启动获取语音列表过程时出错: {str(e)}")
            messagebox.showerror("错误", f"获取语音列表失败: {str(e)}")

    def update_voices_combobox(self, voices, progress_window=None):
        """更新语音下拉框"""
        try:
            if voices:
                # 提取中文语音
                chinese_voices = [v["ShortName"] for v in voices if v["Locale"].startswith("zh-")]

                # 更新下拉框的值
                self.voice_combobox['values'] = chinese_voices

                # 默认选择第一个语音
                if chinese_voices:
                    self.tts_voice.set(chinese_voices[0])

                self.log_mqtt(f"成功获取到 {len(chinese_voices)} 个中文语音")
                messagebox.showinfo("成功", f"成功获取到 {len(chinese_voices)} 个中文语音")
            else:
                self.log_mqtt("获取语音列表失败，无可用语音")
                messagebox.showwarning("警告", "获取语音列表失败，将使用默认语音")
        except Exception as e:
            self.log_mqtt(f"更新语音下拉框时出错: {str(e)}")
        finally:
            # 关闭进度窗口
            if progress_window:
                progress_window.destroy()

    def test_tts_voice(self):
        """测试选定的TTS语音 - 修复版本"""
        if not hasattr(self, 'edge_tts_available') or not self.edge_tts_available:
            messagebox.showwarning("警告", "Edge-TTS模块未安装或不可用")
            return

        try:
            voice = self.tts_voice.get()
            test_text = "这是一个语音合成测试，能听到我说话吗？"

            self.log_mqtt(f"正在测试语音 {voice}...")

            # 创建进度指示器
            progress_window = tk.Toplevel(self.root)
            progress_window.title("生成语音")
            progress_window.geometry("300x100")
            progress_window.transient(self.root)
            progress_window.grab_set()

            ttk.Label(progress_window, text="正在生成语音，请稍候...").pack(pady=10)
            progress = ttk.Progressbar(progress_window, mode="indeterminate")
            progress.pack(fill=tk.X, padx=20, pady=10)
            progress.start()

            # 使用线程生成语音
            def generate_tts_thread():
                try:
                    # 转换文本为语音 - 使用修改后的函数名
                    audio_data = convert_text_to_speech(test_text, voice)

                    # 更新UI必须在主线程中进行
                    self.safe_update_ui(lambda: self.play_test_audio(audio_data, progress_window))
                except Exception as e:
                    self.log_mqtt(f"生成测试语音时出错: {str(e)}")
                    # 清理UI必须在主线程中进行
                    self.safe_update_ui(lambda: progress_window.destroy())
                    self.safe_update_ui(lambda: messagebox.showerror("错误", f"生成测试语音失败: {str(e)}"))

            # 启动线程
            threading.Thread(target=generate_tts_thread, daemon=True).start()

        except Exception as e:
            self.log_mqtt(f"测试语音时出错: {str(e)}")
            messagebox.showerror("错误", f"测试语音失败: {str(e)}")

    def play_test_audio(self, audio_data, progress_window=None):
        """播放测试音频"""
        try:
            if audio_data:
                # 创建临时文件
                with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as temp_file:
                    temp_file.write(audio_data)
                    temp_file_path = temp_file.name

                # 播放音频
                self.log_mqtt(f"播放测试音频...")

                # 使用系统默认播放器播放
                if os.name == 'nt':  # Windows
                    os.system(f'start {temp_file_path}')
                elif os.name == 'posix':  # Linux/Mac
                    os.system(f'xdg-open {temp_file_path}')

                # 计划在30秒后删除临时文件
                def delete_temp_file():
                    try:
                        Path(temp_file_path).unlink(missing_ok=True)
                    except:
                        pass

                self.root.after(30000, delete_temp_file)

                self.log_mqtt("测试音频生成成功并开始播放")
            else:
                self.log_mqtt("生成测试音频失败")
                messagebox.showerror("错误", "生成测试音频失败")
        except Exception as e:
            self.log_mqtt(f"播放测试音频时出错: {str(e)}")
            messagebox.showerror("错误", f"播放测试音频失败: {str(e)}")
        finally:
            # 关闭进度窗口
            if progress_window:
                progress_window.destroy()

    def get_token(self):
        # 检查API Key和Secret Key是否已输入
        if not self.api_key.get() or not self.secret_key.get():
            messagebox.showerror("错误", "请输入API Key和Secret Key")
            return

        # 获取access_token
        self.access_token = get_access_token(self.api_key.get(), self.secret_key.get())

        if self.access_token:
            messagebox.showinfo("成功", "成功获取Access Token")
            self.log_mqtt(f"成功获取百度AI Access Token，有效期30天")
        else:
            messagebox.showerror("错误", "获取Access Token失败，请检查您的API Key和Secret Key")

    def auto_get_token(self):
        """自动获取百度AI的Access Token"""
        self.log_mqtt("自动获取Access Token中...")

        # 检查API Key和Secret Key是否已配置
        if not self.api_key.get() or not self.secret_key.get():
            self.log_mqtt("错误：API Key或Secret Key未配置，无法获取Access Token")
            # 5秒后重试
            self.root.after(5000, self.auto_get_token)
            return

        # 获取access_token
        try:
            self.access_token = get_access_token(self.api_key.get(), self.secret_key.get())

            if self.access_token:
                self.log_mqtt(f"成功自动获取百度AI Access Token，有效期30天")
                self.token_retry_count = 0  # 重置重试计数
            else:
                self.token_retry_count += 1
                self.log_mqtt(f"Access Token获取失败 (尝试 {self.token_retry_count}/{self.max_token_retries})")

                # 如果未达到最大重试次数，则安排重试
                if self.token_retry_count < self.max_token_retries:
                    self.log_mqtt(f"{self.token_retry_interval / 1000}秒后将重试获取Token...")
                    self.root.after(self.token_retry_interval, self.auto_get_token)
                else:
                    self.log_mqtt("达到最大重试次数，请检查API Key和Secret Key是否正确")
        except Exception as e:
            self.log_mqtt(f"获取Access Token时出错: {str(e)}")
            # 5秒后重试
            self.root.after(5000, self.auto_get_token)

    def select_image(self):
        # 打开文件选择对话框
        filetypes = [
            ("图片文件", "*.jpg *.jpeg *.png *.bmp *.gif"),
            ("所有文件", "*.*")
        ]

        self.image_path = filedialog.askopenfilename(filetypes=filetypes)

        if self.image_path:
            self.image_label.config(text=os.path.basename(self.image_path))
            self.display_preview()

            # 显示图片信息（如果调试模式开启）
            if self.debug_mode.get():
                try:
                    img = Image.open(self.image_path)
                    file_size = os.path.getsize(self.image_path) / 1024  # KB
                    self.result_text.delete(1.0, tk.END)
                    self.result_text.insert(tk.END, f"图片信息:\n")
                    self.result_text.insert(tk.END, f"- 文件名: {os.path.basename(self.image_path)}\n")
                    self.result_text.insert(tk.END, f"- 尺寸: {img.width} x {img.height} 像素\n")
                    self.result_text.insert(tk.END, f"- 格式: {img.format}\n")
                    self.result_text.insert(tk.END, f"- 文件大小: {file_size:.2f} KB\n")
                    self.result_text.insert(tk.END, f"- 模式: {img.mode}\n")
                except Exception as e:
                    self.result_text.insert(tk.END, f"获取图片信息失败: {str(e)}\n")
        else:
            self.image_label.config(text="未选择图片")

    def display_preview(self, image_data=None, label=None):
        # 显示图片预览
        try:
            if image_data is None and self.image_path:
                # 从文件打开图片
                img = Image.open(self.image_path)
            elif image_data is not None:
                # 从二进制数据打开图片
                img = Image.open(io.BytesIO(image_data))
            else:
                return

            # 调整图片大小以适应预览区域
            width, height = img.size
            max_size = 300

            if width > height:
                new_width = max_size
                new_height = int(height * max_size / width)
            else:
                new_height = max_size
                new_width = int(width * max_size / height)

            img = img.resize((new_width, new_height), Image.LANCZOS)

            # 转换为Tkinter可用的格式
            photo = ImageTk.PhotoImage(img)

            # 显示图片
            if label is None:
                label = self.preview_label

            # 安全更新UI
            def update_label():
                label.config(image=photo)
                label.image = photo  # 保持引用，防止被垃圾回收

            # 如果在主线程中，直接更新；否则添加到队列
            self.safe_update_ui(update_label)

        except Exception as e:
            messagebox.showerror("错误", f"无法预览图片: {str(e)}")

    def process_recognition_result(self, result, recognition_type, elapsed_time, is_mqtt=False):
        # 创建用于显示的文本和返回给MQTT设备的简化结果
        display_text = ""
        mqtt_result = ""
        speech_text = ""  # 用于TTS的文本

        if 'error_code' in result:
            display_text += f"识别失败：错误码 {result['error_code']}\n"
            display_text += f"错误信息：{result.get('error_msg', '未知错误')}\n\n"
            mqtt_result = f"错误: {result.get('error_msg', '未知错误')}"
            speech_text = f"识别失败，错误信息：{result.get('error_msg', '未知错误')}"
        elif recognition_type == "general":
            if 'result' in result and result['result']:
                # 找出最高得分的结果
                max_score_item = None
                max_score = -1

                for item in result['result']:
                    score = item.get('score', 0)
                    # 确保score是浮点数
                    if isinstance(score, str):
                        try:
                            score = float(score)
                        except ValueError:
                            score = 0

                    if score > max_score:
                        max_score = score
                        max_score_item = item

                # 显示最终结果（得分最高的）
                if max_score_item:
                    keyword = max_score_item.get('keyword', '未知')
                    display_text += "【最终结果】\n"
                    display_text += f"识别为: {keyword} (置信度: {max_score:.2%})\n\n"
                    # 修改：只返回关键词，不带置信度
                    mqtt_result = f"{keyword}"
                    speech_text = f"图像识别结果是：{keyword}，置信度：{max_score:.0%}"

                # 显示所有结果
                display_text += "所有识别结果:\n"
                for item in result['result']:
                    keyword = item.get('keyword', '未知')
                    score = item.get('score', 0)
                    # 确保score是浮点数
                    if isinstance(score, str):
                        try:
                            score = float(score)
                        except ValueError:
                            score = 0
                    display_text += f"• {keyword}: {score:.2%}\n"
            else:
                display_text += "未找到识别结果\n"
                mqtt_result = "未找到识别结果"
                speech_text = "未找到识别结果"
        else:
            # 其他识别类型的处理逻辑
            # 这里为简化代码，仅提取最高得分结果
            if 'result' in result and result['result'] and len(result['result']) > 0:
                max_item = max(result['result'], key=lambda x: float(x.get('score', 0)) if isinstance(x.get('score', 0),
                                                                                                      (float,
                                                                                                       int)) else 0)
                name = max_item.get('name', max_item.get('keyword', '未知'))
                score = max_item.get('score', 0)
                if isinstance(score, str):
                    score = float(score) if score.replace('.', '', 1).isdigit() else 0

                display_text += f"【最终结果】\n识别为: {name} (置信度: {score:.2%})\n\n"
                # 修改：只返回名称，不带置信度
                mqtt_result = f"{name}"
                speech_text = f"图像识别结果是：{name}，置信度：{score:.0%}"
            else:
                display_text += "未找到识别结果\n"
                mqtt_result = "未找到识别结果"
                speech_text = "未找到识别结果"

        # 显示识别耗时
        display_text = f"识别完成！耗时: {elapsed_time:.2f}秒\n\n" + display_text

        # 添加完整的JSON响应
        if self.debug_mode.get():
            display_text += "\n完整API响应:\n"
            display_text += json.dumps(result, ensure_ascii=False, indent=2)

        # 更新UI显示 - 在主线程中安全更新
        if not is_mqtt:
            def update_result_text():
                self.result_text.delete(1.0, tk.END)
                self.result_text.insert(tk.END, display_text)

            self.safe_update_ui(update_result_text)
        else:
            self.log_mqtt(f"识别结果: {mqtt_result}")

        # ===== 使用Edge-TTS处理的部分 =====
        # 如果结果是通过MQTT获取的，则进行TTS转换并发送音频
        if is_mqtt and speech_text and self.mqtt_connected:
            self.log_mqtt("正在使用Edge-TTS将识别结果转换为语音...")

            # 使用线程进行TTS转换，避免阻塞UI
            def tts_thread():
                try:
                    # 使用Edge-TTS转换文本为语音 - 使用修改后的函数名
                    voice = self.tts_voice.get()
                    audio_data = convert_text_to_speech(speech_text, voice)

                    if audio_data:
                        self.log_mqtt("文本成功转换为语音，正在处理音频...")

                        # 处理音频为STM32可播放的格式
                        processed_audio = prepare_audio_for_stm32(audio_data)

                        if processed_audio:
                            self.log_mqtt("正在发送音频到STM32...")

                            # 将音频发送到STM32
                            if send_audio_via_mqtt(self.mqtt_client, processed_audio):
                                self.log_mqtt("音频已成功发送到STM32")
                            else:
                                self.log_mqtt("发送音频到STM32失败")
                        else:
                            self.log_mqtt("处理音频失败")
                    else:
                        self.log_mqtt("文本转语音失败")
                except Exception as e:
                    self.log_mqtt(f"TTS处理过程中出错: {str(e)}")
                    import traceback
                    self.log_mqtt(traceback.format_exc())

            # 启动TTS线程
            threading.Thread(target=tts_thread, daemon=True).start()

        # 保存最近的识别结果
        if is_mqtt:
            # 保存最近的识别结果
            self.last_recognition_result = mqtt_result

        return mqtt_result

    def recognize_image(self, image_path=None, is_mqtt=False):
        # 默认使用本地选择的图片，除非提供了特定的图片路径
        if image_path is None:
            image_path = self.image_path

        # 检查是否已选择图片
        if not image_path:
            messagebox.showerror("错误", "请先选择一张图片")
            return

        # 检查是否已获取access_token
        if not self.access_token:
            messagebox.showerror("错误", "请先获取Access Token")
            return

        # 显示进度
        if not is_mqtt:
            def update_progress():
                self.result_text.delete(1.0, tk.END)
                self.result_text.insert(tk.END, "正在识别图片，请稍候...\n")

            self.safe_update_ui(update_progress)
            self.root.update()
        else:
            self.log_mqtt("正在识别MQTT接收的图片，请稍候...")

        # 开始计时
        start_time = time.time()

        # 获取识别类型
        recognition_type = self.recognition_type.get()

        # 调用识别函数
        result = image_recognition(self.access_token, image_path, recognition_type)

        # 计算耗时
        end_time = time.time()
        elapsed_time = end_time - start_time

        if result:
            # 处理识别结果
            recognition_result = self.process_recognition_result(result, recognition_type, elapsed_time, is_mqtt)

            # 如果是MQTT模式，发送结果回ESP8266
            if is_mqtt and self.mqtt_connected:
                # 去掉置信度部分，只保留物体名称
                clean_result = recognition_result.split(" (")[0] if " (" in recognition_result else recognition_result

                # 如果启用了翻译，先翻译结果
                if hasattr(self, 'translator') and self.translator.enable_translation:
                    original_result = clean_result
                    translated_result = self.translator.translate(clean_result)

                    # 保存最后识别的结果（翻译后的）
                    self.last_recognition_result = translated_result

                    # 多次发送结果以提高可靠性
                    for i in range(3):
                        self.mqtt_client.publish("stm32/result", translated_result)
                        time.sleep(0.2)  # 短暂延迟，避免消息拥堵

                    # 同时将结果保存到文件
                    self.save_result_to_file(translated_result)

                    # 记录日志
                    self.log_mqtt(f"原始结果: {original_result}")
                    self.log_mqtt(f"翻译后结果: {translated_result}")
                    self.log_mqtt(f"翻译后的识别结果已发送回设备（已重试3次）")
                else:
                    # 保存最后识别的结果（原始）
                    self.last_recognition_result = clean_result

                    # 多次发送结果以提高可靠性
                    for i in range(3):
                        self.mqtt_client.publish("stm32/result", clean_result)
                        time.sleep(0.2)  # 短暂延迟，避免消息拥堵

                    # 同时将结果保存到文件
                    self.save_result_to_file(clean_result)

                    self.log_mqtt(f"识别结果已发送回设备（已重试3次）: {clean_result}")

            return recognition_result
        else:
            error_msg = "识别失败，请重试"
            if not is_mqtt:
                def update_error():
                    self.result_text.delete(1.0, tk.END)
                    self.result_text.insert(tk.END, error_msg + "\n")

                self.safe_update_ui(update_error)
            else:
                self.log_mqtt(error_msg)

            return error_msg

    def recognize_received_image(self):
        """识别接收到的图像"""
        if not self.received_image_complete or not hasattr(self, 'reconstructed_image_path'):
            messagebox.showerror("错误", "未接收到完整图像")
            return

        # 检查Access Token
        if not self.access_token:
            messagebox.showerror("错误", "请先获取Access Token")
            return

        # 识别图像
        result = self.recognize_image(self.reconstructed_image_path, is_mqtt=True)

    def connect_mqtt(self):
        """连接到MQTT代理服务器"""
        if self.mqtt_connected:
            return

        try:
            # 创建唯一的客户端ID
            client_id = f'python-mqtt-{uuid.uuid4().hex[:8]}'

            # 设置MQTT客户端，指定API版本
            # 对于paho-mqtt 2.0+
            if hasattr(mqtt, 'CallbackAPIVersion'):
                self.mqtt_client = mqtt.Client(client_id, callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
            else:
                # 兼容1.x版本
                self.mqtt_client = mqtt.Client(client_id)

            # 设置回调函数
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_message = self.on_mqtt_message
            self.mqtt_client.on_disconnect = self.on_mqtt_disconnect

            # 连接到MQTT代理服务器
            self.log_mqtt(f"正在连接到MQTT代理服务器 {self.mqtt_broker.get()}:{self.mqtt_port.get()}...")
            self.mqtt_client.connect(self.mqtt_broker.get(), self.mqtt_port.get(), 60)

            # 启动MQTT循环
            self.mqtt_client.loop_start()

            # 更新UI状态 - 在主线程中安全更新
            self.safe_update_ui(lambda: self.mqtt_connect_button.config(state=tk.DISABLED))
            self.safe_update_ui(lambda: self.mqtt_disconnect_button.config(state=tk.NORMAL))

        except Exception as e:
            self.log_mqtt(f"MQTT连接错误: {str(e)}")
            messagebox.showerror("MQTT错误", f"无法连接到MQTT代理服务器: {str(e)}")

    def disconnect_mqtt(self):
        """断开MQTT连接"""
        if self.mqtt_client and self.mqtt_connected:
            try:
                # 发布离线状态
                self.mqtt_client.publish("stm32/status",
                                         json.dumps({
                                             "status": "offline",
                                             "device": "python_client",
                                             "time": int(time.time())
                                         }))

                # 停止MQTT循环并断开连接
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()

                # 更新状态
                self.mqtt_connected = False

                # 在主线程中安全更新UI
                self.safe_update_ui(lambda: self.mqtt_status_label.config(text="状态: 未连接"))
                self.safe_update_ui(lambda: self.mqtt_connect_button.config(state=tk.NORMAL))
                self.safe_update_ui(lambda: self.mqtt_disconnect_button.config(state=tk.DISABLED))

                self.log_mqtt("已断开MQTT连接")
            except Exception as e:
                self.log_mqtt(f"断开MQTT连接时出错: {str(e)}")
        else:
            self.log_mqtt("MQTT未连接，无需断开")

    def on_mqtt_connect(self, client, userdata, flags, rc, properties=None):
        """MQTT连接回调函数"""
        if rc == 0:
            self.mqtt_connected = True

            # 在主线程中安全更新UI
            self.safe_update_ui(lambda: self.mqtt_status_label.config(text="状态: 已连接"))
            self.log_mqtt("已连接到MQTT代理服务器")

            # 订阅主题
            client.subscribe("stm32/image/info")
            client.subscribe("stm32/image/data/#")
            client.subscribe("stm32/image/end")
            client.subscribe("stm32/status")

            # 添加新的订阅主题
            client.subscribe("stm32/image/status")
            client.subscribe("stm32/image/retrans/complete")

            # 新增：订阅结果请求主题
            client.subscribe("stm32/request_result")
            self.log_mqtt("已订阅结果请求主题")

            # 订阅音频相关主题
            client.subscribe("stm32/audio/status")

            # 发布上线状态
            client.publish("stm32/status",
                           json.dumps({
                               "status": "online",
                               "device": "python_client",
                               "time": int(time.time())
                           }))

            self.log_mqtt("已订阅MQTT主题，等待接收图片...")
        else:
            self.mqtt_connected = False

            # 在主线程中安全更新UI
            self.safe_update_ui(lambda: self.mqtt_status_label.config(text=f"状态: 连接失败 ({rc})"))
            self.log_mqtt(f"MQTT连接失败，返回码: {rc}")

    def on_mqtt_disconnect(self, client, userdata, rc, properties=None):
        """MQTT断开连接回调函数"""
        self.mqtt_connected = False

        # 在主线程中安全更新UI
        self.safe_update_ui(lambda: self.mqtt_status_label.config(text="状态: 未连接"))
        self.log_mqtt(f"MQTT连接已断开，返回码: {rc}")

        # 在主线程中安全更新按钮状态
        self.safe_update_ui(lambda: self.mqtt_connect_button.config(state=tk.NORMAL))
        self.safe_update_ui(lambda: self.mqtt_disconnect_button.config(state=tk.DISABLED))

    def on_mqtt_message(self, client, userdata, msg, properties=None):
        """MQTT消息接收回调函数"""
        topic = msg.topic
        payload = msg.payload

        # 处理结果请求
        if topic == "stm32/request_result":
            try:
                self.log_mqtt("收到结果请求，重新发送最近的识别结果")
                # 检查是否有最近的识别结果
                if hasattr(self, 'last_recognition_result') and self.last_recognition_result:
                    # 多次发送结果以提高可靠性
                    for i in range(3):
                        self.mqtt_client.publish("stm32/result", self.last_recognition_result)
                        self.log_mqtt(f"重新发送结果 (尝试 {i + 1}/3): {self.last_recognition_result}")
                        time.sleep(0.2)  # 短暂延迟，避免消息拥堵

                    # 同时将结果保存到文件
                    self.save_result_to_file(self.last_recognition_result)
                else:
                    self.log_mqtt("没有可用的识别结果可以发送")
            except Exception as e:
                self.log_mqtt(f"处理结果请求失败: {str(e)}")

        # 图像信息
        elif topic == "stm32/image/info":
            try:
                info = json.loads(payload.decode('utf-8'))
                self.received_image_info = info
                self.received_image_data = {}  # 清空之前的数据
                self.received_image_complete = False
                self.log_mqtt(f"接收到图像信息: {info}")
            except Exception as e:
                self.log_mqtt(f"解析图像信息失败: {str(e)}")

        # 图像数据块
        elif topic.startswith("stm32/image/data/"):
            try:
                chunk_index = int(topic.split('/')[-1])
                self.received_image_data[chunk_index] = payload
                self.log_mqtt(f"接收到图像数据块 {chunk_index}, 大小: {len(payload)} 字节")
            except Exception as e:
                self.log_mqtt(f"处理图像数据块失败: {str(e)}")

        # 图像状态信息（含块总数）
        elif topic == "stm32/image/status":
            try:
                status = json.loads(payload.decode('utf-8'))
                self.log_mqtt(f"接收到图像状态信息: {status}")

                # 处理块状态信息
                if 'total_chunks' in status and 'last_chunk_index' in status:
                    total_chunks = status['total_chunks']
                    last_chunk_index = status['last_chunk_index']
                    self.expected_total_chunks = total_chunks

                    self.log_mqtt(f"图像共有 {total_chunks} 个数据块，最后索引: {last_chunk_index}")

                    # 检查缺失的块
                    missing_chunks = []
                    for i in range(total_chunks):
                        if i not in self.received_image_data:
                            missing_chunks.append(i)

                    if missing_chunks:
                        # 生成丢失块列表
                        missing_str = ",".join(map(str, missing_chunks))
                        self.log_mqtt(f"丢失的数据块: {missing_str}")

                        # 请求重传丢失的块
                        self.log_mqtt("请求重传丢失的数据块")
                        client.publish("stm32/image/retrans/req", missing_str)
                    else:
                        self.log_mqtt("已接收所有数据块，无需重传")
                        # 处理完整图像
                        self.process_received_image()
            except Exception as e:
                self.log_mqtt(f"处理图像状态信息失败: {str(e)}")

        # 重传完成通知
        elif topic == "stm32/image/retrans/complete":
            try:
                self.log_mqtt("重传完成，处理图像")
                # 处理图像
                self.process_received_image()
            except Exception as e:
                self.log_mqtt(f"处理重传完成通知失败: {str(e)}")

        # 图像传输结束
        elif topic == "stm32/image/end":
            try:
                self.log_mqtt("接收到图像传输结束信号")

                # 如果所有块都已接收，则处理图像
                if self.expected_total_chunks > 0 and len(self.received_image_data) >= self.expected_total_chunks:
                    self.log_mqtt("图像传输完成，开始处理")
                    self.process_received_image()
                else:
                    # 仍有缺失块，检查并请求重传
                    self.log_mqtt("图像传输结束但有缺失块，检查状态")
            except Exception as e:
                self.log_mqtt(f"处理图像传输结束失败: {str(e)}")

        # 设备状态
        elif topic == "stm32/status":
            try:
                status = json.loads(payload.decode('utf-8'))
                self.log_mqtt(f"设备状态更新: {status}")
            except Exception as e:
                self.log_mqtt(f"解析状态消息失败: {str(e)}")

        # 音频状态
        elif topic == "stm32/audio/status":
            try:
                status = json.loads(payload.decode('utf-8'))
                self.log_mqtt(f"收到音频状态消息: {status}")

                # 处理音频状态反馈
                if 'status' in status:
                    if status['status'] == 'playing':
                        self.log_mqtt("STM32正在播放音频")
                    elif status['status'] == 'complete':
                        self.log_mqtt("STM32已完成音频播放")
                    elif status['status'] == 'error':
                        self.log_mqtt(f"STM32播放音频出错: {status.get('error', '未知错误')}")
            except Exception as e:
                self.log_mqtt(f"处理音频状态消息失败: {str(e)}")

    def process_received_image(self):
        """处理接收到的图像"""
        if not self.received_image_info:
            self.log_mqtt("无图像信息，无法处理")
            return

        # 检查是否有数据块
        if not self.received_image_data:
            self.log_mqtt("未接收到图像数据块")
            return

        # 组装完整图像数据
        try:
            # 按照索引排序
            max_index = max(self.received_image_data.keys())
            self.log_mqtt(f"共接收到 {len(self.received_image_data)} 个数据块，最大索引: {max_index}")

            # 再次检查块是否完整
            missing_chunks = []
            for i in range(max_index + 1):
                if i not in self.received_image_data:
                    missing_chunks.append(i)

            if missing_chunks:
                missing_str = ",".join(map(str, missing_chunks))
                self.log_mqtt(f"仍有缺失数据块: {missing_chunks}")
                # 最后再尝试一次重传请求
                self.mqtt_client.publish("stm32/image/retrans/req", missing_str)
                self.log_mqtt("已请求最终重传")
                return

            # 组装数据
            image_data = bytearray()
            for i in range(max_index + 1):
                if i in self.received_image_data:  # 安全检查
                    image_data.extend(self.received_image_data[i])

            # 检查大小
            expected_size = self.received_image_info.get('size', 0)
            actual_size = len(image_data)
            self.log_mqtt(f"图像大小: {actual_size}/{expected_size} 字节")

            # 即使大小不完全匹配也继续处理（容错）
            if abs(actual_size - expected_size) > expected_size * 0.1:  # 如果差异超过10%则警告
                self.log_mqtt(f"警告: 图像大小差异较大，可能影响质量")

            # 保存图像
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = self.received_image_info.get('filename', f"received_{timestamp}.jpg")

            # 提取文件名而不是路径
            if '/' in filename:
                filename = filename.split('/')[-1]

            # 确保目录存在
            os.makedirs("received", exist_ok=True)
            save_path = os.path.join("received", filename)

            with open(save_path, 'wb') as f:
                f.write(image_data)

            self.log_mqtt(f"图像已保存至 {save_path}")

            # 显示预览
            self.display_preview(image_data, self.mqtt_preview_label)

            # 更新标签和按钮
            self.safe_update_ui(lambda: self.mqtt_preview_label.config(text=filename))
            self.safe_update_ui(lambda: self.mqtt_recognize_button.config(state=tk.NORMAL))

            # 保存到临时变量，以便后续识别
            self.reconstructed_image_data = image_data
            self.reconstructed_image_path = save_path

            # 标记图像已完成接收
            self.received_image_complete = True

            # 检查Access Token并自动识别图像
            if self.access_token:
                self.log_mqtt("图像接收完成，开始自动识别...")
                self.recognize_received_image()
            else:
                self.log_mqtt("未获取百度AI access_token，尝试重新获取...")

                # 重新获取Token并在获取后识别图像
                def get_token_and_recognize():
                    self.auto_get_token()
                    if self.access_token:
                        self.recognize_received_image()
                    else:
                        self.log_mqtt("无法获取Access Token，请稍后手动点击识别按钮")

                # 延迟执行，给一些时间显示消息
                self.root.after(1000, get_token_and_recognize)

        except Exception as e:
            self.log_mqtt(f"处理图像失败: {str(e)}")
            import traceback
            self.log_mqtt(traceback.format_exc())

    def log_mqtt(self, message):
        """向MQTT日志添加消息 - 线程安全版"""
        # 格式化时间
        timestamp = datetime.now().strftime("%H:%M:%S")
        log_message = f"[{timestamp}] {message}\n"

        # 在主线程中安排日志更新
        def update_log():
            self.mqtt_log.config(state=tk.NORMAL)
            self.mqtt_log.insert(tk.END, log_message)
            self.mqtt_log.see(tk.END)  # 滚动到最新消息
            self.mqtt_log.config(state=tk.DISABLED)

        self.safe_update_ui(update_log)

    def mqtt_message_loop(self):
        """后台消息处理循环"""
        while True:
            if self.mqtt_client and self.mqtt_connected:
                try:
                    # 处理可能的消息
                    pass
                except Exception as e:
                    self.log_mqtt(f"MQTT消息处理错误: {str(e)}")
            time.sleep(0.1)  # 小延迟，避免CPU占用过高

    def on_closing(self):
        """窗口关闭时的处理"""
        if self.mqtt_connected:
            self.disconnect_mqtt()
        self.root.destroy()


def main():
    root = tk.Tk()
    app = BaiduImageRecognitionApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()