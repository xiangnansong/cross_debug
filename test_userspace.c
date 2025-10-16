/**
 * 用户空间测试程序 - 用于测试A520内核模块
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

// 共享内存结构定义（与内核模块保持一致）
struct shared_memory {
    char parameters[256];  // 参数区域
    char log_buffer[1024]; // 日志缓冲区
    int log_size;          // 日志大小
    int log_read;          // 日志读取位置
    int log_write;         // 日志写入位置
};

int main(int argc, char *argv[]) {
    int fd;
    struct shared_memory *shm_ptr;
    char input[256];
    
    // 打开设备文件
    fd = open("/dev/shm_device", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }
    
    // 映射共享内存
    shm_ptr = (struct shared_memory *)mmap(NULL, sizeof(struct shared_memory), 
                                          PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("Failed to mmap shared memory");
        close(fd);
        return -1;
    }
    
    printf("Shared memory mapped successfully\n");
    printf("Current parameters: %s\n", shm_ptr->parameters);
    
    // 循环读取用户输入并更新参数
    while (1) {
        printf("Enter new parameters (or 'quit' to exit): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // 移除换行符
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "quit") == 0) {
            break;
        }
        
        // 更新参数
        strncpy(shm_ptr->parameters, input, sizeof(shm_ptr->parameters) - 1);
        shm_ptr->parameters[sizeof(shm_ptr->parameters) - 1] = '\0';
        printf("Parameters updated to: %s\n", shm_ptr->parameters);
        
        // 显示当前日志
        printf("Current log buffer (read=%d, write=%d):\n", 
               shm_ptr->log_read, shm_ptr->log_write);
        
        // 简单显示日志内容
        int available = shm_ptr->log_write - shm_ptr->log_read;
        if (available < 0) {
            available += sizeof(shm_ptr->log_buffer);
        }
        
        if (available > 0) {
            printf("Available log data: %d bytes\n", available);
        }
    }
    
    // 取消映射
    munmap(shm_ptr, sizeof(struct shared_memory));
    close(fd);
    
    printf("Test program exited\n");
    return 0;
}