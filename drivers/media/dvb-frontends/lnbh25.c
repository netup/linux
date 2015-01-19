/*
 * lnbh25.c
 *
 * Driver for LNB supply and control IC LNBH25
 *
 * Copyright (C) 2014 NetUP Inc.
 * Copyright (C) 2014 Sergey Kozlov <serjk@netup.ru>
 * Copyright (C) 2014 Abylay Ospan <aospan@netup.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "dvb_frontend.h"
#include "lnbh25.h"

static int debug;

#define dprintk(args...) \
	do { \
		if (debug) \
			dev_dbg(&priv->i2c->dev, args); \
	} while (0)

struct lnbh25_priv {
	struct i2c_adapter	*i2c;
	u8			i2c_address;
	/*
	 * LNBH25 configuration:
	 * offset 0: first LNBH25 register address: always 0x02 (DATA1)
	 * offset 1: DATA1 register value
	 * offset 2: DATA2 register value
	 */
	u8			config[3];
};

#define LNBH25_STATUS_OFL	0x1
#define LNBH25_STATUS_VMON	0x4
#define LNBH25_VSEL_13		0x03
#define LNBH25_VSEL_18		0x0a

static void lnbh25_read_vmon(struct lnbh25_priv *priv)
{
	int i;
	u8 addr = 0x00;
	u8 status[6];
	struct i2c_msg msg[2] = {
		{
			.addr = priv->i2c_address,
			.flags = 0,
			.len = 1,
			.buf = &addr
		}, {
			.addr = priv->i2c_address,
			.flags = I2C_M_RD,
			.len = sizeof(status),
			.buf = status
		}
	};

	msleep(100);
	for (i = 0; i < 2; i++) {
		if (i2c_transfer(priv->i2c, &msg[i], 1) != 1) {
			dprintk("%s(): I2C transfer %d failed\n",
				__func__, i);
			return;
		}
	}
	for (i = 0; i < sizeof(status); i++)
		dprintk("%s(): reg %d value 0x%x\n",
			__func__, i, (int)status[i]);
	if ((status[0] & (LNBH25_STATUS_OFL | LNBH25_STATUS_VMON)) != 0)
		dev_err(&priv->i2c->dev,
			"%s(): voltage in failure state, status reg 0x%x\n",
			__func__, status[0]);
}

static int lnbh25_set_voltage(struct dvb_frontend *fe,
		fe_sec_voltage_t voltage)
{
	u8 data1_reg;
	const char *vsel;
	struct lnbh25_priv *priv = fe->sec_priv;
	struct i2c_msg msg = {
		.addr = priv->i2c_address,
		.flags = 0,
		.len = sizeof(priv->config),
		.buf = priv->config
	};

	switch (voltage) {
	case SEC_VOLTAGE_OFF:
		data1_reg = 0x00;
		vsel = "Off";
		break;
	case SEC_VOLTAGE_13:
		data1_reg = LNBH25_VSEL_13;
		vsel = "13V";
		break;
	case SEC_VOLTAGE_18:
		data1_reg = LNBH25_VSEL_18;
		vsel = "18V";
		break;
	default:
		return -EINVAL;
	}
	priv->config[1] = data1_reg;
	dprintk("%s(): %s, I2C 0x%x write [ %02x %02x %02x ]\n",
		__func__, vsel, priv->i2c_address,
		priv->config[0], priv->config[1], priv->config[2]);
	if (i2c_transfer(priv->i2c, &msg, 1) != 1) {
		dev_err(&priv->i2c->dev,
			"%s(): I2C transfer error\n", __func__);
		return -EIO;
	}
	msleep(20);
	if (voltage != SEC_VOLTAGE_OFF)
		lnbh25_read_vmon(priv);
	return 0;
}

static void lnbh25_release(struct dvb_frontend *fe)
{
	struct lnbh25_priv *priv = fe->sec_priv;

	dprintk("%s()\n", __func__);
	lnbh25_set_voltage(fe, SEC_VOLTAGE_OFF);
	kfree(fe->sec_priv);
	fe->sec_priv = NULL;
}

struct dvb_frontend *lnbh25_attach(
	struct dvb_frontend *fe,
	struct lnbh25_config *cfg,
	struct i2c_adapter *i2c)
{
	struct lnbh25_priv *priv;

	dprintk("%s()\n", __func__);
	priv = kzalloc(sizeof(struct lnbh25_priv), GFP_KERNEL);
	if (!priv)
		return NULL;
	priv->i2c_address = (cfg->i2c_address >> 1);
	priv->i2c = i2c;
	priv->config[0] = 0x02;
	priv->config[1] = 0x00;
	priv->config[2] = cfg->data2_config;
	fe->sec_priv = priv;
	if (lnbh25_set_voltage(fe, SEC_VOLTAGE_OFF)) {
		dev_err(&i2c->dev, "%s(): no LNBH25 found at I2C addr 0x%02x\n",
			__func__, priv->i2c_address);
		kfree(priv);
		fe->sec_priv = NULL;
		return NULL;
	}

	fe->ops.release_sec = lnbh25_release;
	fe->ops.set_voltage = lnbh25_set_voltage;

	dev_err(&i2c->dev, "%s(): attached at I2C addr 0x%02x\n",
		__func__, priv->i2c_address);
	return fe;
}
EXPORT_SYMBOL(lnbh25_attach);

module_param(debug, int, 0644);
MODULE_DESCRIPTION("ST LNBH25 driver");
MODULE_AUTHOR("info@netup.ru");
MODULE_LICENSE("GPL");

