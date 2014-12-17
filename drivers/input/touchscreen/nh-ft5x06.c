/*
 * NEWHAVEN FT5X06 Touchscreen Controller Driver
 * 		NHD-5.0-800480TF-ATXL#-CTP-ND
 *		http://www.newhavendisplay.com/specs/NHD-5.0-800480TF-ATXL-CTP.pdf
 * Copyright (C) 2014 Intronik GmbH
 *	Daniel Gross <dgross@intronik.de>
 *
 * Base of the kernel driver:
 *  - st1232.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/platform_data/nh_ft5x06_pdata.h>

#define NH_FT5X06_TS_NAME "nh-ft5x06-ts"

#define MIN_X		0x00
#define MIN_Y		0x00
#define MAX_X		(800-1)		// TODO: make that a platform/device tree parameter
#define MAX_Y		(480-1)		// TODO: make that a platform/device tree parameter
#define MAX_FINGERS	1

struct nh_ft5x06_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct dev_pm_qos_request low_latency_req;
	int touch_gpio;
};

static int nh_ft5x06_ts_check(struct i2c_client *client, const u8* buffer, int offset, u8 expected, const char* registerName)
{
	if (buffer[offset]!=expected) {
		dev_err(&client->dev, "register: \"%s\" got 0x%.2X expected 0x%.2X", registerName, buffer[offset], expected);
		return -1;
	}
	return 0;
}

static int nh_ft5x06_ts_identify(struct i2c_client *client)
{
	struct i2c_msg msg[2];
	int error;
	u8 start_reg;
	u8 buf[10];

	/* read touchscreen data from FT5x06 */
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = 0xA1;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;

	error = i2c_transfer(client->adapter, msg, 2);
	if (error < 0)
		return error;

	// check, various firmware bytes
/*	if (
		(nh_ft5x06_ts_check(client, buf, 0xA1-start_reg, 0x30, "ID_G_LIB_VERSION_H")!=0)||
		(nh_ft5x06_ts_check(client, buf, 0xA2-start_reg, 0x01, "ID_G_LIB_VERSION_L")!=0)||
		(nh_ft5x06_ts_check(client, buf, 0xA3-start_reg, 0x55, "ID_G_CIPHER")!=0)||
		(nh_ft5x06_ts_check(client, buf, 0xA6-start_reg, 0x05, "ID_G_FIRMID")!=0))
			return -1;
*/
	return 0;
}

static irqreturn_t nh_ft5x06_ts_irq_handler(int irq, void *dev_id)
{
	struct nh_ft5x06_ts_data *ts = dev_id;
	struct input_dev *input_dev = ts->input_dev;
	struct i2c_client *client = ts->client;
	struct i2c_msg msg[2];
	u8 start_reg;
	u8 buf[6*MAX_FINGERS+1];
	u8* prawfinger;
	int error;
	int count = 0;
	unsigned i;
	unsigned c;

	/* read touchscreen data from FT5x06 */
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = 0x02;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;

	error = i2c_transfer(client->adapter, msg, 2);
	if (error < 0)
		goto end;

	
	c = buf[0];			// get number of fingers
	if (c>MAX_FINGERS)		// limit that value
		c=MAX_FINGERS;		
	prawfinger = &buf[1];		// set start pointer of buffer to first finger struct

	/* multi touch protocol A
		see /Documentation/input/multi-touch-protocol.txt
	*/
	for (i=0; i<buf[0]; i++)
	{
		u16 x = ((prawfinger[0]&0xF)<<8)|(prawfinger[1]);
		u16 y = ((prawfinger[2]&0xF)<<8)|(prawfinger[3]);
		u8 ev = prawfinger[0]>>6;	// 2 bit event flag
		//u8 id = prawfinger[2]>>4;	// 4bit touch id
		prawfinger+=6;			// advance to next finger in raw data
		// only report contact events, since touch down events are sometimes buggy
		if (ev==0x02)
		{
			input_report_abs(input_dev, ABS_X, x);
			input_report_abs(input_dev, ABS_Y, y);
//			input_mt_sync(input_dev);	//  SYN_MT_REPORT
			count++;
		}
	}
	input_report_key(input_dev, BTN_TOUCH, count>0);

	/* SYN_MT_REPORT only if no contact */
//	if (!count) {
//		input_mt_sync(input_dev);
//		if (ts->low_latency_req.dev) {
//			dev_pm_qos_remove_request(&ts->low_latency_req);
//			ts->low_latency_req.dev = NULL;
//		}
//	} else if (!ts->low_latency_req.dev) {
//		/* First contact, request 100 us latency. */
//		dev_pm_qos_add_ancestor_request(&ts->client->dev,
//						&ts->low_latency_req, 100);
//	}

	/* SYN_REPORT */
	input_sync(input_dev);
end:
	return IRQ_HANDLED;
}

static int nh_ft5x06_ts_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct nh_ft5x06_ts_data *ts;
	struct nh_ft5x06_pdata *pdata = dev_get_platdata(&client->dev);
	struct input_dev *input_dev;
	int error=0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "need I2C_FUNC_I2C\n");
		return -EIO;
	}

	if (nh_ft5x06_ts_identify(client))
		return -ENODEV;

	ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(&client->dev);
	if (!input_dev)
		return -ENOMEM;

	ts->client = client;
	ts->input_dev = input_dev;

	if (pdata)
		ts->touch_gpio = pdata->touch_gpio;
	else if (client->dev.of_node)
		ts->touch_gpio = of_get_named_gpio(client->dev.of_node, "touch-gpio", 0);
	else
		ts->touch_gpio = -ENODEV;

	if (gpio_is_valid(ts->touch_gpio)) {
		if (client->irq>0) {
			dev_err(&client->dev,
				"Configuration conflict, do not specify both interrupt %d and touch gpio %d.\n",
				client->irq, ts->touch_gpio);
				return error;
		}
		error = devm_gpio_request_one(&client->dev, ts->touch_gpio, GPIOF_IN|GPIOF_EXPORT, "nh-ft5x06-touch");
		if (error) {
			dev_err(&client->dev,
				"Unable to request GPIO pin %d.\n",
				ts->touch_gpio);
				return error;
		}		
		client->irq = gpio_to_irq(ts->touch_gpio);
		if (client->irq < 0) {
			error = client->irq;
			dev_err(&client->dev,
				"Unable to get irq number for GPIO %d, error %d\n",
				ts->touch_gpio, error);
			return error;
		}
	}

	if (!client->irq) {
		dev_err(&client->dev, "no IRQ?\n");
		return -EINVAL;
	}

	input_dev->name = "nh-ft5x06-touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

//	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, MAX_FINGERS, 0, 0);
	input_set_abs_params(input_dev, ABS_X, MIN_X, MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, MIN_Y, MAX_Y, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_POSITION_X, MIN_X, MAX_X, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, MIN_Y, MAX_Y, 0, 0);

	error = devm_request_threaded_irq(&client->dev, client->irq,
					  NULL, nh_ft5x06_ts_irq_handler,
					  IRQF_ONESHOT|IRQF_TRIGGER_FALLING,
					  client->name, ts);
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		return error;
	}

	error = input_register_device(ts->input_dev);
	if (error) {
		dev_err(&client->dev, "Unable to register %s input device\n",
			input_dev->name);
		return error;
	}

	i2c_set_clientdata(client, ts);
	device_init_wakeup(&client->dev, 1);

	return 0;
}

static int nh_ft5x06_ts_remove(struct i2c_client *client)
{
	device_init_wakeup(&client->dev, 0);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nh_ft5x06_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (device_may_wakeup(&client->dev)) {
		enable_irq_wake(client->irq);
	} else {
		disable_irq(client->irq);
	}

	return 0;
}

static int nh_ft5x06_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	
	if (device_may_wakeup(&client->dev)) {
		disable_irq_wake(client->irq);
	} else {
		enable_irq(client->irq);
	}

	return 0;
}

#endif

static SIMPLE_DEV_PM_OPS(nh_ft5x06_ts_pm_ops,
			 nh_ft5x06_ts_suspend, nh_ft5x06_ts_resume);

static const struct i2c_device_id nh_ft5x06_ts_id[] = {
	{ NH_FT5X06_TS_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nh_ft5x06_ts_id);

#ifdef CONFIG_OF
static const struct of_device_id nh_ft5x06_ts_dt_ids[] = {
	{ .compatible = "newhaven,ft5306", },
	{ .compatible = "newhaven,ft5x06", },
	{ }
};
MODULE_DEVICE_TABLE(of, nh_ft5x06_ts_dt_ids);
#endif

static struct i2c_driver nh_ft5x06_ts_driver = {
	.probe		= nh_ft5x06_ts_probe,
	.remove		= nh_ft5x06_ts_remove,
	.id_table	= nh_ft5x06_ts_id,
	.driver = {
		.name	= NH_FT5X06_TS_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(nh_ft5x06_ts_dt_ids),
		.pm	= &nh_ft5x06_ts_pm_ops,
	},
};

module_i2c_driver(nh_ft5x06_ts_driver);

MODULE_AUTHOR("Daniel Gross <dgross@intronik.de>");
MODULE_DESCRIPTION("NEWHAVEN FT5X06 Touchscreen Controller Driver");
MODULE_LICENSE("GPL");
