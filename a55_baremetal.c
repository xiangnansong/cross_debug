/**
 * A55裸机程序 - 读取共享内存参数并写入日志
 */

#include <stdint.h>
#include <stdio.h>

// 共享内存结构定义（与A520端保持一致）
struct shared_memory {
    char parameters[256];  // 参数区域
    char log_buffer[1024]; // 日志缓冲区
    int log_size;          // 日志大小
    int log_read;          // 日志读取位置
    int log_write;         // 日志写入位置
};

// 函数声明
void strcpy(char *dest, const char *src);
int strlen(const char *str);
void strcat(char *dest, const char *src);
void write_log(const char *msg);
void read_parameters(char *buffer, int buffer_size);

// 假设共享内存映射到固定地址
#define SHARED_MEMORY_ADDR 0x80000000
volatile struct shared_memory *shm = (struct shared_memory *)SHARED_MEMORY_ADDR;

// 简单的字符串复制函数
void strcpy(char *dest, const char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// 简单的字符串长度计算函数
int strlen(const char *str) {
    int len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// 简单的字符串连接函数
void strcat(char *dest, const char *src) {
    while (*dest) dest++;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// 向共享内存写入日志
void write_log(const char *msg) {
    int msg_len = strlen(msg);
    int i;
    
    // 检查是否有足够空间
    int available_space;
    if (shm->log_write >= shm->log_read) {
        available_space = sizeof(shm->log_buffer) - (shm->log_write - shm->log_read) - 1;
    } else {
        available_space = shm->log_read - shm->log_write - 1;
    }
    
    if (msg_len + 1 > available_space) {
        // 空间不足，丢弃部分旧日志
        // 这里简化处理，实际应用中可能需要更复杂的环形缓冲区管理
        return;
    }
    
    // 写入日志消息
    for (i = 0; i < msg_len; i++) {
        shm->log_buffer[shm->log_write] = msg[i];
        shm->log_write = (shm->log_write + 1) % sizeof(shm->log_buffer);
    }
    
    // 添加换行符
    shm->log_buffer[shm->log_write] = '\n';
    shm->log_write = (shm->log_write + 1) % sizeof(shm->log_buffer);
    
    // 更新日志大小
    shm->log_size = (shm->log_write - shm->log_read + sizeof(shm->log_buffer)) % sizeof(shm->log_buffer);
}

// 从共享内存读取参数
void read_parameters(char *buffer, int buffer_size) {
    int i;
    int len = 0;
    
    // 计算参数长度
    while (len < sizeof(shm->parameters) - 1 && shm->parameters[len] != '\0') {
        len++;
    }
    
    // 复制参数到缓冲区
    int copy_len = (len < buffer_size - 1) ? len : buffer_size - 1;
    for (i = 0; i < copy_len; i++) {
        buffer[i] = shm->parameters[i];
    }
    buffer[copy_len] = '\0';
}

// 主程序
int main() {
    char params[256];
    char log_msg[100];
    int counter = 0;
    
    // 初始化共享内存指针（在实际裸机环境中，这可能是固定的物理地址）
    // 这里我们假设它已经被正确映射
    
    // 记录启动日志
    write_log("A55 program started");
    
    // 主循环
    while (1) {
        // 读取参数
        read_parameters(params, sizeof(params));
        
        // 构造日志消息
        strcpy(log_msg, "Parameters: ");
        strcat(log_msg, params);
        write_log(log_msg);
        
        // 执行一些处理任务
        counter++;
        if (counter % 1000000 == 0) {
            // 记录周期性日志
            char counter_msg[50];
            strcpy(counter_msg, "Processing cycle: ");
            // 简化的数字转字符串
            int temp = counter / 1000000;
            int digits[10];
            int digit_count = 0;
            if (temp == 0) {
                digits[digit_count++] = 0;
            } else {
                while (temp > 0) {
                    digits[digit_count++] = temp % 10;
                    temp /= 10;
                }
            }
            
            int j;
            for (j = 0; j < digit_count; j++) {
                counter_msg[17 + j] = '0' + digits[digit_count - 1 - j];
            }
            counter_msg[17 + digit_count] = '\0';
            
            write_log(counter_msg);
        }
        
        // 模拟工作负载
        // 在实际应用中，这里可能是具体的业务逻辑
        
        // 简单的延时（在实际裸机环境中，这可能是定时器或其它同步机制）
        volatile int delay;
        for (delay = 0; delay < 1000; delay++);
    }
    
    return 0;
}