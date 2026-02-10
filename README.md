# Pet Feeder

Run config to align board configuration. It will apply sdkconfig.defaults

```bash
idf.py reconfigure
```

Things I changed: 
1. enables custom lvgl congifure
2. increase flash size
3. add custom partition table, now we have about 3.8MB
4. enable bluetooth