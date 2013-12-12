/*
 * I2C_hw_control
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/acpi_gpio.h>
#include <linux/i2c/i2c-hid.h>
#include "i2c-hw-control.h"

#define I2C_SENSOR_BUS_NUM 5

static int gpio_psh_ctl;
static int gpio_psh_rst;

static struct i2c_hid_hw_data hw_data = {i2c_sensor_hw_init,
					i2c_sensor_hw_reset,
					i2c_sensor_hw_suspend,
					i2c_sensor_hw_resume};

/*
Function: i2c_set_hwdata
This function set the function pointers for init,reset,suspend and resume.
Currently used for byt-m sensor hub. This function can be used to
set the platform specific functions
*/

int i2c_set_hwdata(struct i2c_client *client, struct i2c_hid_hw_data *ihid)
{
	/* On byt-m board, sensor hub is on bus number 5*/
	if (client->adapter->nr == I2C_SENSOR_BUS_NUM) {
		ihid->hw_init  = i2c_sensor_hw_init;
		ihid->hw_reset  = i2c_sensor_hw_reset;
		ihid->hw_suspend  = i2c_sensor_hw_suspend;
		ihid->hw_resume  = i2c_sensor_hw_resume;
	}
	return 0;

}


int i2c_sensor_hw_init(struct i2c_client *client)
{
	gpio_psh_ctl = acpi_get_gpio_by_index(&client->dev, 1, NULL);
	if (gpio_psh_ctl < 0) {
		dev_err(&client->dev, "fail to get psh_ctl pin by ACPI\n");
	} else {
		int rc = gpio_request(gpio_psh_ctl, "psh_ctl");
		if (rc) {
			dev_err(&client->dev, "fail to request psh_ctl pin\n");
			gpio_psh_ctl = -1;
		} else {
			gpio_export(gpio_psh_ctl, 1);
			gpio_direction_output(gpio_psh_ctl, 1);
			gpio_set_value(gpio_psh_ctl, 1);
		}
	}

	gpio_psh_rst = acpi_get_gpio_by_index(&client->dev, 0, NULL);
	if (gpio_psh_rst < 0) {
		dev_err(&client->dev, "failed to get psh_rst pin by ACPI\n");
	} else {
		int rc = gpio_request(gpio_psh_rst, "psh_rst");
		if (rc) {
			dev_err(&client->dev, "fail to request psh_rst pin\n");
			gpio_psh_rst = -1;
		} else {
			gpio_export(gpio_psh_rst, 1);
			gpio_direction_output(gpio_psh_rst, 1);
			gpio_set_value(gpio_psh_rst, 1);
		}
	}
	return 0;
}


int i2c_sensor_hw_suspend(struct i2c_client *client)
{
	if (gpio_psh_ctl > 0)
		gpio_set_value(gpio_psh_ctl, 0);
	dev_dbg(&client->dev, "I2C HID suspend: %s\n", __func__);
	return 0;

}


int i2c_sensor_hw_resume(struct i2c_client *client)
{
	if (gpio_psh_ctl > 0) {
		gpio_set_value(gpio_psh_ctl, 1);
		usleep_range(800, 1000);
	}
	dev_dbg(&client->dev, "I2C HID resume: %s\n", __func__);
	return 0;
}

int i2c_sensor_hw_reset(struct i2c_client *client)
{
	gpio_set_value(gpio_psh_rst, 0);
	usleep_range(10000, 12000);
	gpio_set_value(gpio_psh_rst, 1);
	msleep(1000);
	dev_dbg(&client->dev, "I2C HID hardware reset : %s\n", __func__);
	return 0;
}

MODULE_AUTHOR("Sharada<sharadaX.palasamudram.ashok.kumar@intel.com>");
MODULE_LICENSE("GPL");
