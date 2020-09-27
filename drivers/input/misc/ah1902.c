#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#define DEV_NAME "ah1902"

struct ah1902_platform_data {
	struct input_dev *idev;
	struct mutex dlock;
	struct work_struct dwork;
	struct workqueue_struct *dwq;
	struct regulator *vdd;

	int irq;
	int status;
	unsigned irq_gpio;
};

static void ah1902_work_func(struct work_struct *work)
{

}

static irqreturn_t ah1902_irq_handle(int irq, void *info)
{
	struct ah1902_platform_data *pdata = info;

	schedule_work(&pdata->dwork);

	return IRQ_HANDLED;
}

#if 1
static int ah1902_power_init(struct platform_device *pdev, bool en)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct ah1902_platform_data *pdata = platform_get_drvdata(pdev);

	pr_info("%s %s", __func__, (en==true)?"ON":"OFF");

	if(en) {
		pdata->vdd = regulator_get(dev, "vdd");
		if(IS_ERR(pdata->vdd)) {
			ret = PTR_ERR(pdata->vdd);
			pr_err("regulator get fail, ret=%d\n", ret);
			return ret;
		}

		ret = regulator_enable(pdata->vdd);
		if(ret) {
			pr_err("regulator enable fail, ret=%d\n", ret);
			return ret;
		}
	} else {
		regulator_put(pdata->vdd);
		regulator_disable(pdata->vdd);
	}

	return ret;
}
#endif

static ssize_t ah1902_status_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of((struct device *)dev, struct platform_device, dev);
	struct ah1902_platform_data *pdata = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", gpio_get_value(pdata->irq_gpio));
}

static DEVICE_ATTR(status_check, 0444, ah1902_status_show, NULL);

static struct attribute *ah1902_attributes[] = {
	&dev_attr_status_check.attr,
	NULL
};

static const struct attribute_group ah1902_attribute_group = {
	.attrs = ah1902_attributes,
};


static int parse_dt_to_pdata(struct device *dev, struct ah1902_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;

	pdata->irq_gpio = of_get_named_gpio(np, "hallsensor,irq-gpio", 0);
	if(!pdata->irq_gpio) {
		pr_err("fail to config gpio\n");
		return rc;
	}
	rc = gpio_request(pdata->irq_gpio, "hall_sensor");
	if (rc) {
		pr_err("%s: Failed to get attn gpio %d. Code: %d.",
			__func__, pdata->irq_gpio, rc);
		return rc;
	}
	rc = gpio_direction_input(pdata->irq_gpio);
	if (rc) {
		pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
			__func__, pdata->irq_gpio, rc);
		gpio_free(pdata->irq_gpio);
	}

	return 0;
}



#ifdef CONFIG_PM
static int ah1902_suspend(struct device *dev)
{
	return 0;
}

static int ah1902_resume(struct device *dev)
{
	return 0;
}
#endif

static int ah1902_probe(struct platform_device *pdev)
{
	int err;
#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
#endif
	struct ah1902_platform_data *pdata;

	pr_info("%s\n", __func__);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if(!pdata) {
		pr_err("%s, %d No pdata\n", __func__, __LINE__);
		err = -ENOMEM;
		goto exit;
	}

#ifdef CONFIG_OF
	if(!np) {
		pr_err("%s, %d No pdata and dts\n", __func__, __LINE__);
		err = -ENOMEM;
		goto fail_parse_dt;
	}

	err = parse_dt_to_pdata(&pdev->dev, pdata);
	if(err) {
		pr_err("fail to parse dtsi\n");
		goto fail_parse_dt;
	}
#endif

	mutex_init(&pdata->dlock);
	pdata->dwq = create_singlethread_workqueue("ah1902");
	INIT_WORK(&pdata->dwork, ah1902_work_func);
	pdata->irq = gpio_to_irq(pdata->irq_gpio);

	err = request_irq(pdata->irq, ah1902_irq_handle,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, DEV_NAME, pdata);
	if(err) {
		pr_err("fail to request irq\n");
		goto fail_irq_request;
	}

	disable_irq_nosync(pdata->irq);
	enable_irq(pdata->irq);
	//enable_irq_wake(pdata->irq);

	pdata->idev = input_allocate_device();
	if(!pdata->idev) {
		pr_err("fail to allocate device\n");
		goto fail_allocate_deivce;
	}

	pdata->idev->name = DEV_NAME;
	set_bit(EV_ABS, pdata->idev->evbit);
	input_set_abs_params(pdata->idev, ABS_MISC, 0, 1, 0, 0);
	input_set_drvdata(pdata->idev, pdata);
	platform_set_drvdata(pdev, pdata);

#if 1
	err = ah1902_power_init(pdev, true);
	if(err) {
		goto fail_power_init;
	}
#endif
	err = input_register_device(pdata->idev);
	if(err) {
		pr_err("fail to register device\n");
		goto fail_register_input_dev;
	}

	err = sysfs_create_group(&pdata->idev->dev.kobj, &ah1902_attribute_group);
	if(err) {
		pr_err("fail to create sysfs group\n");
		goto fail_create_sysfs_group;
	}

	err = sysfs_create_link(pdata->idev->dev.kobj.parent, &pdata->idev->dev.kobj, DEV_NAME);
	if(err) {
		pr_err("fail to create sysfs link\n");
		goto fail_create_sysfs_link;
	}

	pr_info("%s exit\n",__func__);
	return 0;

fail_create_sysfs_link:
	sysfs_remove_group(&pdata->idev->dev.kobj, &ah1902_attribute_group);
fail_create_sysfs_group:
	input_unregister_device(pdata->idev);
fail_register_input_dev:
	input_free_device(pdata->idev);
fail_power_init:
fail_allocate_deivce:
	free_irq(pdata->irq, pdata);
fail_irq_request:
	cancel_work_sync(&pdata->dwork);
	flush_workqueue(pdata->dwq);
	destroy_workqueue(pdata->dwq);
	mutex_destroy(&pdata->dlock);
fail_parse_dt:
	kfree(pdata);
exit:
	pr_err("%s fail\n", __func__);
	printk("BBox::UEC;8::54\n");
	return err;
}

static int ah1902_remove(struct platform_device *pdev)
{
	struct ah1902_platform_data *pdata = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdata->idev->dev.kobj, &ah1902_attribute_group);

	input_unregister_device(pdata->idev);
	input_free_device(pdata->idev);

	free_irq(pdata->irq, pdata);

	cancel_work_sync(&pdata->dwork);
	flush_workqueue(pdata->dwq);
	destroy_workqueue(pdata->dwq);
	mutex_destroy(&pdata->dlock);

	kfree(pdata);

	return 0;
}

static const struct of_device_id ah1902_device_of_match[] = {
	{.compatible = "ti,ah1902"},
	{},
};

static UNIVERSAL_DEV_PM_OPS( ah1902_pm_ops, ah1902_suspend, ah1902_resume, NULL);

static struct platform_driver ah1902_platform_driver = {
	.driver = {
		.name  = "ah1902",
		.owner = THIS_MODULE,
		.of_match_table = ah1902_device_of_match,
#ifdef CONFIG_PM
		.pm    = &ah1902_pm_ops,
#endif
	},
	.probe     = ah1902_probe,
	.remove    = ah1902_remove,
};
module_platform_driver(ah1902_platform_driver);
MODULE_DEVICE_TABLE(of, ah1902_device_of_match);
