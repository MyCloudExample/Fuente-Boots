#include <linux/module.h>
#include <linux/init.h>
#include <linux/serdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h> /* Necesario para of_device_id */
#include <linux/of.h>               /* Necesario para OF functions */

#define DEVICE_NAME "egb"
#define FIFO_SIZE 4096 /* Aumentado para mayor seguridad en ráfagas de datos */

static struct serdev_device *egb_serdev;
static struct kfifo rx_fifo;
static spinlock_t fifo_lock;
static wait_queue_head_t rx_wait;

static dev_t dev_num;
static struct class *egb_class;
static struct cdev egb_cdev;

/* Callback de recepción */
static size_t egb_receive_buf(struct serdev_device *serdev, const unsigned char *buf, size_t size)
{
    unsigned long flags;
    size_t copied;

    spin_lock_irqsave(&fifo_lock, flags);
    copied = kfifo_in(&rx_fifo, buf, size);
    spin_unlock_irqrestore(&fifo_lock, flags);

    if (copied > 0) {
        wake_up_interruptible(&rx_wait);
    }
    return size;
}

static const struct serdev_device_ops egb_ops = {
    .receive_buf = egb_receive_buf,
};

/* READ: Basado en kfifo para evitar errores de memoria */
static ssize_t egb_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
    int ret;
    unsigned int copied;

    if (kfifo_is_empty(&rx_fifo)) {
        if (file->f_flags & O_NONBLOCK) return -EAGAIN;
        /* Esperar hasta que lleguen datos o se reciba una señal */
        ret = wait_event_interruptible(rx_wait, !kfifo_is_empty(&rx_fifo));
        if (ret) return ret;
    }

    /* kfifo_to_user es la forma más segura de evitar el "Bad Address" */
    ret = kfifo_to_user(&rx_fifo, buf, len, &copied);
    
    return ret ? ret : copied;
}

static ssize_t egb_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
    char *kbuf;
    kbuf = kmalloc(len, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buf, len)) {
        kfree(kbuf);
        return -EFAULT;
    }

    serdev_device_write_buf(egb_serdev, kbuf, len);
    kfree(kbuf);
    return len;
}

static int egb_open(struct inode *inode, struct file *file) 
{
    /* nonseekable_open marca el archivo como un stream */
    return nonseekable_open(inode, file);
}

static const struct file_operations egb_fops = {
    .owner = THIS_MODULE,
    .read = egb_read,
    .write = egb_write,
    .open = egb_open,
    .llseek = noop_llseek, /* Corregido para Kernel 6.12 */
};

static int egb_probe(struct serdev_device *serdev)
{
    int ret;
    egb_serdev = serdev;
    spin_lock_init(&fifo_lock);
    init_waitqueue_head(&rx_wait);
    
    if (kfifo_alloc(&rx_fifo, FIFO_SIZE, GFP_KERNEL)) 
        return -ENOMEM;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) goto err_fifo;

    egb_class = class_create(DEVICE_NAME "_class");
    if (IS_ERR(egb_class)) goto err_region;

    device_create(egb_class, NULL, dev_num, NULL, DEVICE_NAME);
    
    cdev_init(&egb_cdev, &egb_fops);
    ret = cdev_add(&egb_cdev, dev_num, 1);
    if (ret < 0) goto err_device;

    serdev_device_set_client_ops(serdev, &egb_ops);
    ret = serdev_device_open(serdev);
    if (ret) goto err_cdev;

    serdev_device_set_baudrate(serdev, 115200);
    serdev_device_set_flow_control(serdev, false);

    pr_info("EGB UART: Driver cargado en Kernel 6.12\n");
    return 0;

err_cdev:
    cdev_del(&egb_cdev);
err_device:
    device_destroy(egb_class, dev_num);
    class_destroy(egb_class);
err_region:
    unregister_chrdev_region(dev_num, 1);
err_fifo:
    kfifo_free(&rx_fifo);
    return ret;
}

static void egb_remove(struct serdev_device *serdev)
{
    serdev_device_close(serdev);
    cdev_del(&egb_cdev);
    device_destroy(egb_class, dev_num);
    class_destroy(egb_class);
    unregister_chrdev_region(dev_num, 1);
    kfifo_free(&rx_fifo);
}

static const struct of_device_id egb_ids[] = { 
    { .compatible = "frankie,egb-uart" }, 
    { } 
};
MODULE_DEVICE_TABLE(of, egb_ids);

static struct serdev_device_driver egb_driver = {
    .probe = egb_probe,
    .remove = egb_remove,
    .driver = { 
        .name = "egb_uart", 
        .of_match_table = egb_ids,
    },
};

module_serdev_device_driver(egb_driver);

MODULE_AUTHOR("Wilson");
MODULE_DESCRIPTION("UART Bridge for RPi to Pico with KFIFO");
MODULE_LICENSE("GPL");
