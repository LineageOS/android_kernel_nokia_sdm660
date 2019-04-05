#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

//#define HALLSENSOR_VDD_CONTROL

//Should be defined in input.h
#define KEY_HALL_CLOSE          0x2f0
#define KEY_HALL_OPEN           0x2f1

struct hall_info {
    unsigned irq_gpio;
    unsigned long irq_flags;
    int (*gpio_config)(unsigned gpio, bool configure);
    struct mutex hall_mutex;
    struct work_struct irq_work;
    struct workqueue_struct *hall_work;
    int irq;
    struct input_dev *input_hall;
    unsigned int keycode_open;	/* input event code (KEY_*, SW_*) for open */
    unsigned int keycode_close;	/* input event code (KEY_*, SW_*) for close */
		/* power related */
		struct regulator *vdd;
};

enum hall_status {
    HALL_CLOSED = 0,
    HALL_OPENED
};

struct hall_event_info {
    enum hall_status current_status;
    unsigned int disable_event;
};

static struct hall_event_info gHallEventInfo;

static int hall_gpio_setup(unsigned gpio, bool configure)
{
    int retval=0;
    if (configure)
        {
            retval = gpio_request(gpio, "hall_sensor");
            if (retval) {
                pr_err("%s: Failed to get attn gpio %d. Code: %d.",
                    __func__, gpio, retval);
                return retval;
            }
            retval = gpio_direction_input(gpio);
            if (retval) {
                pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
                    __func__, gpio, retval);
                gpio_free(gpio);
            }
    } else {
            pr_warn("%s: No way to deconfigure gpio %d.",
                    __func__, gpio);
    }
    return retval;
}

static struct hall_info hallsensor_info = {
    .irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
    .irq_gpio = 0,
    .gpio_config = hall_gpio_setup,
};

static ssize_t enable_switch_store (struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    unsigned int enable;
    unsigned long data;

    enable = (unsigned int)kstrtoul(buf, 10, &data);
    gHallEventInfo.disable_event = enable;

    return count;
}

static ssize_t enable_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned int enable = gHallEventInfo.disable_event;

    return sprintf(buf, "%d", enable);
}


static ssize_t Hall_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int hall_status;
    hall_status = gpio_get_value(hallsensor_info.irq_gpio);
    
    pr_info("hall_status(%d)\n", hall_status);
    return sprintf(buf, "%d\n", hall_status);
}
static DEVICE_ATTR(Hall_status, 0444, Hall_show, NULL);
static DEVICE_ATTR(Hall_enable, 0644, enable_switch_show, enable_switch_store);

static irqreturn_t hall_irq_handler(int irq, void *handle)
{
    struct hall_info *pdata = handle;

    if(pdata == NULL)
        return IRQ_HANDLED;
    if(pdata->hall_work != NULL)
        queue_work(pdata->hall_work, &pdata->irq_work);
    else
        pr_info("[Hall sensor] : queue_work fail\n");
    //pr_info("[Hall sensor] : %s\n",__FUNCTION__);

    return IRQ_HANDLED;
}

static void irq_work_func(struct work_struct *work)
{
    struct hall_info *pdata = container_of((struct work_struct *)work,
                        struct hall_info, irq_work);

    pr_info("[Hall sensor] : irq triggered ! \n");
    pr_info("gpio status : %d\n" , gpio_get_value(hallsensor_info.irq_gpio));

    if(gHallEventInfo.disable_event)
        return;

    gHallEventInfo.current_status = gpio_get_value(hallsensor_info.irq_gpio);

    if(gHallEventInfo.current_status == HALL_CLOSED){
        input_report_key(pdata->input_hall, pdata->keycode_close, 1);
        input_sync(pdata->input_hall);
        input_report_key(pdata->input_hall, pdata->keycode_close, 0);
        input_sync(pdata->input_hall);
    }else{
        input_report_key(pdata->input_hall, pdata->keycode_open, 1);
        input_sync(pdata->input_hall);
        input_report_key(pdata->input_hall, pdata->keycode_open, 0);
        input_sync(pdata->input_hall);
    }
}

static  int hall_sensor_probe(struct platform_device *pdev)
{
    int ret;
    struct device_node *np=pdev->dev.of_node;
    struct hall_info *pdata = &hallsensor_info;

    pr_info("[Hall sensor] : %s enter\n",__FUNCTION__);

    pdata->irq_gpio = of_get_named_gpio(np, "hallsensor,irq-gpio", 0);
    mutex_init(&(pdata->hall_mutex));

    if (pdata->gpio_config) {
        ret = pdata->gpio_config(pdata->irq_gpio,
                            true);
        if (ret < 0) {
            pr_info("%s: Failed to configure GPIO\n",
                    __func__);
            return ret;
        }
    }

    pdata->irq = gpio_to_irq(pdata->irq_gpio);
    pdata->input_hall = input_allocate_device();
    if (!pdata->input_hall) {
        pr_info("Not enough memory for input device\n");
        return -ENOMEM;
    }

		if (of_property_read_u32(np, "hall-open,linux,code", &pdata->keycode_open)) {
			pr_info("Hall-OPEN without keycode\n");
			pdata->keycode_open = KEY_HALL_OPEN;
		}

		if (of_property_read_u32(np, "hall-close,linux,code", &pdata->keycode_close)) {
			pr_info("Hall-CLOSE without keycode\n");
			pdata->keycode_close = KEY_HALL_CLOSE;
		}

#ifdef HALLSENSOR_VDD_CONTROL
		pdata->vdd = regulator_get(&pdev->dev, "vdd");
		if (IS_ERR(pdata->vdd)) {
			ret = PTR_ERR(pdata->vdd);
			pr_err("Regulator get failed(vdd) ret=%d\n", ret);
			return ret;
		}

		if (regulator_count_voltages(pdata->vdd) > 0) {
			ret = regulator_set_voltage(pdata->vdd, 1800000, 1800000);
			if (ret) {
			  pr_err("regulator_count_voltages(vdd) failed  ret=%d\n", ret);
				return ret;
			}
		}

		ret = regulator_enable(pdata->vdd);
		if (ret) {
		  pr_err("Regulator vdd enable failed ret=%d\n", ret);
			return ret;
		}
#endif

    pdata->input_hall->name = "hallsensor";
    set_bit(EV_KEY,pdata->input_hall->evbit);
    set_bit(pdata->keycode_open, pdata->input_hall->keybit);
    set_bit(pdata->keycode_close, pdata->input_hall->keybit);

    ret = input_register_device(pdata->input_hall);
    if (ret) {
        pr_info("%s: Failed to register input device\n",
                __func__);
        return ret;
    }

    pdata->hall_work = create_workqueue("hallsensor");
    if(!pdata->hall_work) {
        pr_info("[Hall sensor]%s create_workqueue fail", __func__);
        return -1;
    }

    INIT_WORK(&pdata->irq_work, irq_work_func);

    ret = request_irq(pdata->irq, hall_irq_handler,pdata->irq_flags, "hall_sensor", pdata);
    if (ret < 0) {
            pr_info("%s: Failed to request_threaded_irq\n",
            __func__);
            return ret;
    }
    disable_irq_nosync(pdata->irq);
    enable_irq(pdata->irq);
    if(strstr(saved_command_line,"androidboot.mode=2")!=NULL) //for FTM mode
      pr_info("%s: FTM Mode enter!! No wake-up irq\n",__func__);
    else
      enable_irq_wake(pdata->irq);

    ret = device_create_file(&pdata->input_hall->dev, &dev_attr_Hall_status);
     if (ret) {
        dev_err(&pdev->dev, "%s: dev_attr_Hall_status failed\n", __func__);
        return ret;
    }

    ret = device_create_file(&pdata->input_hall->dev, &dev_attr_Hall_enable);
    if (ret) {
        dev_err(&pdev->dev, "%s: dev_attr_Hall_enable failed\n", __func__);
    }

    ret = sysfs_create_link(pdata->input_hall->dev.kobj.parent,
                            &pdata->input_hall->dev.kobj,
                            pdata->input_hall->name);
    if (ret < 0)    {
        pr_info("[Hall sensor] %s Can't create soft link in hall driver\n", __func__);
    }

    gHallEventInfo.current_status = gpio_get_value(hallsensor_info.irq_gpio);
    gHallEventInfo.disable_event = false;

    pr_info("[Hall sensor] : %s exit\n",__FUNCTION__);

    return ret;
}

static int hall_sensor_remove(struct platform_device *pdev)
{
    struct hall_info *pdata = &hallsensor_info;

#ifdef HALLSENSOR_VDD_CONTROL
    regulator_disable(pdata->vdd);
#endif

    pr_info("[Hall sensor] : %s\n",__FUNCTION__);
    destroy_workqueue(pdata->hall_work);
    input_unregister_device(pdata->input_hall);
    free_irq(pdata->irq, 0);

    return 0;
}

static const struct of_device_id hall_sensor_device_of_match[]  = {
    { .compatible = "fih,hallsensor", },
    {},
};

static struct platform_driver hall_sensor_driver = {
    .driver = {
        .name = "hall_sensor",
        .owner = THIS_MODULE,
        .of_match_table = hall_sensor_device_of_match,
    },
    .probe = hall_sensor_probe,
    .remove = hall_sensor_remove,
};

/*static int __init hall_senso_init(void)
{
    pr_info("F@SENSOR:hall_senso_init\n");
    return platform_driver_register(&hall_sensor_driver);
}


static void __exit hall_senso_exit(void)
{
    platform_driver_unregister(&hall_sensor_driver);
}*/

/*module_init(hall_senso_init);
module_exit(hall_senso_exit);*/
module_platform_driver(hall_sensor_driver);
MODULE_DEVICE_TABLE(of, hall_sensor_device_of_match);

MODULE_LICENSE("Proprietary");
