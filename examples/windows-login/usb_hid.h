#ifndef SAFETOUCH_USB_HID_H
#define SAFETOUCH_USB_HID_H

#include <stdint.h>

void usb_hid_init(void);
void usb_hid_poll(void);
int usb_hid_take_request(uint8_t report[64]);
int usb_hid_send_reply(const uint8_t report[64]);

#endif
