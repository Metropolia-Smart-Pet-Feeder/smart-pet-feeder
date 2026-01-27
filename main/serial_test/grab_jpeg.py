import serial
import re

ser = serial.Serial('/dev/ttyACM0', 5*1024000)  # 改成你的串口
data = ""
capturing = False

while True:
    line = ser.readline().decode('utf-8', errors='ignore')

    if "JPEG_START" in line:
        data = ""
        capturing = True
    elif "JPEG_END" in line:
        # 转成二进制
        jpeg_bytes = bytes.fromhex(data)
        with open('image.jpg', 'wb') as f:
            f.write(jpeg_bytes)
        print(f"保存图片,大小:{len(jpeg_bytes)}")
        capturing = False
    elif capturing:
        data += line.strip()