/*
 * Cypress TrueTouch Gen5 platform glue
 *
 * Copyright (C) 2013-2014 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CYTTSP5_PLATFORM_H
#define _CYTTSP5_PLATFORM_H

#include "cyttsp5_core.h"

int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev);
int cyttsp5_init(struct cyttsp5_core_platform_data *pdata, int on,
		struct device *dev);
int cyttsp5_power(struct cyttsp5_core_platform_data *pdata, int on,
		struct device *dev, atomic_t *ignore_irq);
int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev);
int cyttsp5_detect(struct cyttsp5_core_platform_data *pdata,
		struct device *dev, cyttsp5_platform_read read);

#endif /* _CYTTSP5_PLATFORM_H */
