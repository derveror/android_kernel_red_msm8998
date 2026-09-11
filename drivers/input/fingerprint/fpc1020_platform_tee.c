/*
 * FPC1020 Fingerprint sensor platform resource driver
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>

#define FPC_TTW_HOLD_TIME_MS		1000
#define RESET_LOW_SLEEP_MIN_US		5000
#define RESET_LOW_SLEEP_MAX_US		(RESET_LOW_SLEEP_MIN_US + 100)
#define RESET_HIGH_SLEEP1_MIN_US	100
#define RESET_HIGH_SLEEP1_MAX_US	(RESET_HIGH_SLEEP1_MIN_US + 100)
#define RESET_HIGH_SLEEP2_MIN_US	5000
#define RESET_HIGH_SLEEP2_MAX_US	(RESET_HIGH_SLEEP2_MIN_US + 100)
#define PWR_ON_SLEEP_MIN_US		100
#define PWR_ON_SLEEP_MAX_US		(PWR_ON_SLEEP_MIN_US + 900)
#define NUM_PARAMS_REG_ENABLE_SET	2

static const char * const pctl_names[] = {
	"fpc1020_reset_reset",
	"fpc1020_reset_active",
	"fpc1020_irq_active",
};

struct vreg_config {
	const char *name;
	unsigned long vmin;
	unsigned long vmax;
	int ua_load;
};

static const struct vreg_config vreg_conf[] = {
	{ "vdd_ana", 1800000UL, 1800000UL, 6000 },
	{ "vcc_spi", 1800000UL, 1800000UL, 10 },
	{ "vdd_io", 1800000UL, 1800000UL, 6000 },
};

struct fpc1020_data {
	struct device *dev;
	struct pinctrl *fingerprint_pinctrl;
	struct pinctrl_state *pinctrl_state[ARRAY_SIZE(pctl_names)];
	struct regulator *vreg[ARRAY_SIZE(vreg_conf)];
	struct wakeup_source ttw_wl;
	struct mutex lock; /* Serializes sysfs resource changes. */
	struct notifier_block fb_notifier;
	int irq_gpio;
	int rst_gpio;
	bool prepared;
	bool wakeup_feature_enabled;
	bool irq_enabled;
	atomic_t wakeup_enabled;
};

static int select_pin_ctl(struct fpc1020_data *fpc1020, const char *name)
{
	struct device *dev = fpc1020->dev;
	size_t i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
		const char *n = pctl_names[i];

		if (strncmp(n, name, strlen(n)))
			continue;

		rc = pinctrl_select_state(fpc1020->fingerprint_pinctrl,
					  fpc1020->pinctrl_state[i]);
		if (rc)
			dev_err(dev, "cannot select '%s'\n", name);
		else
			dev_dbg(dev, "selected '%s'\n", name);

		return rc;
	}

	dev_err(dev, "%s: '%s' not found\n", __func__, name);
	return -EINVAL;
}

static int vreg_setup(struct fpc1020_data *fpc, const char *name, bool enable)
{
	struct device *dev = fpc->dev;
	struct regulator *vreg;
	size_t i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(vreg_conf); i++) {
		if (!strncmp(vreg_conf[i].name, name,
			     strlen(vreg_conf[i].name)))
			break;
	}

	if (i == ARRAY_SIZE(vreg_conf)) {
		dev_err(dev, "regulator %s not found\n", name);
		return -EINVAL;
	}

	vreg = fpc->vreg[i];
	if (!enable) {
		if (!vreg)
			return 0;

		if (regulator_is_enabled(vreg)) {
			rc = regulator_disable(vreg);
			if (rc)
				return rc;
		}

		regulator_put(vreg);
		fpc->vreg[i] = NULL;
		return 0;
	}

	if (!vreg) {
		vreg = regulator_get(dev, name);
		if (IS_ERR(vreg)) {
			rc = PTR_ERR(vreg);
			dev_err(dev, "failed to get %s: %d\n", name, rc);
			return rc;
		}
	}

	if (regulator_count_voltages(vreg) > 0) {
		rc = regulator_set_voltage(vreg, vreg_conf[i].vmin,
					   vreg_conf[i].vmax);
		if (rc)
			dev_err(dev, "failed to set voltage on %s: %d\n",
				name, rc);
	}

	rc = regulator_set_load(vreg, vreg_conf[i].ua_load);
	if (rc < 0)
		dev_err(dev, "failed to set current on %s: %d\n", name, rc);

	rc = regulator_enable(vreg);
	if (rc) {
		dev_err(dev, "failed to enable %s: %d\n", name, rc);
		regulator_put(vreg);
		return rc;
	}

	fpc->vreg[i] = vreg;
	return 0;
}

static int device_prepare(struct fpc1020_data *fpc1020, bool enable)
{
	int rc = 0;

	mutex_lock(&fpc1020->lock);
	if (enable && !fpc1020->prepared) {
		fpc1020->prepared = true;
		rc = select_pin_ctl(fpc1020, "fpc1020_reset_reset");
		if (rc)
			goto err_prepared;

		rc = vreg_setup(fpc1020, "vcc_spi", true);
		if (rc)
			goto err_prepared;

		rc = vreg_setup(fpc1020, "vdd_io", true);
		if (rc)
			goto err_vcc_spi;

		rc = vreg_setup(fpc1020, "vdd_ana", true);
		if (rc)
			goto err_vdd_io;

		usleep_range(PWR_ON_SLEEP_MIN_US, PWR_ON_SLEEP_MAX_US);
		rc = select_pin_ctl(fpc1020, "fpc1020_reset_active");
		if (rc)
			goto err_vdd_ana;
	} else if (!enable && fpc1020->prepared) {
		select_pin_ctl(fpc1020, "fpc1020_reset_reset");
		usleep_range(PWR_ON_SLEEP_MIN_US, PWR_ON_SLEEP_MAX_US);
		vreg_setup(fpc1020, "vdd_ana", false);
		vreg_setup(fpc1020, "vdd_io", false);
		vreg_setup(fpc1020, "vcc_spi", false);
		fpc1020->prepared = false;
	}

	mutex_unlock(&fpc1020->lock);
	return rc;

err_vdd_ana:
	vreg_setup(fpc1020, "vdd_ana", false);
err_vdd_io:
	vreg_setup(fpc1020, "vdd_io", false);
err_vcc_spi:
	vreg_setup(fpc1020, "vcc_spi", false);
err_prepared:
	fpc1020->prepared = false;
	mutex_unlock(&fpc1020->lock);
	return rc;
}

static int hw_reset(struct fpc1020_data *fpc1020)
{
	int rc;

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_active");
	if (rc)
		return rc;
	usleep_range(RESET_HIGH_SLEEP1_MIN_US, RESET_HIGH_SLEEP1_MAX_US);

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_reset");
	if (rc)
		return rc;
	usleep_range(RESET_LOW_SLEEP_MIN_US, RESET_LOW_SLEEP_MAX_US);

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_active");
	if (rc)
		return rc;
	usleep_range(RESET_HIGH_SLEEP2_MIN_US, RESET_HIGH_SLEEP2_MAX_US);

	dev_info(fpc1020->dev, "IRQ after reset %d\n",
		 gpio_get_value(fpc1020->irq_gpio));
	return 0;
}

static ssize_t clk_enable_set(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	dev_dbg(dev, "clk_enable is not used by the platform driver\n");
	return count;
}
static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

static ssize_t pinctl_set(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int rc;

	mutex_lock(&fpc1020->lock);
	rc = select_pin_ctl(fpc1020, buf);
	mutex_unlock(&fpc1020->lock);
	return rc ? rc : count;
}
static DEVICE_ATTR(pinctl_set, S_IWUSR, NULL, pinctl_set);

static ssize_t regulator_enable_set(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	char name[16];
	char op;
	bool enable;
	int rc;

	if (sscanf(buf, "%15[^,],%c", name, &op) !=
	    NUM_PARAMS_REG_ENABLE_SET)
		return -EINVAL;

	if (op == 'e')
		enable = true;
	else if (op == 'd')
		enable = false;
	else
		return -EINVAL;

	mutex_lock(&fpc1020->lock);
	rc = vreg_setup(fpc1020, name, enable);
	mutex_unlock(&fpc1020->lock);
	return rc ? rc : count;
}
static DEVICE_ATTR(regulator_enable, S_IWUSR, NULL, regulator_enable_set);

static ssize_t device_prepare_set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int rc;

	if (!strncmp(buf, "enable", strlen("enable")))
		rc = device_prepare(fpc1020, true);
	else if (!strncmp(buf, "disable", strlen("disable")))
		rc = device_prepare(fpc1020, false);
	else
		return -EINVAL;

	return rc ? rc : count;
}
static DEVICE_ATTR(device_prepare, S_IWUSR, NULL, device_prepare_set);

static ssize_t hw_reset_set(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int rc;

	if (strncmp(buf, "reset", strlen("reset")))
		return -EINVAL;

	mutex_lock(&fpc1020->lock);
	rc = hw_reset(fpc1020);
	mutex_unlock(&fpc1020->lock);
	return rc ? rc : count;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, hw_reset_set);

static ssize_t wakeup_enable_set(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	ssize_t ret = count;

	mutex_lock(&fpc1020->lock);
	if (!strncmp(buf, "enable", strlen("enable")))
		atomic_set(&fpc1020->wakeup_enabled, 1);
	else if (!strncmp(buf, "disable", strlen("disable")))
		atomic_set(&fpc1020->wakeup_enabled, 0);
	else
		ret = -EINVAL;
	mutex_unlock(&fpc1020->lock);

	return ret;
}
static DEVICE_ATTR(wakeup_enable, S_IWUSR, NULL, wakeup_enable_set);

/*
 * Stock .118 keeps IRQ always available while this feature is enabled.  When
 * disabled, the framebuffer notifier below gates IRQ with display power.
 */
static ssize_t wakeup_feature_set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	ssize_t ret = count;

	mutex_lock(&fpc1020->lock);
	if (!strncmp(buf, "enable", strlen("enable"))) {
		fpc1020->wakeup_feature_enabled = true;
		if (!fpc1020->irq_enabled) {
			enable_irq(gpio_to_irq(fpc1020->irq_gpio));
			fpc1020->irq_enabled = true;
		}
	} else if (!strncmp(buf, "disable", strlen("disable"))) {
		fpc1020->wakeup_feature_enabled = false;
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&fpc1020->lock);

	return ret;
}
static DEVICE_ATTR(wakeup_feature, S_IWUSR, NULL, wakeup_feature_set);

static ssize_t irq_get(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%i\n",
			 gpio_get_value(fpc1020->irq_gpio));
}

static ssize_t irq_ack(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	dev_dbg(dev, "%s\n", __func__);
	return count;
}
static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);

static struct attribute *fpc1020_attributes[] = {
	&dev_attr_pinctl_set.attr,
	&dev_attr_device_prepare.attr,
	&dev_attr_regulator_enable.attr,
	&dev_attr_hw_reset.attr,
	&dev_attr_wakeup_enable.attr,
	&dev_attr_wakeup_feature.attr,
	&dev_attr_clk_enable.attr,
	&dev_attr_irq.attr,
	NULL,
};

static const struct attribute_group fpc1020_attribute_group = {
	.attrs = fpc1020_attributes,
};

static irqreturn_t fpc1020_irq_handler(int irq, void *handle)
{
	struct fpc1020_data *fpc1020 = handle;

	if (atomic_read(&fpc1020->wakeup_enabled))
		__pm_wakeup_event(&fpc1020->ttw_wl, FPC_TTW_HOLD_TIME_MS);

	sysfs_notify(&fpc1020->dev->kobj, NULL, dev_attr_irq.attr.name);
	return IRQ_HANDLED;
}

static int fpc1020_fb_notifier_cb(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct fpc1020_data *fpc1020 = container_of(nb, struct fpc1020_data,
							fb_notifier);
	struct fb_event *evdata = data;
	int *blank;

	if (!evdata || fpc1020->wakeup_feature_enabled ||
	    event != FB_EARLY_EVENT_BLANK || !evdata->data)
		return 0;

	blank = evdata->data;
	mutex_lock(&fpc1020->lock);
	if (*blank == FB_BLANK_UNBLANK && !fpc1020->irq_enabled) {
		enable_irq(gpio_to_irq(fpc1020->irq_gpio));
		fpc1020->irq_enabled = true;
	} else if (*blank == FB_BLANK_POWERDOWN && fpc1020->irq_enabled) {
		disable_irq(gpio_to_irq(fpc1020->irq_gpio));
		fpc1020->irq_enabled = false;
	}
	mutex_unlock(&fpc1020->lock);

	return 0;
}

static int fpc_request_gpio(struct fpc1020_data *fpc1020,
			    const char *label, int *gpio)
{
	struct device *dev = fpc1020->dev;
	int rc;

	rc = of_get_named_gpio(dev->of_node, label, 0);
	if (rc < 0) {
		dev_err(dev, "failed to get '%s': %d\n", label, rc);
		return rc;
	}
	*gpio = rc;

	rc = devm_gpio_request(dev, *gpio, label);
	if (rc)
		dev_err(dev, "failed to request gpio %d: %d\n", *gpio, rc);

	return rc;
}

static int fpc1020_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fpc1020_data *fpc1020;
	int irq;
	int irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
	size_t i;
	int rc;

	fpc1020 = devm_kzalloc(dev, sizeof(*fpc1020), GFP_KERNEL);
	if (!fpc1020)
		return -ENOMEM;

	fpc1020->dev = dev;
	platform_set_drvdata(pdev, fpc1020);

	if (!dev->of_node)
		return -EINVAL;

	rc = fpc_request_gpio(fpc1020, "fpc,gpio_irq", &fpc1020->irq_gpio);
	if (rc)
		return rc;
	rc = fpc_request_gpio(fpc1020, "fpc,gpio_rst", &fpc1020->rst_gpio);
	if (rc)
		return rc;

	fpc1020->fingerprint_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fpc1020->fingerprint_pinctrl))
		return PTR_ERR(fpc1020->fingerprint_pinctrl);

	for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
		fpc1020->pinctrl_state[i] = pinctrl_lookup_state(
			fpc1020->fingerprint_pinctrl, pctl_names[i]);
		if (IS_ERR(fpc1020->pinctrl_state[i]))
			return PTR_ERR(fpc1020->pinctrl_state[i]);
	}

	rc = select_pin_ctl(fpc1020, "fpc1020_reset_reset");
	if (rc)
		return rc;
	rc = select_pin_ctl(fpc1020, "fpc1020_irq_active");
	if (rc)
		return rc;

	atomic_set(&fpc1020->wakeup_enabled, 0);
	if (of_property_read_bool(dev->of_node, "fpc,enable-wakeup")) {
		irqf |= IRQF_NO_SUSPEND;
		device_init_wakeup(dev, true);
	}

	mutex_init(&fpc1020->lock);
	irq = gpio_to_irq(fpc1020->irq_gpio);
	if (irq < 0) {
		rc = irq;
		goto err_mutex;
	}

	rc = devm_request_threaded_irq(dev, irq, NULL, fpc1020_irq_handler,
				       irqf, dev_name(dev), fpc1020);
	if (rc)
		goto err_mutex;

	rc = enable_irq_wake(irq);
	if (rc)
		dev_warn(dev, "failed to mark IRQ %d wakeable: %d\n", irq, rc);

	wakeup_source_init(&fpc1020->ttw_wl, "fpc_ttw_wl");
	rc = sysfs_create_group(&dev->kobj, &fpc1020_attribute_group);
	if (rc)
		goto err_wakeup_source;

	if (of_property_read_bool(dev->of_node, "fpc,enable-on-boot")) {
		rc = device_prepare(fpc1020, true);
		if (rc)
			goto err_sysfs;
	}

	rc = hw_reset(fpc1020);
	if (rc)
		goto err_power;

	fpc1020->wakeup_feature_enabled = true;
	fpc1020->irq_enabled = true;
	fpc1020->fb_notifier.notifier_call = fpc1020_fb_notifier_cb;
	rc = fb_register_client(&fpc1020->fb_notifier);
	if (rc)
		goto err_power;

	dev_info(dev, "%s: ok\n", __func__);
	return 0;

err_power:
	device_prepare(fpc1020, false);
err_sysfs:
	sysfs_remove_group(&dev->kobj, &fpc1020_attribute_group);
err_wakeup_source:
	wakeup_source_trash(&fpc1020->ttw_wl);
	disable_irq_wake(irq);
err_mutex:
	mutex_destroy(&fpc1020->lock);
	return rc;
}

static int fpc1020_remove(struct platform_device *pdev)
{
	struct fpc1020_data *fpc1020 = platform_get_drvdata(pdev);
	int irq = gpio_to_irq(fpc1020->irq_gpio);

	fb_unregister_client(&fpc1020->fb_notifier);
	sysfs_remove_group(&pdev->dev.kobj, &fpc1020_attribute_group);
	device_prepare(fpc1020, false);
	wakeup_source_trash(&fpc1020->ttw_wl);
	disable_irq_wake(irq);
	mutex_destroy(&fpc1020->lock);

	return 0;
}

static const struct of_device_id fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020" },
	{ }
};
MODULE_DEVICE_TABLE(of, fpc1020_of_match);

static struct platform_driver fpc1020_driver = {
	.driver = {
		.name = "fpc1020",
		.owner = THIS_MODULE,
		.of_match_table = fpc1020_of_match,
	},
	.probe = fpc1020_probe,
	.remove = fpc1020_remove,
};

module_platform_driver(fpc1020_driver);

MODULE_AUTHOR("Fingerprint Cards AB <tech@fingerprints.com>");
MODULE_DESCRIPTION("FPC1020 fingerprint sensor platform resource driver");
MODULE_LICENSE("GPL v2");
