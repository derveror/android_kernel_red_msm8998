/*
 * Cypress TrueTouch Gen5 platform glue
 *
 * Copyright (C) 2013-2014 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2.
 */

#include "cyttsp5_regs.h"
#include "cyttsp5_platform.h"

int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	gpio_set_value(pdata->rst_gpio, 1);
	msleep(20);
	gpio_set_value(pdata->rst_gpio, 0);
	msleep(40);
	gpio_set_value(pdata->rst_gpio, 1);
	msleep(20);

	return 0;
}

int cyttsp5_init(struct cyttsp5_core_platform_data *pdata, int on,
		struct device *dev)
{
	int rc;

	if (!pdata || !dev)
		return -EINVAL;

	if (!on) {
		gpio_free(pdata->irq_gpio);
		gpio_free(pdata->rst_gpio);
		return 0;
	}

	if (!gpio_is_valid(pdata->rst_gpio) ||
			!gpio_is_valid(pdata->irq_gpio))
		return -EINVAL;

	rc = gpio_request(pdata->rst_gpio, "cyttsp5_reset");
	if (rc)
		return rc;

	rc = gpio_direction_output(pdata->rst_gpio, 1);
	if (rc)
		goto free_reset;

	rc = gpio_request(pdata->irq_gpio, "cyttsp5_irq");
	if (rc)
		goto free_reset;

	rc = gpio_direction_input(pdata->irq_gpio);
	if (rc)
		goto free_irq;

	return 0;

free_irq:
	gpio_free(pdata->irq_gpio);
free_reset:
	gpio_free(pdata->rst_gpio);
	return rc;
}

int cyttsp5_power(struct cyttsp5_core_platform_data *pdata, int on,
		struct device *dev, atomic_t *ignore_irq)
{
	return 0;
}

int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}

int cyttsp5_detect(struct cyttsp5_core_platform_data *pdata,
		struct device *dev, cyttsp5_platform_read read)
{
	char buf;
	int retry = 3;
	int rc = -ENODEV;

	while (retry--) {
		pdata->xres(pdata, dev);
		msleep(100);
		rc = read(dev, &buf, sizeof(buf));
		if (!rc)
			return 0;
	}

	return rc;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cypress TrueTouch Gen5 platform glue");
