/*
 *  HID driver for the multitouch panel on the Hanvon tablet
 *
 *  Copyright (c) 2010 Stephane Chatty <chatty@enac.fr>
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include "usbhid/usbhid.h"

MODULE_AUTHOR("Stephane Chatty <chatty@enac.fr>");
MODULE_DESCRIPTION("Hanvon dual-touch panel");
MODULE_LICENSE("GPL");

#include "hid-ids.h"

struct hanvon_data {
	__u16 x, y;
	__u8 id;
};

static int hanvon_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	switch (usage->hid & HID_USAGE_PAGE) {

	case HID_UP_GENDESK:
		switch (usage->hid) {
		case HID_GD_X:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_POSITION_X);
			/* touchscreen emulation */
			input_set_abs_params(hi->input, ABS_X,
						field->logical_minimum,
						field->logical_maximum, 0, 0);
			return 1;
		case HID_GD_Y:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_POSITION_Y);
			/* touchscreen emulation */
			input_set_abs_params(hi->input, ABS_Y,
						field->logical_minimum,
						field->logical_maximum, 0, 0);
			return 1;
		}
		return 0;

	case HID_UP_DIGITIZER:
		switch (usage->hid) {
		case HID_DG_TIPSWITCH:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
						ABS_MT_TOUCH_MAJOR);
			return 1;

		case HID_DG_INRANGE:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
						ABS_MT_TOUCH_MINOR);
			return 1;

		case HID_DG_CONFIDENCE:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
						ABS_MT_WIDTH_MAJOR);
			return 1;

		case HID_DG_CONTACTID:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TRACKING_ID);
			return 1;

                case HID_DG_CONTACTCOUNT:
                        hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_TOUCH_MAJOR);
                        input_set_abs_params(hi->input, ABS_MT_TOUCH_MAJOR, 0, 255, 4, 0);
                        hid_map_usage(hi, usage, bit, max, EV_ABS, ABS_MT_TOUCH_MINOR);
                        input_set_abs_params(hi->input, ABS_MT_TOUCH_MINOR, 0, 255, 4, 0);
                        return 1;

		case HID_DG_CONTACTMAX:
			hid_map_usage(hi, usage, bit, max, EV_ABS,
						ABS_MT_ORIENTATION);
			return 1;
		}
		return 0;
	}

	return 0;
}

static int hanvon_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	if (usage->type == EV_KEY || usage->type == EV_ABS)
		clear_bit(usage->code, *bit);

	return 0;
}


static int hanvon_event(struct hid_device *hid, struct hid_field *field,
				struct hid_usage *usage, __s32 value)
{
//	struct hanvon_data *td = hid_get_drvdata(hid);

	if (hid->claimed & HID_CLAIMED_INPUT) {
		struct input_dev *input = field->hidinput->input;
		switch (usage->hid) {
		case HID_DG_TIPSWITCH:
			input_event(input, EV_ABS, ABS_MT_TOUCH_MAJOR, value);
			break;
		case HID_DG_INRANGE:
			input_event(input, EV_ABS, ABS_MT_TOUCH_MINOR, value);
			break;
		case HID_DG_CONFIDENCE:
			input_event(input, EV_ABS, ABS_MT_WIDTH_MAJOR, value);
			break;
		case HID_GD_X:
			input_event(input, EV_ABS, ABS_MT_POSITION_X, value);
			break;
		case HID_GD_Y:
			input_event(input, EV_ABS, ABS_MT_POSITION_Y, value);
			input_mt_sync(input);
			break;
		case HID_DG_CONTACTID:
			input_event(input, EV_ABS, ABS_MT_TRACKING_ID, value);
			break;
		case HID_DG_CONTACTCOUNT:
			input_event(input, EV_ABS, ABS_MT_WIDTH_MINOR, value);
			break;
		case HID_DG_CONTACTMAX:
			break;
			input_event(input, EV_ABS, ABS_MT_ORIENTATION, value);

		default:
			/* fallback to the generic hidinput handling */
			return 0;
		}
	}

	/* we have handled the hidinput part, now remains hiddev */
	if (hid->claimed & HID_CLAIMED_HIDDEV && hid->hiddev_hid_event)
		hid->hiddev_hid_event(hid, field, usage, value);

	return 1;
}

static int hanvon_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct hanvon_data *td;


	td = kmalloc(sizeof(struct hanvon_data), GFP_KERNEL);
	if (!td) {
		dev_err(&hdev->dev, "cannot allocate Hanvon data\n");
		return -ENOMEM;
	}
	hid_set_drvdata(hdev, td);

	ret = hid_parse(hdev);
	if (ret == 0)
		ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);

	if (ret)
		kfree(td);

	return ret;
}

static void hanvon_remove(struct hid_device *hdev)
{
	hid_hw_stop(hdev);
	kfree(hid_get_drvdata(hdev));
	hid_set_drvdata(hdev, NULL);
}

static const struct hid_device_id hanvon_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_HANVON, USB_DEVICE_ID_HANVON_MULTITOUCH) },
	{ }
};
MODULE_DEVICE_TABLE(hid, hanvon_devices);

static const struct hid_usage_id hanvon_grabbed_usages[] = {
	{ HID_ANY_ID, HID_ANY_ID, HID_ANY_ID },
	{ HID_ANY_ID - 1, HID_ANY_ID - 1, HID_ANY_ID - 1}
};

static struct hid_driver hanvon_driver = {
	.name = "hanvon",
	.id_table = hanvon_devices,
	.probe = hanvon_probe,
	.remove = hanvon_remove,
	.input_mapping = hanvon_input_mapping,
	.input_mapped = hanvon_input_mapped,
	.usage_table = hanvon_grabbed_usages,
	.event = hanvon_event,
};

static int __init hanvon_init(void)
{
	return hid_register_driver(&hanvon_driver);
}

static void __exit hanvon_exit(void)
{
	hid_unregister_driver(&hanvon_driver);
}

module_init(hanvon_init);
module_exit(hanvon_exit);

