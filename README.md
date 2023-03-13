# 旋转编码器 V1.0  
适用于IDF 4.*

# 如何使用?
* 包含头文件 `encoder_ec11.h`
* 硬件初始化
```c
    ec11_config_t cfg = {
        .ec11_type = ONE_POSITION_ONE_PULSE,
        .signal_A_gpio_num = 12, //两路编码器输出引脚
        .signal_B_gpio_num = 18,
        .button_active_level = LEVEL_LOW, //设置触发电平
        .button_gpio_num = 5 //设置按键引脚
    };
    
    ec11_handle_t ec11_handle = encoder_ec11_create(&cfg);
```

* 注册回调函数
```c
ec11_button_register_cb(ec11_handle, EC11_BNT_PRESS_DOWN, ec11_button_cb);
```

* 回调函数
```c
static void ec11_button_cb(void *arg)
{
    encoder_ec11_handle_t handle = (encoder_ec11_handle_t) arg;

    ec11_bnt_event_t event = (uint8_t)ec11_button_get_event(handle);

    if (event == EC11_BNT_PRESS_DOWN) {
        //TODO
    } else if (event == EC11_BNT_PRESS_UP) {
        //TODO
    }
}
```
