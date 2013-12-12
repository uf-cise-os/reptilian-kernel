#ifndef __I2C_HW_CONTROL_H
#define __I2C_HW_COMTROL_H
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

#include <linux/i2c.h>
#include <linux/i2c/i2c-hid.h>

int i2c_sensor_hw_init(struct i2c_client *client);
int i2c_sensor_hw_reset(struct i2c_client *client);
int i2c_sensor_hw_suspend(struct i2c_client *client);
int i2c_sensor_hw_resume(struct i2c_client *client);

#endif
