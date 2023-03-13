/**
 * @file encoder_ec11.c
 *
 * @version v1.0
 * 
 * @date: 2022-6-10
 * 
 **/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "encoder_ec11.h"

static const char *TAG = "ec11";


#define TICKS_INTERVAL    5
#define DEBOUNCE_TICKS    2 //MAX 8
#define SHORT_TICKS       (180 /TICKS_INTERVAL)
#define LONG_TICKS        (1500 /TICKS_INTERVAL)

#define EC11_CHECK(a, str, ret_val)                               \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define CALL_EC11_ENCODER_CB(dev,ev) if(dev->encoder->cb[ev])dev->encoder->cb[ev](dev)
#define CALL_EC11_BUTTON_CB(dev,ev) if(dev->button->cb[ev])dev->button->cb[ev](dev)

typedef struct {
    uint16_t            ticks;
    uint8_t             repeat;
    ec11_bnt_event_t    event;
    uint8_t             state : 3;
    uint8_t             debounce_cnt : 3;
    signal_level_t      active_level : 1;
    uint8_t             level: 1;
    ec11_cb_t           cb[EC11_BNT_EVENT_MAX];
    uint32_t            gpio_num;
} ec11_btn_dev_t;

typedef struct {
    signal_level_t       a_pre_state : 1;
    signal_level_t       b_pre_state : 1;
    uint32_t             a_gpio_num;
    uint32_t             b_gpio_num;
    ec11_encoder_event_t  event;
    ec11_type_t          type;
    int16_t              pulse_cnt;
    ec11_cb_t            cb[EC11_EVENT_MAX];
} ec11_encoder_dev_t;

typedef struct encoder_ec11 {
    ec11_encoder_dev_t *encoder;
    ec11_btn_dev_t *button;
    struct encoder_ec11 *next;
} ec11_dev_t;

static ec11_dev_t *g_head_handle = NULL;
static esp_timer_handle_t g_ec11_timer_handle;
static bool g_is_timer_running = false;
//uint8_t g_index = 0;

static void ec11_handler(ec11_dev_t *ec11_dev)
{
    if (NULL != ec11_dev->encoder) {
        signal_level_t A_cur_state = gpio_get_level((uint32_t)ec11_dev->encoder->a_gpio_num);
        signal_level_t B_cur_state = gpio_get_level((uint32_t)ec11_dev->encoder->b_gpio_num);
        signal_level_t A_pre_state = ec11_dev->encoder->a_pre_state;

        if (ONE_POSITION_ONE_PULSE == ec11_dev->encoder->type)
        {
            if (A_cur_state != A_pre_state)
            {
                if (A_cur_state == LEVEL_LOW)
                {
                    if (B_cur_state == LEVEL_HIGH)
                    {
                        ec11_dev->encoder->event = EC11_DIRECTION_CW;
                        ec11_dev->encoder->pulse_cnt++;
                        CALL_EC11_ENCODER_CB(ec11_dev, EC11_DIRECTION_CW);
                    } else {
                        ec11_dev->encoder->event = EC11_DIRECTION_CCW;
                        ec11_dev->encoder->pulse_cnt--;
                        CALL_EC11_ENCODER_CB(ec11_dev, EC11_DIRECTION_CCW);
                    }
                }
                ec11_dev->encoder->a_pre_state = A_cur_state;
                ec11_dev->encoder->b_pre_state = B_cur_state;
            }
        }
    } else { /*TWO_POSITION_ONE_PULSE*/
        // TODO
    }

    /*button handle*/
    if (NULL != ec11_dev->button)
    {
        uint8_t read_bnt_level = gpio_get_level((uint32_t)ec11_dev->button->gpio_num);

        /** ticks counter working.. */
        if (ec11_dev->button->state > 0) {
            ec11_dev->button->ticks++;
        }

        /**< button debounce handle */
        if (read_bnt_level != ec11_dev->button->level) {
            if(++(ec11_dev->button->debounce_cnt) >= DEBOUNCE_TICKS) {
                ec11_dev->button->level = read_bnt_level;
                ec11_dev->button->debounce_cnt = 0;
            }
                       
        } else {
            ec11_dev->button->debounce_cnt = 0;
        }
        //ESP_LOGE(CB, "ec11_dev->button->level : %d", ec11_dev->button->level);
        /** State machine */
        switch (ec11_dev->button->state) {
            case 0:
                if (ec11_dev->button->level == ec11_dev->button->active_level) {
                    ec11_dev->button->event = EC11_BNT_PRESS_DOWN;
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_PRESS_DOWN); //event callback
                    ec11_dev->button->ticks = 0;
                    ec11_dev->button->repeat = 1;
                    ec11_dev->button->state = 1;
                } else {
                   ec11_dev->button->event = EC11_BNT_NONE_PRESS; 
                }
            break;

            case 1:
                if (ec11_dev->button->level != ec11_dev->button->active_level) {
                    ec11_dev->button->event = EC11_BNT_PRESS_UP;
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_PRESS_UP); //event callback
                    ec11_dev->button->ticks = 0;
                    ec11_dev->button->state = 2;
                } else if (ec11_dev->button->ticks > LONG_TICKS) {
                    ec11_dev->button->event = EC11_BNT_LONG_PRESS_START;
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_LONG_PRESS_START); //event callback
                    ec11_dev->button->state = 4;
                }
            break;

            case 2:
                if (ec11_dev->button->level == ec11_dev->button->active_level) {
                    ec11_dev->button->event = EC11_BNT_PRESS_DOWN;
                    ec11_dev->button->repeat++;
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_PRESS_REPEAT); //event callback
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_PRESS_DOWN); //event callback
                    ec11_dev->button->ticks = 0;
                    ec11_dev->button->state = 3;
                } else if (ec11_dev->button->ticks > SHORT_TICKS) {
                    if (ec11_dev->button->repeat == 1) {
                        ec11_dev->button->event = EC11_BNT_SINGLE_CLICK;
                        CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_SINGLE_CLICK); //event callback
                    } else if (ec11_dev->button->repeat == 2) {
                        ec11_dev->button->event = EC11_BNT_DOUBLE_CLICK;
                        CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_DOUBLE_CLICK); //event callback
                    }
                    ec11_dev->button->state = 0;
                }
            break;

            case 3:
                if (ec11_dev->button->level != ec11_dev->button->active_level) {
                    ec11_dev->button->event = EC11_BNT_PRESS_UP;
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_PRESS_UP); //event callback
                    if (ec11_dev->button->ticks < SHORT_TICKS) {
                        ec11_dev->button->ticks = 0;
                        ec11_dev->button->state = 2;
                    } else {
                        ec11_dev->button->state = 0;
                    }
                }
            break;

            case 4:
                if (ec11_dev->button->level == ec11_dev->button->active_level) {
                    ec11_dev->button->event = EC11_BNT_LONG_PRESS_HOLD;
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_LONG_PRESS_HOLD); //event callback
                } else {
                    ec11_dev->button->event = EC11_BNT_PRESS_UP;
                    CALL_EC11_BUTTON_CB(ec11_dev, EC11_BNT_PRESS_UP); //event callback
                    ec11_dev->button->state = 0;
                }
            break;

            default : break;
        }
    }
}

static void ec11_cb(void *args)
{
    ec11_dev_t *target;
    for (target = g_head_handle; target; target = target->next) {
        ec11_handler(target);
    }
}

esp_err_t ec11_gpio_init(const ec11_config_t *cfg)
{
    gpio_config_t gpio_conf;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    if (-1 == cfg->button_gpio_num) {
        gpio_conf.pin_bit_mask = ((1ULL << cfg->signal_A_gpio_num) |
                                  (1ULL << cfg->signal_B_gpio_num));
    }
    else {
        gpio_conf.pin_bit_mask = ((1ULL << cfg->signal_A_gpio_num) |
                                  (1ULL << cfg->signal_B_gpio_num) |
                                  (1ULL << cfg->button_gpio_num));
    }

    gpio_config(&gpio_conf);

    return ESP_OK;
}

esp_err_t ec11_gpio_deinit(int gpio_num)
{
    /** both disable pullup and pulldown */
    gpio_config_t gpio_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << gpio_num),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&gpio_conf);
    return ESP_OK;
}

esp_err_t ec11_dev_init(ec11_dev_t *ec11)
{
    if (NULL != ec11->encoder) {
        ec11->encoder->a_pre_state = LEVEL_LOW;
        ec11->encoder->b_pre_state = LEVEL_LOW;
        ec11->encoder->event = EC11_NONE;
        ec11->encoder->pulse_cnt = 0;
    }

    if (NULL != ec11->button)
    {
        ec11->button->event = EC11_BNT_NONE_PRESS;
        ec11->button->state = 0; 
        ec11->button->ticks = 0;
        ec11->button->debounce_cnt = 0;
    }

    return ESP_OK;
}

encoder_ec11_handle_t encoder_ec11_create(const ec11_config_t *config)
{
    ec11_dev_t *ec11 = (ec11_dev_t *)calloc(1, sizeof(ec11_dev_t));
    EC11_CHECK(NULL != ec11, "ec11 memory alloc failed", NULL);

    if ((-1 != config->signal_A_gpio_num) && (-1 != config->signal_B_gpio_num)) {
        ec11_encoder_dev_t * encoder = (ec11_encoder_dev_t *)calloc(1, sizeof(ec11_encoder_dev_t));
        EC11_CHECK(NULL != encoder, "encoder memory alloc failed", NULL);
        ec11->encoder = encoder;
    } else {
        ec11->encoder = NULL;
    }

    if (-1 != config->button_gpio_num)
    {
        ec11_btn_dev_t *bnt = (ec11_btn_dev_t *)calloc(1, sizeof(ec11_btn_dev_t));
        EC11_CHECK(NULL != bnt, "bnt memory alloc failed", NULL);
        ec11->button = bnt;
    } else {
        ec11->button = NULL;
    }

    ec11_dev_init(ec11);
    if (NULL != ec11->encoder) {
        ec11->encoder->type = config->ec11_type;
        ec11->encoder->a_gpio_num = config->signal_A_gpio_num;
        ec11->encoder->b_gpio_num = config->signal_B_gpio_num;        
    }

    if (NULL != ec11->button)
    {
        ec11->button->gpio_num = config->button_gpio_num;
        if( (LEVEL_LOW != config->button_active_level) && (LEVEL_HIGH != config->button_active_level) ) {
            ec11->button->active_level = LEVEL_LOW;  //default level
        } else {
            ec11->button->active_level = config->button_active_level;
        }        
        ec11->button->level = !ec11->button->active_level;
    }

    ec11_gpio_init(config);

    /** Add handle to list */
    ec11->next = g_head_handle;
    g_head_handle = ec11;

    /*set ec11 timer*/
    if (false == g_is_timer_running)
    {
        esp_timer_create_args_t ec11_timer;
        ec11_timer.arg = NULL;
        ec11_timer.callback = ec11_cb;
        ec11_timer.dispatch_method = ESP_TIMER_TASK;
        ec11_timer.name = "ec11_timer";
        esp_timer_create(&ec11_timer, &g_ec11_timer_handle);
        esp_timer_start_periodic(g_ec11_timer_handle, TICKS_INTERVAL * 1000U);
        g_is_timer_running = true;
    }

    return (encoder_ec11_handle_t)ec11;
}

esp_err_t encoder_ec11_delete(encoder_ec11_handle_t ec11_handle)
{
    esp_err_t ret = ESP_OK;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;

    if(NULL != ec11->encoder) {
        ec11_gpio_deinit(ec11->encoder->a_gpio_num);
        ec11_gpio_deinit(ec11->encoder->b_gpio_num);
        free(ec11->encoder);
    }

    if(NULL != ec11->button) {
        ec11_gpio_deinit(ec11->button->gpio_num);
        free(ec11->button);
    }

    ec11_dev_t ** curr;
    for (curr = &g_head_handle; *curr; ) {
        ec11_dev_t * entry = *curr;
        if (entry == ec11) {
            *curr = entry->next;
            free(entry);
        } else {
            curr = &entry->next;
        }
    }

    /* count ec11 number */
    uint16_t number = 0;
    ec11_dev_t * target = g_head_handle;
    while (target) {
        target = target->next;
        number++;
    }
    ESP_LOGD(TAG, "remain ec11 number=%d", number);

    if (0 == number && g_is_timer_running) { /**<  if all button is deleted, stop the timer */
        esp_timer_stop(g_ec11_timer_handle);
        esp_timer_delete(g_ec11_timer_handle);
        g_is_timer_running = false;
    }

    return ret;
}

ec11_bnt_event_t ec11_button_get_event(encoder_ec11_handle_t ec11_handle)
{
    ec11_bnt_event_t event;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;

    if(NULL != ec11->button) {
        event = ec11->button->event;
    } else {
        event = EC11_BNT_NOT_EXIST;
    }

    return event;
}

uint8_t ec11_button_get_repeat(encoder_ec11_handle_t ec11_handle)
{
    uint8_t repeat;

    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", 0);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;

    if(NULL != ec11->button) {
        repeat = ec11->button->repeat;
    } else {
        repeat = -1;
    }
    return repeat;
}

ec11_encoder_event_t ec11_encoder_get_event(encoder_ec11_handle_t ec11_handle)
{
    ec11_encoder_event_t event;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;

    if(NULL != ec11->encoder) {
        event = ec11->encoder->event;
        ec11->encoder->event = EC11_NONE;
    } else {
        event = EC11_ENCODER_NOT_EXIST;
    }
    return event;
}

int16_t c11_encoder_get_pulse_cnt(encoder_ec11_handle_t ec11_handle)
{
    uint16_t pulse_cnt = 0;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;   

    if(NULL != ec11->encoder) {
        pulse_cnt = ec11->encoder->pulse_cnt;
    } 

    return pulse_cnt;
}

esp_err_t ec11_button_register_cb(encoder_ec11_handle_t ec11_handle, ec11_bnt_event_t event, ec11_cb_t cb)
{
    esp_err_t ret = ESP_OK;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    EC11_CHECK((event < EC11_BNT_EVENT_MAX), "event is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;
    if(NULL != ec11->button) {
        ec11->button->cb[event] = cb;
    } else {
        ret = ESP_FAIL;
    }
    return ret;
}

esp_err_t ec11_encoder_register_cb(encoder_ec11_handle_t ec11_handle, ec11_encoder_event_t event, ec11_cb_t cb)
{
    esp_err_t ret = ESP_OK;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    EC11_CHECK((event < EC11_EVENT_MAX), "event is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;   

    if (NULL != ec11->encoder) {
        ec11->encoder->cb[event] = cb;
    } else {
        ret = ESP_FAIL;
    }
    return ret;
}

esp_err_t ec11_button_unregister_cb(encoder_ec11_handle_t ec11_handle, ec11_bnt_event_t event)
{
    esp_err_t ret = ESP_OK;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    EC11_CHECK((event < EC11_BNT_EVENT_MAX), "event is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle;
    if(NULL != ec11->button) {
        ec11->button->cb[event] = NULL;
    } else {
        ret = ESP_FAIL;
    }
    return ret;
}

esp_err_t ec11_encoder_unregister_cb(encoder_ec11_handle_t ec11_handle, ec11_encoder_event_t event)
{
    esp_err_t ret = ESP_OK;
    EC11_CHECK(NULL != ec11_handle, "Pointer of handle is invalid", ESP_ERR_INVALID_ARG);
    EC11_CHECK((event < EC11_EVENT_MAX), "event is invalid", ESP_ERR_INVALID_ARG);
    ec11_dev_t * ec11 = (ec11_dev_t * ) ec11_handle; 

    if(NULL != ec11->encoder) {
        ec11->encoder->cb[event] = NULL;
    } else {
        ret = ESP_FAIL; 
    }
    return ret;
}



void ec11_test()
{
    ESP_LOGE(TAG, "ec11_dev size : %d", sizeof(ec11_dev_t));
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));

    }
}
