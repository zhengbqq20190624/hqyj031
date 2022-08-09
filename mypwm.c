#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include "mypwm.h"
//设备树
/*myplatform{
    compatible = "hqyj,myplatform";
    interrupt-parent = <&gpiof>;
    interrupts =<9 0>;
    reg = <0x12345678 0x1E>;
    led1 = <&gpioe 10 0>;
    fan = <&gpioe 9 0>;
    buzz = <&gpiob 6 0>;
    motor = <&gpiof 6 0>;
};*/
#define CNAME "platform_cdev"
int status = 0;     //0 1切换
int condition = 0;  //好像没什么用
int irqno;      //中断号
int major;
char kbuf[128] = {0};   //fops中read和write用，没用到
char *gpioname[3] = {"fan", "buzz", "motor"};
struct class *cls;
struct device *dev;
struct device_node *node;
struct gpio_desc *desc[3]; //【0】fan 【1】buzz 【2】motor

wait_queue_head_t wq;

//中断
irqreturn_t key_irq(int irq, void *dev)
{
    int i = 0;

    //唤醒
    condition = 1;
    status = 0;
    wake_up_interruptible(&wq);
    //按键中断停止运行
    for (i = 0; i < 3; i++)
    {
        status = gpiod_get_value(desc[i]);
        if (status == 1)
        {
            status = !status;
            gpiod_set_value(desc[i], status);
        }
    }
    return IRQ_HANDLED;
}
// 控制风扇等开关
void ctl_app(struct gpio_desc *desc, int cmd)
{
    gpiod_set_value(desc, cmd);
}
int pdrv_open(struct inode *inode, struct file *file)
{
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
ssize_t pdrv_read(struct file *file, char __user *ubuf, size_t size,
                  loff_t *offs)
{
    int ret;
    // 1.判断是否是阻塞打开的
    if (file->f_flags & O_NONBLOCK)
    {
        return -EINVAL;
    }
    else
    {
        // 2.阻塞
        ret = wait_event_interruptible(wq, condition);
        if (ret < 0)
        {
            printk("receive signal....\n");
            return ret;
        }
    }
    // 3.拷贝数据到用户空间
    if (size > sizeof(status))
        size = sizeof(status);
    ret = copy_to_user(ubuf, &status, size);
    if (ret)
    {
        printk("copy data to user error\n");
        return -EIO;
    }
    // 4.清空condition
    condition = 0;
    return size;
}
ssize_t pdrv_write(struct file *file, const char __user *ubuf,
                   size_t size, loff_t *offs)
{
    int ret;
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    if (size > sizeof(kbuf))
        size = sizeof(kbuf);
    ret = copy_from_user(kbuf, ubuf, size);
    if (ret)
    {
        printk("copy data from user error\n");
        return -EIO;
    }
    return size;
}
//与应用层的交互
long pdrv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret;

    switch (cmd)
    {
    case FAN_ON:
        printk("from user FAN_ON\n");
        ctl_app(desc[0], 1);
        break;
    case FAN_OFF:
        printk("from user FAN_OFF\n");
        ctl_app(desc[0], 0);
        break;

    case BUZZ_ON:
        printk("from user BUZZ_ON\n");
        ctl_app(desc[1], 1);
        break;
    case BUZZ_OFF:
        printk("from user BUZZ_OFF\n");
        ctl_app(desc[1], 0);
        break;
    case MOTOR_ON:
        printk("from user MOTOR_ON\n");
        ctl_app(desc[2], 1);
        break;
    case MOTOR_OFF:
        printk("from user MOTOR_OFF\n");
        ctl_app(desc[2], 0);
        break;
    }
    return 0;
}
int pdrv_close(struct inode *inode, struct file *file)
{
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
struct file_operations fops = {
    .open = pdrv_open,
    .read = pdrv_read,
    .write = pdrv_write,
    .unlocked_ioctl = pdrv_ioctl,
    .release = pdrv_close,
};
int pdrv_probe(struct platform_device *pdev)
{
    int ret, i = 0;
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    //1.获取结点
    node = of_find_node_by_path("/myplatform");
    if (node == NULL)
    {
        printk("get device node error\n");
        ret = -EAGAIN;
        goto ERR1;
    }
    // 2.获取desc(gpiod)
    for (i = 0; i < 3; i++)
    {
        desc[i] = gpiod_get_from_of_node(node, gpioname[i], 0, GPIOD_OUT_LOW, NULL);
        if (IS_ERR(desc[i]))
        {
            printk("get fan error\n");
            ret = PTR_ERR(desc[i]);
            goto ERR1;
        }
    }
    // 3.获取软中断号、注册中断
    irqno = platform_get_irq(pdev, 0);
    if (irqno < 0)
    {
        printk("get irq resource error\n");
        return irqno;
    }
    ret = request_irq(irqno, key_irq, IRQF_TRIGGER_FALLING, CNAME, NULL);
    if (ret)
    {
        printk("request irq error\n");
        goto ERR2;
    }
    major = register_chrdev(0, CNAME, &fops);
    if (major <= 0)
    {
        printk("register chrdev error\n");
        ret = -EAGAIN;
        goto ERR3;
    }
    //注册cls dev
    cls = class_create(THIS_MODULE, CNAME);
    if (IS_ERR(cls))
    {
        printk("class create error\n");
        ret = PTR_ERR(cls);
        goto ERR4;
    }

    dev = device_create(cls, &pdev->dev, MKDEV(major, 0), NULL, CNAME);
    if (IS_ERR(dev))
    {
        printk("device create error\n");
        ret = PTR_ERR(dev);
        goto ERR5;
    }
    // 5.初始化等待队列头
    init_waitqueue_head(&wq);

    return 0;
ERR5:
    class_destroy(cls);
ERR4:
    unregister_chrdev(major, CNAME);
ERR3:
    free_irq(irqno, NULL);
ERR2:
    for (i = 0; i < 3; i++)
    {
        gpiod_put(desc[i]);
    }
ERR1:
    return ret;
}

int pdrv_remove(struct platform_device *pdev)
{
    int i = 0;
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, CNAME);
    free_irq(irqno, NULL);

    for (i = 0; i < 3; i++)
    {
        gpiod_set_value(desc[i], 0);
        gpiod_put(desc[i]);
    }

    return 0;
}

struct of_device_id oftable[] = {
    {
        .compatible = "hqyj,myplatform",
    },
    {/*end*/}};
MODULE_DEVICE_TABLE(of, oftable);
struct platform_driver pdrv = {
    .probe = pdrv_probe,
    .remove = pdrv_remove,
    .driver = {
        .name = "ppp",
        .of_match_table = oftable,
    },
};

module_platform_driver(pdrv);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("P");