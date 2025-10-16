# 跨处理器通信系统

## 编译和安装

### 编译A520内核模块
```bash
make
```

### 安装内核模块
```bash
sudo make install
```

### 卸载内核模块
```bash
sudo make uninstall
```

### 编译测试程序
```bash
gcc -o test_userspace test_userspace.c
```

## 使用方法

1. 编译并安装A520内核模块
2. 通过sysfs接口配置参数：
   ```bash
   echo "your_parameters" > /sys/class/shm_class/shm_device/param
   ```
3. 在A55端运行裸机程序
4. 通过sysfs接口查看A55的日志：
   ```bash
   cat /sys/class/shm_class/shm_device/log
   ```

## sysfs接口

- `/sys/class/shm_class/shm_device/param` - 读写参数配置
- `/sys/class/shm_class/shm_device/log` - 读取A55日志

## 设备文件

- `/dev/shm_device` - 用于用户空间程序访问共享内存

## 共享内存结构

```c
struct shared_memory {
    char parameters[256];  // 参数区域
    char log_buffer[1024]; // 日志缓冲区
    int log_size;          // 日志大小
    int log_read;          // 日志读取位置
    int log_write;         // 日志写入位置
};
```
