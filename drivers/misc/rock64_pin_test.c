#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/syscore_ops.h>
#include <linux/irq.h>

#define LOG(x...)   printk(KERN_INFO "[PIN_TEST]: "x)
#define TIMER_MS_COUNTS         1000

struct pin_gpio_data {
	int io;
	char name[12];
	int enable;//set high or low
};
struct pin_list {
	struct list_head list;
	struct pin_gpio_data gpio_data;
};

struct pin_data {
	struct device *dev;
	struct workqueue_struct         *pin_wq;
	struct delayed_work	pin_work;
	struct list_head pin_head;
};
struct pin_data *pin_data;

static void pin_gpio_work(struct work_struct *work)
{
	struct pin_data *data = container_of(work, struct pin_data,pin_work.work);
	struct list_head *pos;
	struct pin_list *pl;

	if(list_empty(&data->pin_head)) {
		printk("list empty\n");
		return ;
	}

	list_for_each(pos,&data->pin_head) {
		pl = list_entry(pos, struct pin_list,list);
		//printk("%s gpio:%d val=%d\n",pl->gpio_data.name,pl->gpio_data.io,!pl->gpio_data.enable);
		gpio_direction_output(pl->gpio_data.io ,!pl->gpio_data.enable);
		pl->gpio_data.enable = !pl->gpio_data.enable;
		mdelay(10);
		
	}

	queue_delayed_work(data->pin_wq, &data->pin_work, msecs_to_jiffies(3*TIMER_MS_COUNTS));

}

static int pin_parse_dt(struct device *dev, struct pin_data *data)
{
	struct device_node *root = dev->of_node;
	struct device_node *child;
	struct pin_list *pl;
	enum of_gpio_flags flags;
	int ret;

	INIT_LIST_HEAD(&data->pin_head);
	if(!root) {
		dev_err(data->dev, "can't find rock64 pin dt node \n");
 		return -ENODEV;
	}

	for_each_child_of_node(root, child) {
		pl = kmalloc(sizeof(struct pin_list), GFP_KERNEL);
		if(!pl) {
			dev_err(data->dev, "pin_list alloc failed\n");
			return -ENOMEM;
		}
		strcpy(pl->gpio_data.name, child->name);
		pl->gpio_data.io = of_get_gpio_flags(child, 0,&flags);
		pl->gpio_data.enable = !(flags & OF_GPIO_ACTIVE_LOW);
		if (!gpio_is_valid(pl->gpio_data.io)) {
                	dev_err(data->dev, "%s ivalid gpio\n", child->name);
                                        return -EINVAL;
                } 
		ret = gpio_request(pl->gpio_data.io ,pl->gpio_data.name);
		if (ret) {
			dev_err(data->dev, "request %s gpio fail:%d\n", child->name, ret);
		}

		printk("%s gpio :%d,enable=%d\n",pl->gpio_data.name,pl->gpio_data.io,pl->gpio_data.enable);	
		list_add_tail(&pl->list,&data->pin_head);
	}

	return 0;
}
static int pin_test_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pin_data *pdata = NULL;
	if(!pdata){
		pdata = kzalloc(sizeof(struct pin_data),GFP_KERNEL);
		if(!pdata)
			return -ENOMEM;
	}	
	pdata->dev = &pdev->dev;
	ret = pin_parse_dt(&pdev->dev,pdata);
	pdata->pin_wq = alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_FREEZABLE, "rock64-pin-gpio-wq");
	INIT_DELAYED_WORK(&pdata->pin_work, pin_gpio_work);

	queue_delayed_work(pdata->pin_wq, &pdata->pin_work, msecs_to_jiffies(3000));

	
	return ret;
}
static int pin_test_remove(struct platform_device *pdev)
{
	return 0;
}
static int pin_test_suspend(struct platform_device *pdev, pm_message_t state)
{
    LOG("Enter %s\n", __func__);
    return 0;
}

static int pin_test_resume(struct platform_device *pdev)
{
    LOG("Enter %s\n", __func__);
    return 0;
}

#ifdef CONFIG_OF
static struct of_device_id pin_test_of_match[] = {
    { .compatible = "pi2-test" },
    { }
};
MODULE_DEVICE_TABLE(of, pin_test_of_match);
#endif //CONFIG_OF

static struct platform_driver pin_driver = {
        .probe = pin_test_probe,
        .remove = pin_test_remove,
    	.suspend = pin_test_suspend,
    	.resume = pin_test_resume,
        .driver = {
                .name = "pi2-test",
                .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(pin_test_of_match),
        },   
};

static int __init pin_init(void)
{
    LOG("Enter %s\n", __func__);
        return platform_driver_register(&pin_driver);
}

static void __exit pin_exit(void)
{
    LOG("Enter %s\n", __func__);
        platform_driver_unregister(&pin_driver);
}

module_init(pin_init);
module_exit(pin_exit);

MODULE_DESCRIPTION("rock64 pin fun test driver");
MODULE_AUTHOR("test@test.com");
MODULE_LICENSE("GPL");
