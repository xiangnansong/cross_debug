#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cross Processor Communication");
MODULE_DESCRIPTION("A520 module for parameter configuration and log handling");
MODULE_VERSION("0.1");

// 共享内存结构
struct shared_memory {
    char parameters[256];  // 参数区域
    char log_buffer[1024]; // 日志缓冲区
    int log_size;          // 日志大小
    int log_read;          // 日志读取位置
    int log_write;         // 日志写入位置
};

// 全局变量
static struct shared_memory *shm_ptr = NULL;
static struct class *shm_class = NULL;
static struct device *shm_device = NULL;
static dev_t shm_dev_num = 0;
static struct cdev shm_cdev;

// 参数变量
static char param_value[256] = "default_parameters";
static DEFINE_MUTEX(param_mutex);

// Sysfs接口 - 参数属性
static ssize_t param_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t ret;
    
    mutex_lock(&param_mutex);
    ret = sprintf(buf, "%s\n", param_value);
    mutex_unlock(&param_mutex);
    
    return ret;
}

static ssize_t param_store(struct device *dev, struct device_attribute *attr,
                          const char *buf, size_t count)
{
    mutex_lock(&param_mutex);
    
    if (count >= sizeof(param_value)) {
        count = sizeof(param_value) - 1;
    }
    
    strncpy(param_value, buf, count);
    if (param_value[count-1] == '\n') {
        param_value[count-1] = '\0';
        count--;
    } else {
        param_value[count] = '\0';
    }
    
    // 更新共享内存中的参数
    if (shm_ptr) {
        strncpy(shm_ptr->parameters, param_value, sizeof(shm_ptr->parameters) - 1);
        shm_ptr->parameters[sizeof(shm_ptr->parameters) - 1] = '\0';
    }
    
    mutex_unlock(&param_mutex);
    
    printk(KERN_INFO "Parameter updated: %s\n", param_value);
    return count;
}

static DEVICE_ATTR_RW(param);

// Sysfs接口 - 日志属性
static ssize_t log_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;
    int available;
    
    mutex_lock(&param_mutex);
    
    if (shm_ptr) {
        // 计算可读取的日志数据量
        available = shm_ptr->log_write - shm_ptr->log_read;
        if (available < 0) {
            available += sizeof(shm_ptr->log_buffer);
        }
        
        if (available > 0) {
            // 读取日志数据
            if (shm_ptr->log_read + available <= sizeof(shm_ptr->log_buffer)) {
                // 数据是连续的
                ret = snprintf(buf, available + 1, "%.*s", available, 
                              shm_ptr->log_buffer + shm_ptr->log_read);
            } else {
                // 数据环绕缓冲区
                int first_part = sizeof(shm_ptr->log_buffer) - shm_ptr->log_read;
                ret = snprintf(buf, first_part + 1, "%.*s", first_part, 
                              shm_ptr->log_buffer + shm_ptr->log_read);
                ret += snprintf(buf + first_part, available - first_part + 1, "%.*s", 
                               available - first_part, shm_ptr->log_buffer);
            }
            
            // 更新读取位置
            shm_ptr->log_read = (shm_ptr->log_read + available) % sizeof(shm_ptr->log_buffer);
        }
    }
    
    mutex_unlock(&param_mutex);
    
    return ret;
}

static DEVICE_ATTR_RO(log);

// 设备文件操作
static int shm_dev_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int shm_dev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long shm_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return 0;
}

// MMAP操作 - 允许用户空间访问共享内存
static int shm_dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    unsigned long size = vma->vm_end - vma->vm_start;
    
    if (size > sizeof(struct shared_memory)) {
        return -EINVAL;
    }
    
    pfn = virt_to_phys((void *)shm_ptr) >> PAGE_SHIFT;
    
    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
        return -EAGAIN;
    }
    
    return 0;
}

// 文件操作结构体
static const struct file_operations shm_dev_fops = {
    .owner = THIS_MODULE,
    .open = shm_dev_open,
    .release = shm_dev_release,
    .unlocked_ioctl = shm_dev_ioctl,
    .mmap = shm_dev_mmap,
};

// 模块初始化
static int __init a520_module_init(void)
{
    int ret;
    
    // 分配共享内存
    shm_ptr = kmalloc(sizeof(struct shared_memory), GFP_KERNEL);
    if (!shm_ptr) {
        printk(KERN_ERR "Failed to allocate shared memory\n");
        return -ENOMEM;
    }
    
    // 初始化共享内存
    memset(shm_ptr, 0, sizeof(struct shared_memory));
    strncpy(shm_ptr->parameters, param_value, sizeof(shm_ptr->parameters) - 1);
    shm_ptr->parameters[sizeof(shm_ptr->parameters) - 1] = '\0';
    
    // 分配设备号
    ret = alloc_chrdev_region(&shm_dev_num, 0, 1, "shm_device");
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate char device number\n");
        goto err_free_shm;
    }
    
    // 初始化字符设备
    cdev_init(&shm_cdev, &shm_dev_fops);
    shm_cdev.owner = THIS_MODULE;
    
    // 添加字符设备
    ret = cdev_add(&shm_cdev, shm_dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "Failed to add character device\n");
        goto err_unregister_chrdev;
    }
    
    // 创建设备类
    shm_class = class_create(THIS_MODULE, "shm_class");
    if (IS_ERR(shm_class)) {
        printk(KERN_ERR "Failed to create device class\n");
        ret = PTR_ERR(shm_class);
        goto err_cdev_del;
    }
    
    // 创建设备
    shm_device = device_create(shm_class, NULL, shm_dev_num, NULL, "shm_device");
    if (IS_ERR(shm_device)) {
        printk(KERN_ERR "Failed to create device\n");
        ret = PTR_ERR(shm_device);
        goto err_class_destroy;
    }
    
    // 创建sysfs属性
    ret = device_create_file(shm_device, &dev_attr_param);
    if (ret) {
        printk(KERN_ERR "Failed to create param sysfs attribute\n");
        goto err_device_destroy;
    }
    
    ret = device_create_file(shm_device, &dev_attr_log);
    if (ret) {
        printk(KERN_ERR "Failed to create log sysfs attribute\n");
        goto err_remove_param_attr;
    }
    
    printk(KERN_INFO "A520 module loaded successfully\n");
    printk(KERN_INFO "Shared memory at %p\n", shm_ptr);
    printk(KERN_INFO "Parameters sysfs: /sys/class/shm_class/shm_device/param\n");
    printk(KERN_INFO "Log sysfs: /sys/class/shm_class/shm_device/log\n");
    printk(KERN_INFO "Device file: /dev/shm_device\n");
    
    return 0;
    
err_remove_param_attr:
    device_remove_file(shm_device, &dev_attr_param);
err_device_destroy:
    device_destroy(shm_class, shm_dev_num);
err_class_destroy:
    class_destroy(shm_class);
err_cdev_del:
    cdev_del(&shm_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(shm_dev_num, 1);
err_free_shm:
    kfree(shm_ptr);
    return ret;
}

// 模块退出
static void __exit a520_module_exit(void)
{
    // 移除sysfs属性
    device_remove_file(shm_device, &dev_attr_log);
    device_remove_file(shm_device, &dev_attr_param);
    
    // 清理设备文件
    device_destroy(shm_class, shm_dev_num);
    class_destroy(shm_class);
    cdev_del(&shm_cdev);
    unregister_chrdev_region(shm_dev_num, 1);
    
    // 释放共享内存
    if (shm_ptr) {
        kfree(shm_ptr);
    }
    
    printk(KERN_INFO "A520 module unloaded\n");
}

module_init(a520_module_init);
module_exit(a520_module_exit);