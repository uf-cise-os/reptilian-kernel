/*
 *  HID driver for PenMount touchscreens
 *
 *  Copyright (c) 2011 PenMount Touch Solutions <penmount@seed.net.tw>
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/module.h>
#include <linux/hid.h>
#include <linux/version.h>
#include <linux/input/mt.h>
#include "hid-ids.h"

#define PENMOUNT_MAXTOUCH   2
#define PENMOUNT_MTPROTO_A  0
#define PENMOUNT_MTPROTO_B  1
#define PENMOUNT_ANDROID    0
#define PENMOUNT_MAXTRACKID 0xFFFF

#ifndef ABS_MT_SLOT
#define PENMOUNT_MTPROTO   PENMOUNT_MTPROTO_A
#else
#define PENMOUNT_MTPROTO   PENMOUNT_MTPROTO_B
#endif

#ifndef USB_DEVICE_ID_PENMOUNT_MF
#define USB_DEVICE_ID_PENMOUNT_MF       0x6250
#endif

typedef struct tagPMTOUCH {
	__u8  bUpdated    ;
	__s32 TrackID     ;
	__u8  Slot        ;
	__u8  bTouch      ;
	__u8  bTouching   ;
	__u16 X           ;
	__u16 Y           ;
	__u16 LastX       ;
	__u16 LastY       ;
	__u8  LastState   ;
} strPMTOUCH ;

typedef struct tagPenMount {
	__u8        MaxTouch     ;
	__u8        MTProtocol   ;
	__s32       TrackIDCount ;
	strPMTOUCH  Touch[PENMOUNT_MAXTOUCH] ;
	strPMTOUCH *pLastTouch   ;
	strPMTOUCH *pMainTouch   ;

} strPenMount ;

static int PenMountHID_input_mapping ( struct hid_device *pHidDevice ,
		struct hid_input  *pHidInput  ,
		struct hid_field  *pHidField  ,
		struct hid_usage  *pHidUsage  ,
		unsigned long    **bit        ,
		int               *max        )
{
	// return > 0 : mapped
	// return < 0 : ignore

	struct input_dev   *pInputDev = pHidInput->input ;
	strPenMount *pPenMount = (strPenMount *)hid_get_drvdata ( pHidDevice ) ;

	if ( !pInputDev )
		return 0 ;

	switch ( pHidUsage->hid & HID_USAGE_PAGE )
	{
		case HID_UP_GENDESK :
			switch ( pHidUsage->hid )
			{
				case HID_GD_X :
					if ( pPenMount->MaxTouch > 1 ) {
						hid_map_usage ( pHidInput, pHidUsage, bit, max, EV_ABS, ABS_MT_POSITION_X ) ;
						input_set_abs_params ( pInputDev, ABS_X, pHidField->logical_minimum, pHidField->logical_maximum, 0, 0 ) ;
					}
					else
						hid_map_usage ( pHidInput, pHidUsage, bit, max, EV_ABS, ABS_X ) ;

					return 1 ;
				case HID_GD_Y :
					if ( pPenMount->MaxTouch > 1 ) {
						hid_map_usage ( pHidInput, pHidUsage, bit, max, EV_ABS, ABS_MT_POSITION_Y ) ;
						input_set_abs_params ( pInputDev, ABS_Y, pHidField->logical_minimum, pHidField->logical_maximum, 0, 0 ) ;
					}
					else
						hid_map_usage ( pHidInput, pHidUsage, bit, max, EV_ABS, ABS_Y ) ;

					return 1 ;
			}
			break ;
		case HID_UP_BUTTON :
			hid_map_usage ( pHidInput, pHidUsage, bit, max, EV_KEY, BTN_TOUCH ) ;
			return 1 ;
			break ;
		case HID_UP_DIGITIZER :
			switch ( pHidUsage->hid )
			{
				case HID_DG_TIPSWITCH    :
					hid_map_usage ( pHidInput, pHidUsage, bit, max, EV_KEY, BTN_TOUCH ) ;
					return 1 ;
				case HID_DG_CONTACTID    :
					switch ( pPenMount->MTProtocol )
					{
						case PENMOUNT_MTPROTO_A:
							input_set_abs_params ( pInputDev, ABS_MT_TOUCH_MAJOR, 0, 5, 0, 0 ) ;
							break ;
						case PENMOUNT_MTPROTO_B:
							input_mt_init_slots  ( pInputDev, pPenMount->MaxTouch ) ;
							break ;
					}
					return -1 ;
				case HID_DG_INRANGE      :
				case HID_DG_CONFIDENCE   :
					return -1 ;
			}
			break ;
	}

	return 0 ;
}

static void PenMount_ProcessEvent ( struct input_dev   *pInputDev ,
		strPenMount *pPenMount ,
		strPMTOUCH  *pTouch    )
{
	if ( pTouch->bTouch ) {
		if ( !pTouch->bTouching ) {
			if ( ( pPenMount->MaxTouch == 1 ) && ( pTouch->Slot == 0 ) )
				input_report_key ( pInputDev, BTN_TOUCH , 1 ) ;
			pTouch->bTouching = 1 ;
		}
	} else {
		if ( pTouch->bTouching ) {
			if ( ( pPenMount->MaxTouch == 1 ) && ( pTouch->Slot == 0 ) )
				input_report_key ( pInputDev, BTN_TOUCH , 0 ) ;
			pTouch->bTouching = 0 ;
		}
	}

	if ( ( pPenMount->MaxTouch == 1 ) && ( pTouch->Slot == 0 ) ) {
		input_report_abs ( pInputDev, ABS_X  , pTouch->X ) ;
		input_report_abs ( pInputDev, ABS_Y  , pTouch->Y ) ;
		input_sync       ( pInputDev ) ;
	}

	pTouch->bTouch = 0 ;

	return ;
}

static void PenMount_ProcessMTEvent ( struct input_dev *pInputDev ,
		strPenMount *pPenMount )
{
	__u8 i = 0 ;

	if ( pPenMount->MaxTouch > 1 ) {
		__u8         TouchCount = 0 ;

		switch ( pPenMount->MTProtocol )
		{
			case PENMOUNT_MTPROTO_A :
				for ( i = 0 ; i < pPenMount->MaxTouch ; i++ ) {
					if ( ( pPenMount->pMainTouch == NULL ) && ( pPenMount->Touch[i].bTouching ) )
						pPenMount->pMainTouch = &pPenMount->Touch[i] ;

					if ( pPenMount->Touch[i].bTouching ) {
						input_report_abs ( pInputDev, ABS_MT_TOUCH_MAJOR , 2 ) ;
						input_report_abs ( pInputDev, ABS_MT_POSITION_X  , pPenMount->Touch[i].X ) ;
						input_report_abs ( pInputDev, ABS_MT_POSITION_Y  , pPenMount->Touch[i].Y ) ;
						input_mt_sync    ( pInputDev ) ;
						TouchCount++ ;
					}
				}
				if ( !TouchCount )
					input_mt_sync ( pInputDev ) ;

				break ;
			case PENMOUNT_MTPROTO_B :
#ifdef ABS_MT_SLOT
				for ( i = 0 ; i < pPenMount->MaxTouch ; i++ ) {
					if ( ( pPenMount->pMainTouch == NULL ) && ( pPenMount->Touch[i].bTouching ) )
						pPenMount->pMainTouch = &pPenMount->Touch[i] ;

					if ( ( pPenMount->Touch[i].X != pPenMount->Touch[i].LastX )
							|| ( pPenMount->Touch[i].Y != pPenMount->Touch[i].LastY )
							|| ( pPenMount->Touch[i].bTouching != pPenMount->Touch[i].LastState ) ) {
						input_mt_slot ( pInputDev, i ) ;
						input_mt_report_slot_state ( pInputDev, MT_TOOL_FINGER, pPenMount->Touch[i].bTouching ) ;
						if ( pPenMount->Touch[i].bTouching ) {
							if ( pPenMount->Touch[i].TrackID < 0 )
								pPenMount->Touch[i].TrackID = ( pPenMount->TrackIDCount++ & 0xFFFF ) ;
							input_report_abs ( pInputDev, ABS_MT_TRACKING_ID , pPenMount->Touch[i].TrackID ) ;
							if ( pPenMount->Touch[i].X != pPenMount->Touch[i].LastX ) {
								input_report_abs ( pInputDev, ABS_MT_POSITION_X  , pPenMount->Touch[i].X ) ;
								pPenMount->Touch[i].LastX = pPenMount->Touch[i].X ;
							}
							if ( pPenMount->Touch[i].Y != pPenMount->Touch[i].LastY ) {
								input_report_abs ( pInputDev, ABS_MT_POSITION_Y  , pPenMount->Touch[i].Y ) ;
								pPenMount->Touch[i].LastY = pPenMount->Touch[i].Y ;
							}
							if ( pPenMount->Touch[i].bTouching != pPenMount->Touch[i].LastState )
								pPenMount->Touch[i].LastState = pPenMount->Touch[i].bTouching ;
						} else if ( pPenMount->Touch[i].TrackID != -1 ) {
							pPenMount->Touch[i].TrackID = -1 ;
							input_report_abs ( pInputDev, ABS_MT_TRACKING_ID , pPenMount->Touch[i].TrackID ) ;
						}
					}
				}
#endif
				break ;
		}
		// Single-Touch Emulation
		input_mt_report_pointer_emulation ( pInputDev, true ) ;
		input_sync ( pInputDev ) ;
	}

	for ( i = 0 ; i < pPenMount->MaxTouch ; i++ )
		pPenMount->Touch[i].bUpdated = 0 ;

	return ;
}

static int PenMountHID_event ( struct hid_device *pHidDevice ,
		struct hid_field  *pHidField  ,
		struct hid_usage  *pHidUsage  ,
		__s32              value      )
{
	__u8 i              = 0 ;
	__u8 bProcessEvents = 1 ;
	if ( !pHidDevice )
		return 0 ;

	if ( pHidDevice->claimed & HID_CLAIMED_INPUT ) {
		strPenMount *pPenMount = hid_get_drvdata ( pHidDevice ) ;
		struct input_dev   *pInputDev = pHidField->hidinput->input ;

		if ( !pInputDev )
			return 0 ;

		if ( !pPenMount->pLastTouch )
			pPenMount->pLastTouch = &(pPenMount->Touch[0]) ;

		switch ( pHidUsage->hid )
		{
			case HID_DG_CONTACTID :
				if ( value < pPenMount->MaxTouch )
					pPenMount->pLastTouch = &( pPenMount->Touch[value] ) ;
				if ( pPenMount->pLastTouch->bUpdated )
					PenMount_ProcessMTEvent ( pInputDev, pPenMount ) ;
				pPenMount->pLastTouch->bUpdated = 1 ;
				break ;
			case HID_DG_TIPSWITCH :
				pPenMount->pLastTouch->bTouch = value ;
				break ;
			case HID_GD_X         :
				pPenMount->pLastTouch->X  = value ;
				break ;
			case HID_GD_Y         :
				pPenMount->pLastTouch->Y  = value ;
				PenMount_ProcessEvent ( pInputDev, pPenMount, pPenMount->pLastTouch ) ;
				for ( i = 0 ; i < pPenMount->MaxTouch ; i++ )
					if ( ( !pPenMount->Touch[i].bUpdated )
							&& ( pPenMount->Touch[i].bTouching ) )
						bProcessEvents = 0 ;
				if ( bProcessEvents )
					PenMount_ProcessMTEvent ( pInputDev, pPenMount ) ;
				break ;
			default :
				/* fallback to the generic hidinput handling */
				return 0 ;
		}
	}

	if ( ( pHidDevice->claimed & HID_CLAIMED_HIDDEV )
			&& ( pHidDevice->hiddev_hid_event ) )
		pHidDevice->hiddev_hid_event ( pHidDevice, pHidField, pHidUsage, value ) ;

	return 1 ;
}

static int PenMountHID_probe ( struct hid_device *pHidDevice ,
		const struct hid_device_id *pHidDevID  )
{
	int               ret       = 0 ;
	int               i         = 0 ;
	strPenMount      *pPenMount = NULL ;

	pPenMount = kmalloc ( sizeof(strPenMount), GFP_KERNEL ) ;
	if ( !pPenMount )
		return -ENOMEM ;

	memset ( pPenMount , 0 , sizeof(strPenMount) ) ;

	for ( i = 0 ; i < PENMOUNT_MAXTOUCH ; i++ ) {
		pPenMount->Touch[i].Slot    = i ;
		pPenMount->Touch[i].TrackID = -1 ;
	}
	switch ( pHidDevID->product )
	{
		case USB_DEVICE_ID_PENMOUNT_PCI:
			pPenMount->MaxTouch = 2 ;
			pPenMount->MTProtocol = PENMOUNT_MTPROTO ;
			break ;
		default:
			pPenMount->MaxTouch = 1 ;
			break ;
	}

	pPenMount->pLastTouch = &(pPenMount->Touch[0]) ;

	hid_set_drvdata ( pHidDevice , pPenMount ) ;

	ret = hid_parse ( pHidDevice ) ;
	if ( ret ) {
		kfree ( pPenMount ) ;
		return ret ;
	}

	ret = hid_hw_start ( pHidDevice, HID_CONNECT_DEFAULT ) ;
	if ( ret ) {
		kfree ( pPenMount ) ;
		return ret ;
	}

	return ret ;
}

static void PenMountHID_remove ( struct hid_device *pHidDevice )
{
	if ( !pHidDevice )
		return ;

	hid_hw_stop ( pHidDevice ) ;
	kfree ( hid_get_drvdata(pHidDevice) ) ;
	hid_set_drvdata ( pHidDevice , NULL ) ;

	return ;
}

static const struct hid_device_id PENMOUNTHID_DEVICEID [] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_PENMOUNT, USB_DEVICE_ID_PENMOUNT_5000) } ,
	{ HID_USB_DEVICE(USB_VENDOR_ID_PENMOUNT, USB_DEVICE_ID_PENMOUNT_6000) } ,
	{ HID_USB_DEVICE(USB_VENDOR_ID_PENMOUNT, USB_DEVICE_ID_PENMOUNT_MF)   } ,
	{ HID_USB_DEVICE(USB_VENDOR_ID_PENMOUNT, USB_DEVICE_ID_PENMOUNT_PCI)  } ,
	{ }
};

static const struct hid_usage_id PENMOUNTHID_USAGETABLE[] = {
	{ HID_ANY_ID    , HID_ANY_ID    , HID_ANY_ID     } ,
	{ HID_ANY_ID - 1, HID_ANY_ID - 1, HID_ANY_ID - 1 }
};

static struct hid_driver PENMOUNTHID_DRIVER = {
	.name          = "Hid-PenMount"            ,
	.id_table      = PENMOUNTHID_DEVICEID      ,
	.probe         = PenMountHID_probe         ,
	.remove        = PenMountHID_remove        ,
	.input_mapping = PenMountHID_input_mapping ,
	.usage_table   = PENMOUNTHID_USAGETABLE    ,
	.event         = PenMountHID_event         ,
};

static int __init PenMountHID_init ( void )
{
	return hid_register_driver ( &PENMOUNTHID_DRIVER ) ;
}

static void __exit PenMountHID_exit ( void )
{
	hid_unregister_driver ( &PENMOUNTHID_DRIVER ) ;
	return ;
}

module_init(PenMountHID_init);
module_exit(PenMountHID_exit);

MODULE_AUTHOR("PenMount Touch Solutions <penmount@seed.net.tw>");
MODULE_DESCRIPTION("PenMount HID TouchScreen Driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(hid, PENMOUNTHID_DEVICEID);
