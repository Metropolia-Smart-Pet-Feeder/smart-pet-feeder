# Display and Network Components
This document is for explaining how the display and network features are implemented. I will briefly introduce all three components. One important note: I increased the app partition from 1mb to 3mb "single factory app (large)".

## Display
1. SPIBus: Basic spi bus configurations. DMA is enabled for pixel data transmission. This need to be improved for connecting more SPI device on the same bus.
2. Display: Initialize spi device io panel and ILI9341 driver. Provides interface for lvgl to trigger data transaction. Allow lvgl to set its lv_disp_flush_ready as display driver's dma callback. This ensures the data transation is asychronous.
3. I2CBus: Basic I2C bus configurations. Pretty straight-forward.
4. TouchDriver: Configure the touch driver. Creates semaphore for signaling touch in isr. Provide interface for lvgl to read touch data in its touch polling callback.
5. LVGLManager: Bridge class that connects hardwares and LVGL. LVGL has its own config file. We need to set LV_COLOR_16_SWAP to 1 for our display chip. In order to use this config file, run `idf.py menuconfig`, go to Component config -> LVGL configuration -> "Uncheck this to use custom lv_conf.h"

## EventBus
This is a wrapper of esp-idf's event loop. It works as a observer pattern that coordinates events across tasks. 
If you want to wait for a certain event signal, simply call "subscribe" method and pass the event name and a handler function pointer. For example: `event_bus->subscribe(EVENT_START_PROVISIONING, onStartProvisioningEvent, this);` If you want to send signal of a certain event, call `publish(EVENT_NAME)`
This seems to work pretty well and easy to manage. May consider using it in our project.

## WiFiManager
Obtain WiFi credentials via BLE provisioning. I tested with esp's provisioning app. If we make our own mobile app, it should also match esp's provisioning protocol. 
Once provisioned, esp idf's WiFi manager will automatically store the credentials in esp's nvs flash memory. It will automatically connect on the next reboot. 

## UI Demo
![Demo1](readme_assets/1.gif)
![Demo2](readme_assets/2.gif)
![Demo3](readme_assets/3.gif)

## Problem
The screen seems unstable, probably due to physical connection. I can make it behave normally if I wiggle the wire a bit. We may need to add a capicitor to make it stable.

## What's next
1. Research and choose a cloud platform for remote control. I'm thinking using a remote mqtt brocker such as HiveMQ. Each phone&device pair publish and subscribe to one mqtt topic.
2. Need to think of a way to allow only 1-2 phones to pair with one device. 
3. Need to consider enctyption and authentication and other safty issue.
4. Implement other local control.