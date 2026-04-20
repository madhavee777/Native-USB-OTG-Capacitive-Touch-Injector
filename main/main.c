#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"

// Define the Touch Pad and a baseline threshold
#define TOUCH_PAD_NUM TOUCH_PAD_NUM1 
#define TOUCH_THRESHOLD 30000 // You will need to calibrate this based on your jumper wire

// --- TinyUSB HID Callbacks and Descriptor ---

// 1. Define the HID Report Descriptor for a Standard Keyboard
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

// 2. Callback: Invoked when the host requests the report descriptor
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    // Tells the PC: "I am a keyboard"
    return desc_hid_report;
}

// 3. Callback: Invoked when the host wants to read the current state (GET_REPORT)
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    // We don't need to support reading state for a simple injector
    return 0; 
}

// 4. Callback: Invoked when the host sends data to the device (SET_REPORT)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    // The host sends data here for things like turning on the Num Lock or Caps Lock LEDs.
    // We can safely ignore this for our project.
}

void touch_keyboard_task(void *pvParameters) {
    uint32_t touch_value;
    bool pressed = false;

    while (1) {
        // Read the capacitance value
        touch_pad_read_raw_data(TOUCH_PAD_NUM, &touch_value);

        // Check if the wire was touched (capacitance changes)
        if (touch_value > TOUCH_THRESHOLD && !pressed) {
            
            // 1. Prepare the HID Keyboard report (e.g., Spacebar)
            uint8_t keycode[6] = {HID_KEY_SPACE};
            
            // 2. Send the key press to the host OS
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            pressed = true;

            // 3. Simple debounce delay
            vTaskDelay(pdMS_TO_TICKS(100));

            // 4. Send empty report to simulate key release
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
            
        } else if (touch_value < TOUCH_THRESHOLD) {
            pressed = false; // Reset state when wire is released
        }

        // Yield to the RTOS scheduler
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}

void app_main(void) {
    // 1. Initialize the Touch Pad Peripheral
    touch_pad_init();
    touch_pad_config(TOUCH_PAD_NUM);
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();

    // 2. Initialize TinyUSB with default configurations from menuconfig
    const tinyusb_config_t tusb_cfg = { 0 };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // 3. Spin up the polling task
    xTaskCreate(touch_keyboard_task, "touch_kb_task", 4096, NULL, 5, NULL);
}