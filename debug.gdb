# ESP32-P4 Debug Script
# 使用方法: riscv32-esp-elf-gdb -x debug.gdb build/ESP32P4.elf

# 设置架构
set architecture riscv:rv32

# 连接到 OpenOCD
target remote :3333

# 复位并停止
monitor reset halt

# 加载程序到目标设备
load

# 设置断点在 app_main
break app_main

# 设置断点在 init_esp_video_dvp (如果你想调试初始化)
break init_esp_video_dvp

# 设置断点在 test_camera_info (第一个测试函数)
break test_camera_info

# 显示信息
echo \n
echo ====================================\n
echo ESP32-P4 Debug Session Started\n
echo ====================================\n
echo Breakpoints set at:\n
echo   - app_main\n
echo   - init_esp_video_dvp\n
echo   - test_camera_info\n
echo \n
echo Type 'continue' or 'c' to start\n
echo ====================================\n
echo \n

# 可选: 自动继续运行
# continue
