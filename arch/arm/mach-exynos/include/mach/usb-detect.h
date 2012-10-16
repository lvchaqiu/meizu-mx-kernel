#ifndef _MX_USB_DETECT_H_
#define _MX_USB_DETECT_H_

struct usb_detect_platform_data{
	int usb_vbus_gpio;
	int usb_host_gpio;
	int usb_dock_gpio;
};

extern int register_mx_usb_notifier(struct notifier_block *nb);
extern int unregister_mx_usb_notifier(struct notifier_block *nb);
extern int mx_is_usb_host_insert(void);
extern int mx_is_usb_vbus_insert(void);
extern int mx_is_usb_dock_insert(void);

enum usb_insert_status {
	USB_VBUS_INSERT,
	USB_VBUS_REMOVE,
	USB_HOST_INSERT,
	USB_HOST_REMOVE,
	USB_DOCK_INSERT,
	USB_DOCK_REMOVE,
};
#endif
