/*
 * TI LM36923H backlight driver for the RED Hydrogen One
 *
 * Copyright (C) 2017-2018 Texas Instruments Incorporated
 * Copyright (C) 2026 RED Hydrogen One LineageOS contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define LM36923H_ENABLE			0x10
#define LM36923H_BRIGHTNESS_CTRL	0x11
#define LM36923H_BOOST_CTRL		0x13
#define LM36923H_AUTO_FREQ_HI		0x15
#define LM36923H_AUTO_FREQ_LO		0x16
#define LM36923H_BRIGHTNESS_LSB		0x18
#define LM36923H_BRIGHTNESS_MSB		0x19

#define LM36923H_DEVICE_ENABLE		0x01
#define LM36923H_ALL_STRINGS_ENABLE	0x0f
#define LM36923H_SECONDARY_ADDR		0x37
#define LM36923H_MAX_CURRENT_MA		25
#define LM36923H_MAX_BRIGHTNESS		255

struct lm36923h_reg {
	u8 reg;
	u8 value;
};

/* Sequence recovered from the stock .118 kernel image. */
static const struct lm36923h_reg lm36923h_init_sequence[] = {
	{ LM36923H_BRIGHTNESS_CTRL, 0x02 },
	{ LM36923H_BOOST_CTRL, 0x3c },
	{ LM36923H_AUTO_FREQ_HI, 0x50 },
	{ LM36923H_AUTO_FREQ_LO, 0x29 },
	{ LM36923H_BRIGHTNESS_LSB, 0x00 },
	{ LM36923H_BRIGHTNESS_MSB, 0x0f },
	{ LM36923H_ENABLE, LM36923H_ALL_STRINGS_ENABLE },
};

struct lm36923h {
	struct i2c_client *client;
	struct led_classdev led;
	struct mutex lock; /* Protects I2C writes and initialization state. */
	struct work_struct brightness_work;
	int hwen_gpio;
	u32 current_limit_ma;
	bool dual_backlight;
	bool initialized;
};

static int lm36923h_write(struct lm36923h *lm, u16 addr, u8 reg, u8 value)
{
	u8 data[] = { reg, value };
	struct i2c_msg message = {
		.addr = addr,
		.flags = 0,
		.len = sizeof(data),
		.buf = data,
	};
	int ret;

	ret = i2c_transfer(lm->client->adapter, &message, 1);
	if (ret == 1)
		return 0;

	return ret < 0 ? ret : -EIO;
}

static int lm36923h_write_pair(struct lm36923h *lm, u8 reg, u8 value)
{
	int ret;

	ret = lm36923h_write(lm, lm->client->addr, reg, value);
	if (ret)
		return ret;

	if (!lm->dual_backlight)
		return 0;

	return lm36923h_write(lm, LM36923H_SECONDARY_ADDR, reg, value);
}

static int lm36923h_write_sequence(struct lm36923h *lm, u16 addr)
{
	const struct lm36923h_reg *entry;
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(lm36923h_init_sequence); i++) {
		entry = &lm36923h_init_sequence[i];
		ret = lm36923h_write(lm, addr, entry->reg, entry->value);
		if (ret)
			return ret;
	}

	return 0;
}

static int lm36923h_write_brightness(struct lm36923h *lm,
				     unsigned int brightness)
{
	u8 scaled;
	int ret;

	brightness = min_t(unsigned int, brightness, LM36923H_MAX_BRIGHTNESS);
	if (lm->current_limit_ma < LM36923H_MAX_CURRENT_MA) {
		u32 limited_max;

		limited_max = DIV_ROUND_CLOSEST(LM36923H_MAX_BRIGHTNESS *
						lm->current_limit_ma,
						LM36923H_MAX_CURRENT_MA);
		brightness = brightness * limited_max /
			     LM36923H_MAX_BRIGHTNESS;
	}
	scaled = brightness;

	/* Stock maps the 8-bit LED ABI into the LM36923H 11-bit pair. */
	ret = lm36923h_write_pair(lm, LM36923H_BRIGHTNESS_LSB, 0);
	if (ret)
		return ret;

	return lm36923h_write_pair(lm, LM36923H_BRIGHTNESS_MSB, scaled);
}

static int lm36923h_init_registers(struct lm36923h *lm)
{
	int ret;

	gpio_set_value_cansleep(lm->hwen_gpio, 1);

	ret = lm36923h_write_pair(lm, LM36923H_ENABLE,
				  LM36923H_DEVICE_ENABLE);
	if (ret)
		goto disable;

	ret = lm36923h_write_sequence(lm, lm->client->addr);
	if (ret)
		goto disable;

	if (lm->dual_backlight) {
		ret = lm36923h_write_sequence(lm, LM36923H_SECONDARY_ADDR);
		if (ret)
			goto disable;
	}

	/* Avoid a visible flash while userspace still requests LED_OFF. */
	ret = lm36923h_write_brightness(lm, LED_OFF);
	if (ret)
		goto disable;

	lm->initialized = true;
	return 0;

disable:
	lm->initialized = false;
	gpio_set_value_cansleep(lm->hwen_gpio, 0);
	return ret;
}

static int lm36923h_set_brightness(struct lm36923h *lm,
				   unsigned int brightness)
{
	int ret;

	mutex_lock(&lm->lock);
	if (!lm->initialized) {
		ret = lm36923h_init_registers(lm);
		if (ret)
			goto out;
	}

	ret = lm36923h_write_brightness(lm, brightness);
out:
	mutex_unlock(&lm->lock);
	return ret;
}

static void lm36923h_brightness_work(struct work_struct *work)
{
	struct lm36923h *lm = container_of(work, struct lm36923h,
					  brightness_work);
	int ret;

	ret = lm36923h_set_brightness(lm, lm->led.brightness);
	if (ret)
		dev_err(&lm->client->dev,
			"brightness update failed: %d\n", ret);
}

static void lm36923h_brightness_set(struct led_classdev *led,
				    enum led_brightness brightness)
{
	struct lm36923h *lm = container_of(led, struct lm36923h, led);

	queue_work(system_power_efficient_wq, &lm->brightness_work);
}

static int lm36923h_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device_node *node = client->dev.of_node;
	struct lm36923h *lm;
	u32 current_limit;
	int ret;

	if (!node)
		return -EINVAL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EIO;

	lm = devm_kzalloc(&client->dev, sizeof(*lm), GFP_KERNEL);
	if (!lm)
		return -ENOMEM;

	lm->client = client;
	lm->dual_backlight = of_property_read_bool(node, "dual-3d-bkl");
	lm->current_limit_ma = LM36923H_MAX_CURRENT_MA;
	ret = of_property_read_u32(node, "current-limitation", &current_limit);
	if (!ret) {
		if (!current_limit || current_limit > LM36923H_MAX_CURRENT_MA) {
			dev_err(&client->dev,
				"invalid current-limitation %u mA\n",
				current_limit);
			return -EINVAL;
		}
		lm->current_limit_ma = current_limit;
	} else if (ret != -EINVAL) {
		return ret;
	}

	lm->hwen_gpio = of_get_named_gpio(node, "hwen-gpio", 0);
	if (lm->hwen_gpio == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (!gpio_is_valid(lm->hwen_gpio)) {
		dev_err(&client->dev, "missing valid hwen-gpio\n");
		return -EINVAL;
	}

	ret = devm_gpio_request_one(&client->dev, lm->hwen_gpio,
				    GPIOF_OUT_INIT_LOW, "lm36923h-hwen");
	if (ret)
		return ret;

	mutex_init(&lm->lock);
	INIT_WORK(&lm->brightness_work, lm36923h_brightness_work);
	i2c_set_clientdata(client, lm);

	ret = lm36923h_init_registers(lm);
	if (ret) {
		dev_err(&client->dev,
			"initialization failed at 0x%02x%s: %d\n",
			client->addr, lm->dual_backlight ? "/0x37" : "", ret);
		return ret;
	}

	lm->led.name = "LM36923H-BL";
	lm->led.max_brightness = LM36923H_MAX_BRIGHTNESS;
	lm->led.brightness = LED_OFF;
	lm->led.brightness_set = lm36923h_brightness_set;

	ret = led_classdev_register(&client->dev, &lm->led);
	if (ret) {
		lm36923h_write_pair(lm, LM36923H_ENABLE, 0);
		gpio_set_value_cansleep(lm->hwen_gpio, 0);
		return ret;
	}

	dev_info(&client->dev, "LM36923H backlight%s registered\n",
		 lm->dual_backlight ? " pair" : "");
	return 0;
}

static int lm36923h_remove(struct i2c_client *client)
{
	struct lm36923h *lm = i2c_get_clientdata(client);

	led_classdev_unregister(&lm->led);
	cancel_work_sync(&lm->brightness_work);
	mutex_lock(&lm->lock);
	lm36923h_write_pair(lm, LM36923H_ENABLE, 0);
	lm->initialized = false;
	gpio_set_value_cansleep(lm->hwen_gpio, 0);
	mutex_unlock(&lm->lock);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lm36923h_suspend(struct device *dev)
{
	struct lm36923h *lm = dev_get_drvdata(dev);
	int ret;

	cancel_work_sync(&lm->brightness_work);
	mutex_lock(&lm->lock);
	ret = lm36923h_write_pair(lm, LM36923H_ENABLE, 0);
	mutex_unlock(&lm->lock);
	return ret;
}

static int lm36923h_resume(struct device *dev)
{
	struct lm36923h *lm = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&lm->lock);
	ret = lm36923h_write_pair(lm, LM36923H_ENABLE,
				  LM36923H_ALL_STRINGS_ENABLE);
	mutex_unlock(&lm->lock);
	if (!ret)
		queue_work(system_power_efficient_wq, &lm->brightness_work);

	return ret;
}

static SIMPLE_DEV_PM_OPS(lm36923h_pm_ops, lm36923h_suspend,
			  lm36923h_resume);
#define LM36923H_PM_OPS (&lm36923h_pm_ops)
#else
#define LM36923H_PM_OPS NULL
#endif

static const struct of_device_id lm36923h_of_match[] = {
	{ .compatible = "ti,lm36923h" },
	{ }
};
MODULE_DEVICE_TABLE(of, lm36923h_of_match);

static const struct i2c_device_id lm36923h_id[] = {
	{ "lm36923h", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm36923h_id);

static struct i2c_driver lm36923h_driver = {
	.driver = {
		.name = "lm36923h",
		.of_match_table = lm36923h_of_match,
		.pm = LM36923H_PM_OPS,
	},
	.probe = lm36923h_probe,
	.remove = lm36923h_remove,
	.id_table = lm36923h_id,
};
module_i2c_driver(lm36923h_driver);

MODULE_DESCRIPTION("TI LM36923H backlight driver for RED Hydrogen One");
MODULE_LICENSE("GPL v2");
