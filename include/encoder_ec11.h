/**
 * @file encoder_ec11.h
 * 
**/
#ifndef ENCODER_EC11_H
#define ENCODER_EC11_H

#include "esp_err.h"

typedef void *encoder_ec11_handle_t;
typedef void (* ec11_cb_t)(void *);

/**
 * @brief Supported encoder type.
 *        Number of pulses in 360° rotation.
 *
 */
typedef enum {
    ONE_POSITION_ONE_PULSE = 0, /**< 20pulses/360° */
    TWO_POSITION_ONE_PULSE,     /**< 15pulses/360° */
}ec11_type_t;

/**
 * @brief EC11 encoder events
 *
 */
typedef enum {
    EC11_DIRECTION_CW = 0, /**< Clockwise direction */
    EC11_DIRECTION_CCW,    /**< Counterclockwise direction */
    EC11_EVENT_MAX,
    EC11_NONE,
    EC11_ENCODER_NOT_EXIST,
}ec11_encoder_event_t;

/**
 * @brief EC11 button events
 *
 */
typedef enum {
    EC11_BNT_PRESS_DOWN = 0,
    EC11_BNT_PRESS_UP,
    EC11_BNT_PRESS_REPEAT,
    EC11_BNT_SINGLE_CLICK,
    EC11_BNT_DOUBLE_CLICK,
    EC11_BNT_LONG_PRESS_START,
    EC11_BNT_LONG_PRESS_HOLD,
    EC11_BNT_EVENT_MAX,
    EC11_BNT_NONE_PRESS,
    EC11_BNT_NOT_EXIST,
} ec11_bnt_event_t;

typedef enum {
    LEVEL_LOW = 0,
    LEVEL_HIGH,
} signal_level_t;

/**
 * @brief EC11 configuration
 *
 */
typedef struct {
    ec11_type_t ec11_type;
    uint32_t        signal_A_gpio_num; 
    uint32_t        signal_B_gpio_num;
    signal_level_t  button_active_level;
    uint32_t        button_gpio_num;
}ec11_config_t;

/**
 * @brief Short name of EC11 handle
 *
 */
typedef encoder_ec11_handle_t ec11_handle_t;

/**
 * @brief Create a EC11
 *
 * @param config pointer of EC11 configuration, must corresponding the EC11 type (ec11_type).
 *               if signal_A_gpio_num or signal_B_gpio_num is set to -1 means do not use the encoder.
 *               if button_gpio_num is set to -1 means do not use the button.
 *
 * @return A handle to the created EC11, or NULL in case of error.
 */
encoder_ec11_handle_t encoder_ec11_create(const ec11_config_t * config);

/**
 * @brief Delete a EC11
 *
 * @param ec11_handle A EC11 handle to delete
 *
 * @return
 *      - ESP_OK  Success
 *      - ESP_FAIL Failure
 */
esp_err_t encoder_ec11_delete(encoder_ec11_handle_t ec11_handle);

/**
 * @brief Register event callback function of EC11 button.
 *
 * @param ec11_handle A EC11 handle to register
 * @param event EC11 button event
 * @param cb Callback function.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t ec11_button_register_cb(encoder_ec11_handle_t ec11_handle, ec11_bnt_event_t event, ec11_cb_t cb);

/**
 * @brief Register event callback function of EC11 encoder.
 *
 * @param ec11_handle A EC11 handle to register
 * @param event EC11 encoder event
 * @param cb Callback function.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t ec11_encoder_register_cb(encoder_ec11_handle_t ec11_handle, ec11_encoder_event_t event, ec11_cb_t cb);

/**
 * @brief Unregister event callback function of EC11 button.
 *
 * @param ec11_handle A EC11 handle to unregister
 * @param event EC11 button event
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t ec11_button_unregister_cb(encoder_ec11_handle_t ec11_handle, ec11_bnt_event_t event);

/**
 * @brief Unregister event callback function of EC11 encoder.
 *
 * @param ec11_handle A EC11 handle to unregister
 * @param event EC11 encoder event
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t ec11_encoder_unregister_cb(encoder_ec11_handle_t ec11_handle, ec11_encoder_event_t event);

/**
 * @brief Get EC11 button event
 *
 * @param ec11_handle EC11 handle
 *
 * @return Current EC11 button event. See ec11_bnt_event_t
 */
ec11_bnt_event_t ec11_button_get_event(encoder_ec11_handle_t ec11_handle);

/**
 * @brief Get EC11 button repeat times
 *
 * @param ec11_handle EC11 handle
 *
 * @return EC11 button pressed times. For example, double-click return 2, triple-click return 3, etc.
 */
uint8_t ec11_button_get_repeat(encoder_ec11_handle_t ec11_handle);

/**
 * @brief Get EC11 encoder event
 *
 * @param ec11_handle EC11 handle
 *
 * @return Current EC11 encoder event. See ec11_encoder_event_t
 */
ec11_encoder_event_t ec11_encoder_get_event(encoder_ec11_handle_t ec11_handle);

/**
 * @brief Get accumulated number of pulses obtained from EC11 encoder
 * 
 * @param ec11_handle EC11 handle
 *
 * @return Accumulation of all pulse counts.
 *         The encoder rotates one pulse clockwise, the counter increases by one, 
 *         and when the encoder rotates one pulse counterclockwise, the counter decreases by one.
 *         For example, rotates clockwise for two pulses return 2, 
 *         then rotates two pulses counterclockwise return 0,
 *         finally rotates two pulses counterclockwise return -2. 
 */
int16_t c11_encoder_get_pulse_cnt(encoder_ec11_handle_t ec11_handle);

#endif /*ENCODER_EC11_H*/
