#include <common.h>
#include <delay.h>
#include <gpio.h>
#include <configs/adp-ag102.h>

/* 片选使能（低电平有效） */
static void spi_cs_enable(void)
{
    gpio_set_value(SPI_CS_GPIO, 0);
    udelay(1);  // 满足时序时序要求的延迟
}

/* 片选禁用（高电平无效） */
static void spi_cs_disable(void)
{
    gpio_set_value(SPI_CS_GPIO, 1);
    udelay(1);
}

/* 发送一个字节（高位先出） */
void spi_send_send_byte(u8 data)
{
    int i;
    for (i = 7; i >= 0; i--) {
        gpio_set_value(SPI_SCK_GPIO, 0);          // 拉低时钟，准备数据
        udelay(1);
        gpio_set_value(SPI_MOSI_GPIO, (data >> i) & 0x01);  // 输出当前bit
        udelay(1);
        gpio_set_value(SPI_SCK_GPIO, 1);          // 拉高时钟，锁存数据
        udelay(1);
    }
    gpio_set_value(SPI_SCK_GPIO, 0);  // 结束后拉低时钟
}

/* 接收一个字节（高位先出） */
u8 spi_recv_byte(void)
{
    int i;
    u8 data = 0;
    for (i = 7; i >= 0; i--) {
        gpio_set_value(SPI_SCK_GPIO, 0);          // 拉低时钟，准备读取
        udelay(1);
        gpio_set_value(SPI_SCK_GPIO, 1);          // 拉高时钟，读取数据
        udelay(1);
        if (gpio_get_value(SPI_MISO_GPIO)) {
            data |= (1 << i);  // 采样当前bit
        }
    }
    gpio_set_value(SPI_SCK_GPIO, 0);  // 结束后拉低时钟
    return data;
}

/* 发送命令（示例：命令前缀0x00，需根据设备手册修改） */
void spi_send_cmd(u8 cmd)
{
    spi_cs_enable();
    spi_send_byte(0x00);  // 命令模式标识
    spi_send_byte(cmd);
    spi_cs_disable();
}

/* 发送数据（示例：数据前缀0x40，需根据设备手册修改） */
void spi_send_data(u8 data)
{
    spi_cs_enable();
    spi_send_byte(0x40);  // 数据模式标识
    spi_send_byte(data);
    spi_cs_disable();
}

/* 读取数据（示例：读前缀0x80，需根据设备手册修改） */
u8 spi_read_data(void)
{
    u8 data;
    spi_cs_enable();
    spi_send_byte(0x80);  // 读模式标识
    data = spi_recv_byte();
    spi_cs_disable();
    return data;
}
