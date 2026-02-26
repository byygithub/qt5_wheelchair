#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <errno.h>

/**
 * @brief 初始化 I2C 主设备
 * @param i2c_path: I2C 设备节点路径（如 "/dev/i2c-1"）
 * @param slave_addr: 从设备 7 位地址（如 0x48）
 * @return 成功返回文件描述符，失败返回 -1
 */
int i2c_init(const char *i2c_path, unsigned char slave_addr);

/**
 * @brief 发送单字节数据到 I2C 从设备
 * @param fd: I2C 设备文件描述符
 * @param data: 要发送的单字节数据
 * @return 成功返回 0，失败返回 -1
 */
int i2c_send_byte(int fd, unsigned char data);

/**
 * @brief 发送多字节数据到 I2C 从设备
 * @param fd: I2C 设备文件描述符
 * @param buf: 待发送数据缓冲区
 * @param len: 发送数据长度
 * @return 成功返回 0，失败返回 -1
 */
int i2c_send_bytes(int fd, const unsigned char *buf, int len);

/**
 * @brief 关闭 I2C 设备
 * @param fd: I2C 设备文件描述符
 */
void i2c_close(int fd);

#endif // I2C_MASTER_H
