 /*
 * Button Debounce using Finite State Machine
 *
 * IDLE: Button is released and stable. Waiting for a press.
 * PRESSING: Pin went LOW but we are not sure yet — could be bounce. Waiting for debounce timer.
 * PRESSED: Pin has been LOW for long enough — confirmed press.
 * RELEASING: Pin went HIGH but debounce timer not expired — could be bounce.
 * 
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define BUTTON_GPIO     GPIO_NUM_4   /* change to your pin */
#define TAG             "BUTTON_FMS"
#define DEBOUNCE_MS     150 // Adjust 15-50ms per your button

// FSM states
typedef enum {
  BTN_IDLE,      // Released, stable
  BTN_PRESSING,  // Went LOW, debouncing
  BTN_PRESSED,   // Confirmed pressed
  BTN_RELEASING  // Went HIGH, debouncing
} ButtonState;

static ButtonState btnState     = BTN_IDLE;
static int64_t debounceTimer    = 0;

static volatile bool buttonEvent    = false;  
static volatile bool releaseEvent   = false;
static volatile bool led_state      = false;

void updateButton(void)
{
    int     rawPin  = gpio_get_level(BUTTON_GPIO);
    int64_t now     = esp_timer_get_time();
    //ESP_LOGI(TAG, "rawPin=%d state=%d", rawPin, btnState);
    
    switch (btnState) {
        case BTN_IDLE:
            if (rawPin == 0) {
                btnState = BTN_PRESSING;// Possible press detected — start debounce timer
                debounceTimer = now;
            }
            break;
        
        case BTN_PRESSING:
            if (rawPin == 1) {
                btnState = BTN_IDLE;// Bounced back before timeout — false alarm
            } else if (now - debounceTimer >= DEBOUNCE_MS * 1000LL) {
                btnState = BTN_PRESSED;// Stable LOW for full debounce period — genuine press!
                buttonEvent = true;  // Signal a press event
            }
            break;
        
        case BTN_PRESSED:
            if (rawPin == 1) {
                btnState = BTN_RELEASING; // Button released — start release debounce
                debounceTimer = now;
            }
            break;
            
        case BTN_RELEASING:
            if (rawPin == 0) {
                btnState = BTN_PRESSED; // Bounced back during release — still pressed
            } else if (now - debounceTimer >= DEBOUNCE_MS * 1000LL) {
                btnState = BTN_IDLE; // Stable HIGH for full debounce period — genuine release!
                releaseEvent = true;
            }
            break;
    }
}

void app_main(void)
{
    // Set led pin
    gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_15, 0);
    
    // Set button pin
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,      // ← критично
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,       // ← без переривань
    };
    gpio_config(&btn_conf);

    ESP_LOGI(TAG,"Button GPIO: %d", BUTTON_GPIO);

    while (true) {
        updateButton();
        
        if (buttonEvent) {
            buttonEvent = false;
            
            led_state = !led_state;
            gpio_set_level(GPIO_NUM_15, led_state);

            ESP_LOGI(TAG, "PRESS = %d, LED state = %d",
                    (int)gpio_get_level(BUTTON_GPIO) ,(int)led_state);
        }

        if (releaseEvent) {
            releaseEvent = false;
            ESP_LOGI(TAG, "RELEASE = %d, LED state = %d",
                    (int)gpio_get_level(BUTTON_GPIO) ,(int)led_state);   
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}