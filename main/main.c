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
#define TOUCH_THRESHOLD 24000 // calibrate this later

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

// --- 2. Standard HID Callbacks (Required by TinyUSB) ---

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    return desc_hid_report;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    return 0; 
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
}

// Helper function to type out a full string safely
void type_string(const char *text) {
    // 1. Safety Check: Make sure the Mac has actually mounted the USB
    if (!tud_mounted()) {
        printf("USB is not mounted by the OS yet!\n");
        return;
    }

    while (*text) {
        uint8_t keycode[6] = {0};
        uint8_t modifier = 0;
        char c = *text;

        // Map ASCII character to HID Keycode
        if (c >= 'a' && c <= 'z') {
            keycode[0] = HID_KEY_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keycode[0] = HID_KEY_A + (c - 'A');
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT; 
        } else if (c >= '1' && c <= '9') {
            keycode[0] = HID_KEY_1 + (c - '1');
        } else if (c == '0') {
            keycode[0] = HID_KEY_0;
        } else if (c == ' ') {
            keycode[0] = HID_KEY_SPACE;
        } else if (c == '\n') {
            keycode[0] = HID_KEY_ENTER;
        } else if (c == '.') { 
            keycode[0] = HID_KEY_PERIOD;
        } else if (c == '-') {
            keycode[0] = HID_KEY_MINUS;
        }

        if (keycode[0] != 0 || modifier != 0) {
            
            // 2. WAIT until the Mac is ready to receive a new report
            while (!tud_hid_ready()) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            
            // Send the Key Press
            tud_hid_keyboard_report(0, modifier, keycode);

            // 3. WAIT until the Mac processes the press before sending the release
            while (!tud_hid_ready()) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            
            // Send the Key Release
            tud_hid_keyboard_report(0, 0, NULL);
            
            // A tiny human-like delay between letters so macOS doesn't drop them
            vTaskDelay(pdMS_TO_TICKS(15)); 
        }
        text++; 
    }
}

// --- The Main Application Task ---

void touch_keyboard_task(void *pvParameters) {
    uint32_t touch_value;
    bool pressed = false;

    while (1) {
        // Read the capacitance value
        touch_pad_read_raw_data(TOUCH_PAD_NUM, &touch_value);
        
        // Print the value so you can calibrate!
        printf("Current Touch Value: %" PRIu32 "\n", touch_value);

        // Flipped logic: We check if it drops BELOW the calibrated threshold
        if (touch_value < TOUCH_THRESHOLD && !pressed) {
            
            printf("Touch Detected! Injecting String...\n");

            // Type your automated payload!
            type_string("Hello from the ESP32-S3.\n");
            
            pressed = true;

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
    tusb_cfg.descriptor.full_speed_config = desc_configuration;

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // 4. Spin up the polling task
    xTaskCreate(touch_keyboard_task, "touch_kb_task", 4096, NULL, 5, NULL);
}