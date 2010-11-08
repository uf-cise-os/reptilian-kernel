/*
 *  HID driver for the Pixcir multitouch controller
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
MODULE_DESCRIPTION("Pixcir dual-touch controller");
MODULE_LICENSE("GPL");

#include "hid-ids.h"

struct pixcir_data {
	__u16 x, y;
	__u8 id;
	bool valid;		/* valid finger data, or just placeholder? */
	bool first;		/* is this the first finger in this frame? */
};

static int pixcir_input_mapping(struct hid_device *hdev, struct hid_input *hi,
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
			hid_map_usage(hi, usage, bit, max, EV_KEY, BTN_TOUCH);
			return 1;

		/* these ones are not reliable */
		case HID_DG_INRANGE:
		case HID_DG_CONFIDENCE:
		case HID_DG_CONTACTCOUNT:
			return -1;

		case HID_DG_CONTACTID:
			hid_map_usage(hi, usage, bit, max,
					EV_ABS, ABS_MT_TRACKING_ID);
			return 1;

		/* ignore this feature for now */
		case HID_DG_CONTACTMAX:
			return -1;
		}
		return 0;
	}

	return 0;
}

static int pixcir_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	if (usage->type == EV_KEY || usage->type == EV_ABS)
		clear_bit(usage->code, *bit);

	return 0;
}

/*
 * this function is called when a whole finger has been parsed,
 * so that it can decide what to send to the input layer.
 */
static void pixcir_filter_event(struct pixcir_data *td, struct input_dev *input)
{

	td->first = !td->first; /* touchscreen emulation */

	if (!td->valid) {
		/*
		 * touchscreen emulation: if no finger in this frame is valid
		 * this is a release
		 */
		if (td->first)
			input_event(input, EV_KEY, BTN_TOUCH, 0);
		return;
	}

	input_event(input, EV_ABS, ABS_MT_TRACKING_ID, td->id);
	input_event(input, EV_ABS, ABS_MT_POSITION_X, td->x);
	input_event(input, EV_ABS, ABS_MT_POSITION_Y, td->y);

	input_mt_sync(input);
	td->valid = false;

	/* touchscreen emulation: if first finger in this frame... */
	if (td->first) {
		/* this is our preferred finger */
		input_event(input, EV_KEY, BTN_TOUCH, 1);
		input_event(input, EV_ABS, ABS_X, td->x);
		input_event(input, EV_ABS, ABS_Y, td->y);
	}
}

static int pixcir_event(struct hid_device *hid, struct hid_field *field,
				struct hid_usage *usage, __s32 value)
{
	struct pixcir_data *td = hid_get_drvdata(hid);

	if (hid->claimed & HID_CLAIMED_INPUT) {
		struct input_dev *input = field->hidinput->input;
		switch (usage->hid) {
		case HID_DG_TIPSWITCH:
			td->valid = value;
			break;
		case HID_DG_INRANGE:
		case HID_DG_CONFIDENCE:
		case HID_DG_CONTACTCOUNT:
			/* avoid interference from generic hidinput handling */
			break;
		case HID_DG_CONTACTID:
			td->id = value;
			break;
		case HID_GD_X:
			td->x = value;
			break;
		case HID_GD_Y:
			td->y = value;
			pixcir_filter_event(td, input);
			break;
		case HID_DG_CONTACTMAX:
			break;

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

static int pixcir_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct pixcir_data *td;


	td = kzalloc(sizeof(struct pixcir_data), GFP_KERNEL);
	if (!td) {
		dev_err(&hdev->dev, "cannot allocate Pixcir data\n");
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

static void pixcir_remove(struct hid_device *hdev)
{
	hid_hw_stop(hdev);
	kfree(hid_get_drvdata(hdev));
	hid_set_drvdata(hdev, NULL);
}

static const struct hid_device_id pixcir_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_HANVON, USB_DEVICE_ID_HANVON_MULTITOUCH) },
	{ }
};
MODULE_DEVICE_TABLE(hid, pixcir_devices);

static const struct hid_usage_id pixcir_grabbed_usages[] = {
	{ HID_ANY_ID, HID_ANY_ID, HID_ANY_ID },
	{ HID_ANY_ID - 1, HID_ANY_ID - 1, HID_ANY_ID - 1}
};

static struct hid_driver pixcir_driver = {
	.name = "pixcir",
	.id_table = pixcir_devices,
	.probe = pixcir_probe,
	.remove = pixcir_remove,
	.input_mapping = pixcir_input_mapping,
	.input_mapped = pixcir_input_mapped,
	.usage_table = pixcir_grabbed_usages,
	.event = pixcir_event,
};

static int __init pixcir_init(void)
{
	return hid_register_driver(&pixcir_driver);
}

static void __exit pixcir_exit(void)
{
	hid_unregister_driver(&pixcir_driver);
}

module_init(pixcir_init);
module_exit(pixcir_exit);
