#include "uart_master.h"

// 初始化UART（适配i.MX6ULL /dev/ttymxc5，波特率115200）
int uart_init(const char *uart_path) {
    // 打开串口（仅写模式，满足纯发送需求）
    int fd = open(uart_path, O_WRONLY | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        fprintf(stderr, "UART打开失败: %s\n", strerror(errno));
        return -1;
    }

    // 配置串口参数（115200 8N1，无流控）
    struct termios cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.c_cflag = B115200 | CS8 | CLOCAL | CREAD; // 波特率+8位数据+本地模式
    cfg.c_iflag = IGNPAR;                         // 忽略奇偶错误
    cfg.c_oflag = 0;
    cfg.c_lflag = 0;
    cfg.c_cc[VTIME] = 0;
    cfg.c_cc[VMIN] = 1;

    // 应用配置
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &cfg) < 0) {
        fprintf(stderr, "UART配置失败: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

// 发送单个字符（核心接口，专门适配你的F/B/L/R/S发送需求）
int uart_send_char(int fd, char c) {
    if (fd < 0) {
        fprintf(stderr, "UART句柄无效\n");
        return -1;
    }

    // 发送单个字符
    ssize_t len = write(fd, &c, 1);
    if (len != 1) {
        fprintf(stderr, "发送字符失败: %c, 错误: %s\n", c, strerror(errno));
        return -1;
    }
    usleep(1000); // 适配外设响应延时
    return 0;
}

// 保留多字节发送接口（可选，若用不到可删除）
int uart_send_bytes(int fd, const unsigned char *buf, int len) {
    if (fd < 0 || buf == NULL || len <= 0) return -1;
    ssize_t len_write = write(fd, buf, len);
    if (len_write != len) {
        fprintf(stderr, "发送多字节失败: %s\n", strerror(errno));
        return -1;
    }
    usleep(1000);
    return 0;
}

// 关闭UART
void uart_close(int fd) {
    if (fd >= 0) close(fd);
}
