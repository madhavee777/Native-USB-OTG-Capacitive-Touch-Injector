#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "tinyusb_default_config.h"

// Define the Touch Pad and a baseline threshold
#define TOUCH_PAD_NUM TOUCH_PAD_NUM1 
#define TOUCH_THRESHOLD 30000 // calibrate this later

// --- 1. TinyUSB Manual Descriptors ---

// The HID Report (Tells the PC: "I am a keyboard")
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

// The Device Descriptor (The physical ID card of the USB chip)
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A, // Espressif Vendor ID
    .idProduct          = 0x4001, // Custom Product ID
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// The Configuration Descriptor (Power and Endpoint settings)
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), 0x81, 16, 10)
};

// The String Descriptors (Human-readable names)
static const char* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 }, // 0: Supported language is English
    "Custom Corp",                 // 1: Manufacturer
    "ESP32-S3 Touch Injector",     // 2: Product
    "123456"                       // 3: Serial Number
};

// --- 2. Standard HID Callbacks (Required by TinyUSB) ---

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    return desc_hid_report;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    return 0; 
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
}

// --- 3. The Main Application Task ---

void touch_keyboard_task(void *pvParameters) {
    uint32_t touch_value;
    bool pressed = false;

    while (1) {
        // Read the capacitance value
        touch_pad_read_raw_data(TOUCH_PAD_NUM, &touch_value);
        
        // Print the value so you can calibrate!
        printf("Current Touch Value: %" PRIu32 "\n", touch_value);

        // Flipped logic: We check if it drops BELOW the threshold
        if (touch_value < TOUCH_THRESHOLD && !pressed) {
            
            printf("Touch Detected! Injecting Spacebar...\n");

            // 1. Prepare the HID Keyboard report (Spacebar)
            uint8_t keycode[6] = {HID_KEY_SPACE};
            
            // 2. Send the key press to the host OS
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
            pressed = true;

            // 3. Simple debounce delay
            vTaskDelay(pdMS_TO_TICKS(100));

            // 4. Send empty report to simulate key release
            tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
            
        // Check if the value has returned to baseline
        } else if (touch_value > TOUCH_THRESHOLD) {
            pressed = false; // Reset state when wire is released
        }

        // Yield to the RTOS scheduler
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    // 1. Initialize the Touch Pad Peripheral
    touch_pad_init();
    touch_pad_config(TOUCH_PAD_NUM);
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();

    // 2. Initialize TinyUSB with default task memory
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    
    // 3. Inject custom descriptors using the nested struct
    tusb_cfg.descriptor.device = &desc_device;
    tusb_cfg.descriptor.string = string_desc_arr;
    tusb_cfg.descriptor.full_speed_config = desc_configuration;

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // 4. Spin up the polling task
    xTaskCreate(touch_keyboard_task, "touch_kb_task", 4096, NULL, 5, NULL);
}