#include "i2c_master.h"

int i2c_init(const char *i2c_path, unsigned char slave_addr) {
    // 打开 I2C 设备
    int fd = open(i2c_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "I2C open fail: %s\n", strerror(errno));
        return -1;
    }

    // 设置从设备地址
    if (ioctl(fd, I2C_SLAVE, slave_addr) < 0) {
        fprintf(stderr, "Set slave addr fail: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int i2c_send_byte(int fd, unsigned char data) {
    if (write(fd, &data, 1) != 1) {
        fprintf(stderr, "I2C send byte fail: %s\n", strerror(errno));
        return -1;
    }
    usleep(1000); // 适配从设备响应延时
    return 0;
}

int i2c_send_bytes(int fd, const unsigned char *buf, int len) {
    if (buf == NULL || len <= 0) {
        fprintf(stderr, "Invalid send params\n");
        return -1;
    }

    if (write(fd, buf, len) != len) {
        fprintf(stderr, "I2C send %d bytes fail: %s\n", len, strerror(errno));
        return -1;
    }
    usleep(1000);
    return 0;
}

void i2c_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}
