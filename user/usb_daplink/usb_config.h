#ifndef DAP_USB_CONFIG_H
#define DAP_USB_CONFIG_H
#include "ch32fun.h"
#include "usb_identity.h"
#include "usb_defines.h"
#define FUSB_BUFFERS_NUMBER 3
#define FUSB_EP1_MODE USBFS_EP_MODE_TX
#define FUSB_EP2_MODE USBFS_EP_MODE_RX
#define FUSB_SUPPORTS_SLEEP 0
#define FUSB_HID_INTERFACES 0
#define FUSB_HID_USER_REPORTS 0
#define FUSB_CURSED_TURBO_DMA 0
#define FUSB_IO_PROFILE 0
#define FUSB_USE_HPE 0
#define FUSB_USER_HANDLERS 1
#define FUSB_USE_DMA7_COPY 0
#define FUSB_VDD_5V FUNCONF_USE_5V_VDD
#define FUSB_FROM_RAM 1
void dap_usb_reset(void);
#define FUSB_RESET_CALLBACK dap_usb_reset
/* Development identity, following the sibling bridge. Override for production. */
#ifndef FUSB_USB_VID
#define FUSB_USB_VID 0x1A86
#endif
#ifndef FUSB_USB_PID
#define FUSB_USB_PID 0x0003
#endif
#define DAP_MS_VENDOR_CODE 0x20
static const uint8_t device_descriptor[] = {
    18,1,0,2,0,0,0,64, FUSB_USB_VID & 255,FUSB_USB_VID >> 8,
    FUSB_USB_PID & 255,FUSB_USB_PID >> 8, 0,1,1,2,3,1
};
static const uint8_t config_descriptor[] = {
    9,2,32,0,1,1,0,0x80,50,
    9,4,0,0,2,0xff,0,0,2,
    /* CMSIS-DAP order: Bulk OUT first, Bulk IN second. */
    7,5,0x02,2,64,0,0,
    7,5,0x81,2,64,0,0
};
static const uint8_t language[] = {4,3,9,4};
static const struct { uint8_t len,type; uint16_t text[7]; } manufacturer = {16,3,u"ch32fun"};
static const struct { uint8_t len,type; uint16_t text[18]; } product = {38,3,u"CH570 CMSIS-DAP v2"};
static const uint8_t ms_string[] = {18,3,'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,DAP_MS_VENDOR_CODE,0};
static const struct descriptor_list_struct {
    uint32_t lIndexValue;
    const uint8_t *addr;
    uint8_t length;
} descriptor_list[] = {
    {0x100,device_descriptor,sizeof(device_descriptor)},
    {0x200,config_descriptor,sizeof(config_descriptor)},
    {0x300,language,sizeof(language)},
    {0x04090301,(const uint8_t *)&manufacturer,sizeof(manufacturer)},
    {0x04090302,(const uint8_t *)&product,sizeof(product)},
    {0x04090303,(const uint8_t *)&usb_serial,sizeof(usb_serial)},
    {0x03ee,ms_string,sizeof(ms_string)},
    {0x040903ee,ms_string,sizeof(ms_string)}
};
#define DESCRIPTOR_LIST_ENTRIES (sizeof(descriptor_list)/sizeof(descriptor_list[0]))
#endif
