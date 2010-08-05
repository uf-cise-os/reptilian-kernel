/* 
* Owen Kwon <pinebud77@hotmail.com>
* modified from touchkit_ps for viliv S5 TS
*/

#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/input.h>
#include <linux/serio.h>
#include <linux/libps2.h>

#include "psmouse.h"
#include "ezex_ps2.h"

#define EZEX_MAX_XC         0x03ff
#define EZEX_MAX_YC         0x03ff

/*
int prev_x = -1;
int prev_y = -1;
*/

static psmouse_ret_t ezex_ps2_process_byte(struct psmouse *psmouse)
{
	unsigned char *packet = psmouse->packet;
	struct input_dev *dev = psmouse->dev;
	int x, y;

	if (psmouse->pktcnt != 4)
		return PSMOUSE_GOOD_DATA;

	if (packet[0] == 0x01) {
		//pressed
		/*
		x = ((packet[3]&0x3)<<8) + ((packet[2]&0x0f)<<4) + (packet[1]&0x0f);
		y = (((packet[3]&0xb)>>2)<<8) + (((packet[2]&0xf0)>>4)<<4) 
			+ ((packet[1]&0xf0)>>4);
			*/
		x = ((packet[3]&0x3)<<8) + packet[1];
		y = (((packet[3]>>2)&0x3)<<8) + packet[2];

		x = EZEX_MAX_XC - x;
		y = EZEX_MAX_YC - y;

		input_report_abs(dev, ABS_X, x);
	   	input_report_abs(dev, ABS_Y, y);
	   	input_report_key(dev, BTN_TOUCH, 1);
		input_sync(dev);

		/*	sync part -_-??
		if (prev_x != x || prev_y != y)
			input_sync(dev);

		prev_x = x;
		prev_y = y;
		*/

		/*
		printk(KERN_ERR"pressed %2.2x %2.2x %2.2x %2.2x -> x%d y%d\n",
				packet[0], packet[1], packet[2], packet[3], x, y);
		*/
	} else {
		//printk(KERN_ERR"released\n");
	   	input_report_key(dev, BTN_TOUCH, 0);
	   	input_sync(dev);

		/*
		prev_x = -1;
		prev_y = -1;
		*/
	}

	return PSMOUSE_FULL_PACKET;
}

int ezex_ps2_detect(struct psmouse *psmouse, int set_properties)
{
   	struct input_dev *dev = psmouse->dev;
	unsigned char param[2]; 
	
	param[0] = 200;
	ps2_command(&psmouse->ps2dev, param, PSMOUSE_CMD_SETRATE);
	param[0] = 100;
	ps2_command(&psmouse->ps2dev, param, PSMOUSE_CMD_SETRATE);
	param[0] =  80;
	ps2_command(&psmouse->ps2dev, param, PSMOUSE_CMD_SETRATE);
	ps2_command(&psmouse->ps2dev, param, PSMOUSE_CMD_GETID);

	if (param[0] != 3)
		return -1;


	 if (set_properties) {
		 dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		 set_bit(BTN_TOUCH, dev->keybit);
		 input_set_abs_params(dev, ABS_X, 0, EZEX_MAX_XC, 0, 0);
		 input_set_abs_params(dev, ABS_Y, 0, EZEX_MAX_YC, 0, 0);

		 psmouse->vendor = "Ezex";
		 psmouse->name = "Touchscreen";
		 psmouse->protocol_handler = ezex_ps2_process_byte;
		 psmouse->pktsize = 4;
	 }

	return 0;
}

