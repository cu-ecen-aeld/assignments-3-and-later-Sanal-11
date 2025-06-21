/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
int aesd_init_module(void);

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    /* Device information*/
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    if (!filp->private_data) {
        PDEBUG("Error: filp->private_data is NULL");
        return -EINVAL;
    }

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
     
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    ssize_t bytes_to_copy = 0;
    struct aesd_dev *device = filp->private_data;
    struct aesd_buffer_entry *entry = NULL;
    size_t entry_offset_byte = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    if (!device || !buf || !f_pos) {
        PDEBUG("Error: NULL pointer detected in aesd_read");
        return -EINVAL;
    }

    if (mutex_lock_interruptible(&device->lock)) {
        PDEBUG("Error: Could not lock buffer_mutex");
        return -ERESTARTSYS;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&device->buffer, *f_pos, &entry_offset_byte);
    if (!entry) {
        PDEBUG("End of buffer reached, returning 0 bytes");
        mutex_unlock(&device->lock);
        return 0;
    }

    bytes_to_copy = min(count, entry->size - entry_offset_byte);
    if (copy_to_user(buf, entry->buffptr + entry_offset_byte, bytes_to_copy)) {
        PDEBUG("Error: copy_to_user failed");
        retval = -EFAULT;
    } else {
        *f_pos += bytes_to_copy;
        retval = bytes_to_copy;
        PDEBUG("Read %zd bytes successfully", retval);
    }

    mutex_unlock(&device->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *device = filp->private_data;
    struct aesd_buffer_entry entry;
    ssize_t retval = count;
    char *temp_buf;
    size_t i;
    bool is_newline = false;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (!device || !buf || !f_pos) {
        PDEBUG("Error: NULL pointer detected in aesd_write");
        return -EINVAL;
    }

    temp_buf = kmalloc(count, GFP_KERNEL);
    if (!temp_buf) {
        PDEBUG("Error: kmalloc failed");
        return -ENOMEM;
    }

    if (copy_from_user(temp_buf, buf, count)) {
        PDEBUG("Error: copy_from_user failed");
        kfree(temp_buf);
        return -EFAULT;
    }

    mutex_lock(&device->lock);

    // Check for newline character in input data
    for (i = 0; i < count; i++) {
        if (temp_buf[i] == '\n') {
            is_newline = true;
            break;
        }
    }

    // Append data to temporary buffer before writing to circular buffer
    device->entry.buffptr = krealloc(device->entry.buffptr, device->entry.size + count, GFP_KERNEL);
    if (!device->entry.buffptr) {
        PDEBUG("Error: kernel realloc failed");
        kfree(temp_buf);
        mutex_unlock(&device->lock);
        return -ENOMEM;
    }

    memcpy((char *)device->entry.buffptr + device->entry.size, temp_buf, count);
    device->entry.size += count;

    if (is_newline) {
        aesd_circular_buffer_add_entry(&device->buffer, &device->entry);

        // const char *old_buffer = aesd_circular_buffer_add_entry(&device->buffer, &device->entry);
        // if (old_buffer) {
        //     kfree(old_buffer);  // Free memory from oldest entry only when buffer is full
        //     PDEBUG("Old buffer freed");
        // }

        device->entry.buffptr = NULL;
        device->entry.size = 0;
    }

    kfree(temp_buf);
    mutex_unlock(&device->lock);
    PDEBUG("Write successful, wrote %zd bytes", retval);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

static void free_circular_buffer(void)
{
    uint8_t i;
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        kfree(aesd_device.buffer.entry[i].buffptr);
        aesd_device.buffer.entry[i].buffptr = NULL;
        aesd_device.buffer.entry[i].size = 0;
    }
    aesd_device.buffer.in_offs = 0;
    aesd_device.buffer.out_offs = 0;
    aesd_device.buffer.full = false;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    printk(KERN_INFO "aesdchar: loaded module\n");
    return result;

}

static void __exit aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    mutex_lock(&aesd_device.lock);
    free_circular_buffer();
    kfree(aesd_device.partial_write_buffer);
    mutex_unlock(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
    printk(KERN_INFO "aesdchar: unloaded module\n");
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
