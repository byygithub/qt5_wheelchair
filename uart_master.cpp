#include "uart_master.h"

// 极速UART初始化：适配i.MX6ULL ttymxc5（移除不兼容的O_DIRECT）
int uart_init(const char *uart_path) {
    // 修复：移除O_DIRECT，保留其他加速标志（O_WRONLY | O_NOCTTY | O_NDELAY）
    int fd = open(uart_path, O_WRONLY | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        fprintf(stderr, "UART打开失败: %s\n", strerror(errno));
        return -1;
    }

    // 2. 极致低延迟配置（保留所有加速逻辑）
    struct termios cfg;
    memset(&cfg, 0, sizeof(cfg));
    // 115200波特率 + 8N1 + 无流控（硬件最优）
    cfg.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    cfg.c_iflag = 0; // 禁用输入处理（仅写模式，节省资源）
    cfg.c_oflag = 0; // 禁用输出转换（指令直接发走）
    cfg.c_lflag = 0; // 禁用本地模式（无回显/信号）
    // 无超时 + 最小字符数1（立即返回）
    cfg.c_cc[VTIME] = 0;
    cfg.c_cc[VMIN] = 1;

    // 3. 立即应用配置，清空所有缓冲
    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &cfg) < 0) {
        fprintf(stderr, "UART配置失败: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

// 按钮按下时极速发送：指令0延迟出现在TX引脚（保留加速逻辑）
int uart_send_char(int fd, char c) {
    if (fd < 0) {
        fprintf(stderr, "UART句柄无效\n");
        return -1;
    }

    // 核心加速：先发送（无缓冲），再排空队列
    ssize_t len = write(fd, &c, 1);
    tcdrain(fd); // 强制等待硬件发送完成，指令立即发走

    if (len != 1) {
        fprintf(stderr, "发送字符失败: %c, 错误: %s\n", c, strerror(errno));
        return -1;
    }

    return 0;
}

// 保留多字节接口（无需加速，你用不到）
int uart_send_bytes(int fd, const unsigned char *buf, int len) {
    if (fd < 0 || buf == NULL || len <= 0) return -1;
    ssize_t len_write = write(fd, buf, len);
    tcdrain(fd);
    if (len_write != len) {
        fprintf(stderr, "发送多字节失败: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// 快速关闭串口
void uart_close(int fd) {
    if (fd >= 0) {
        tcflush(fd, TCIOFLUSH); // 快速清空缓冲
        close(fd);
    }
}
