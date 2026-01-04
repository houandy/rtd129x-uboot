#include <common.h>
#include <delay.h>
#include <gpio.h>
#include <configs/adp-ag102.h>  // 板级GPIO定义
#include "../spi/spi_gpio_bitbang.h"  // SPI模拟通信函数

/********************* NT7534 硬件参数定义 *********************/
// 屏幕分辨率（128x64单色屏，8行×8列=64行）
#define NT7534_WIDTH    128
#define NT7534_HEIGHT   64
#define NT7534_PAGE_CNT (NT7534_HEIGHT / 8)  // 8个PAGE（每页8行像素）

// NT7534 指令集（参考官方数据手册）
#define NT7534_CMD_DISPLAY_OFF    0xAE
#define NT7534_CMD_DISPLAY_ON     0xAF
#define NT7534_CMD_SET_PAGE       0xB0  // 设置页地址（0~7）
#define NT7534_CMD_SET_COL_HIGH   0x10  // 设置列地址高4位
#define NT7534_CMD_SET_COL_LOW    0x00  // 设置列地址低4位
#define NT7534_CMD_SET_ADC_NORMAL 0xA0  // 列地址正序（0→127）
#define NT7534_CMD_SET_COM_NORMAL 0xC0  // 行扫描正序（0→63）
#define NT7534_CMD_SET_POWER      0x2F  // 电源控制（开启所有电源）
#define NT7534_CMD_SET_RESISTOR   0x27  // 内部电阻配置
#define NT7534_CMD_SET_VOLUME     0x81  // 对比度设置
#define NT7534_CMD_SET_VOLUME_VAL 0x10  // 对比度值（0~31可调）

/********************* 屏幕显存（128列×8页） *********************/
static u8 nt7534_fb[NT7534_PAGE_CNT][NT7534_WIDTH] = {0};  // 帧缓冲区

/********************* 基础硬件操作 *********************/
/**
 * @brief 向NT7534发送命令
 */
static void nt7534_send_cmd(u8 cmd)
{
    spi_cs_enable();          // 片选使能
    spi_send_byte(0x00);      // 命令模式（D/C=0）
    spi_send_byte(cmd);       // 发送指令
    spi_cs_disable();         // 片选禁用
    udelay(10);               // 指令执行延迟
}

/**
 * @brief 向NT7534发送数据（像素值）
 */
static void nt7534_send_data(u8 data)
{
    spi_cs_enable();          // 片选使能
    spi_send_byte(0x40);      // 数据模式（D/C=1）
    spi_send_byte(data);      // 发送像素数据
    spi_cs_disable();         // 片选禁用
    udelay(1);                // 短延迟
}

/**
 * @brief 硬件复位NT7534
 */
static void nt7534_hw_reset(void)
{
    gpio_set_value(SPI_RST_GPIO, 0);  // 拉低复位引脚
    mdelay(10);                       // 保持复位10ms
    gpio_set_value(SPI_RST_GPIO, 1);  // 释放复位
    mdelay(50);                       // 等待屏幕稳定
}

/**
 * @brief 初始化NT7534屏幕
 */
int nt7534_init(void)
{
    // 1. 初始化SPI GPIO
    if (spi_gpio_init() != 0) {
        printf("NT7534: SPI GPIO init failed!\n");
        return -1;
    }

    // 2. 硬件复位
    nt7534_hw_reset();

    // 3. 发送初始化指令序列
    nt7534_send_cmd(NT7534_CMD_DISPLAY_OFF);  // 关闭显示
    nt7534_send_cmd(NT7534_CMD_SET_ADC_NORMAL); // 列地址正序
    nt7534_send_cmd(NT7534_CMD_SET_COM_NORMAL);  // 行扫描正序
    nt7534_send_cmd(NT7534_CMD_SET_PAGE);        // 默认页0
    nt7534_send_cmd(NT7534_CMD_SET_COL_HIGH);    // 默认列高4位0
    nt7534_send_cmd(NT7534_CMD_SET_COL_LOW);     // 默认列低4位0
    nt7534_send_cmd(NT7534_CMD_SET_POWER);       // 开启电源
    nt7534_send_cmd(NT7534_CMD_SET_RESISTOR);    // 配置内部电阻
    nt7534_send_cmd(NT7534_CMD_SET_VOLUME);      // 设置对比度
    nt7534_send_cmd(NT7534_CMD_SET_VOLUME_VAL);  // 对比度值
    nt7534_send_cmd(NT7534_CMD_DISPLAY_ON);      // 开启显示

    // 4. 清屏（帧缓冲区+硬件屏）
    nt7534_clear();

    printf("NT7534: Init success!\n");
    return 0;
}

/********************* 帧缓冲区操作 *********************/
/**
 * @brief 清屏（帧缓冲区填充0，同步到硬件）
 */
void nt7534_clear(void)
{
    int page, col;

    // 1. 清空帧缓冲区
    memset(nt7534_fb, 0, sizeof(nt7534_fb));

    // 2. 同步到硬件屏幕
    for (page = 0; page < NT7534_PAGE_CNT; page++) {
        // 设置当前页地址
        nt7534_send_cmd(NT7534_CMD_SET_PAGE | page);
        // 设置列地址起始（0列）
        nt7534_send_cmd(NT7534_CMD_SET_COL_HIGH | 0x00);
        nt7534_send_cmd(NT7534_CMD_SET_COL_LOW | 0x00);

        // 发送当前页所有列数据
        spi_cs_enable();          // 批量发送：仅一次片选
        spi_send_byte(0x40);      // 数据模式
        for (col = 0; col < NT7534_WIDTH; col++) {
            spi_send_byte(0x00);  // 发送0（清屏）
        }
        spi_cs_disable();
    }
}

/**
 * @brief 设置单个像素点（帧缓冲区）
 * @param x 横坐标（0~127）
 * @param y 纵坐标（0~63）
 * @param val 像素值（0=灭，1=亮）
 */
void nt7534_set_pixel(int x, int y, u8 val)
{
    // 边界检查
    if (x < 0 || x >= NT7534_WIDTH || y < 0 || y >= NT7534_HEIGHT)
        return;

    int page = y / 8;    // 计算所在页（0~7）
    int bit = y % 8;     // 计算页内位偏移（0~7）

    if (val) {
        nt7534_fb[page][x] |= (1 << bit);  // 置1（点亮）
    } else {
        nt7534_fb[page][x] &= ~(1 << bit); // 置0（熄灭）
    }
}

/**
 * @brief 将帧缓冲区同步到硬件屏幕
 */
void nt7534_refresh(void)
{
    int page, col;

    for (page = 0; page < NT7534_PAGE_CNT; page++) {
        // 设置当前页地址
        nt7534_send_cmd(NT7534_CMD_SET_PAGE | page);
        // 设置列地址起始（0列）
        nt7534_send_cmd(NT7534_CMD_SET_COL_HIGH | 0x00);
        nt7534_send_cmd(NT7534_CMD_SET_COL_LOW | 0x00);

        // 批量发送当前页数据（减少片选切换，提升效率）
        spi_cs_enable();
        spi_send_byte(0x40);  // 数据模式
        for (col = 0; col < NT7534_WIDTH; col++) {
            spi_send_byte(nt7534_fb[page][col]);
        }
        spi_cs_disable();
    }
}

/********************* 字符/字符串显示 *********************/
// 8x8 ASCII字库（仅示例，可替换为完整字库）
static const u8 font_8x8[][8] = {
    [0x20] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 空格
    [0x30] = {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0
    [0x31] = {0x00,0x7C,0x7E,0x06,0x00,0x00,0x7E,0x00}, // 1
    [0x32] = {0x76,0x67,0x63,0x6B,0x73,0x73,0x6E,0x00}, // 2
    // 可扩展更多字符...
};

/**
 * @brief 绘制8x8字符到帧缓冲区
 * @param x 字符左上角x坐标
 * @param y 字符左上角y坐标
 * @param c 要显示的ASCII字符
 */
void nt7534_draw_char(int x, int y, char c)
{
    // 边界检查
    if (x + 8 > NT7534_WIDTH || y + 8 > NT7534_HEIGHT || c < 0x20 || c > 0x7E)
        return;

    const u8 *font = font_8x8[c];  // 获取字符字模
    int px, py;

    // 逐像素绘制字符
    for (py = 0; py < 8; py++) {
        for (px = 0; px < 8; px++) {
            // 判断当前像素是否点亮
            u8 val = (font[py] >> (7 - px)) & 0x01;
            nt7534_set_pixel(x + px, y + py, val);
        }
    }
}

/**
 * @brief 显示字符串
 * @param x 字符串左上角x坐标
 * @param y 字符串左上角y坐标
 * @param str 要显示的字符串
 */
void nt7534_draw_string(int x, int y, const char *str)
{
    int x_off = 0;
    while (*str) {
        nt7534_draw_char(x + x_off, y, *str);
        x_off += 8;  // 字符宽度8像素，间隔0
        str++;
    }
    nt7534_refresh();  // 同步到屏幕
}

/********************* U-Boot命令注册（测试用） *********************/
#if defined(CONFIG_CMD_NT7534)
static int do_nt7534_test(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("nt7534 init   - Init NT7534 screen\n");
        printf("nt7534 clear  - Clear screen\n");
        printf("nt7534 show   - Show test string\n");
        return -1;
    }

    if (!strcmp(argv[1], "init")) {
        return nt7534_init();
    } else if (!strcmp(argv[1], "clear")) {
        nt7534_clear();
        printf("NT7534: Screen cleared\n");
    } else if (!strcmp(argv[1], "show")) {
        nt7534_draw_string(0, 0, "U-Boot NT7534 Test");
        nt7534_draw_string(0, 16, "128x64 SPI LCD");
        printf("NT7534: Show test string\n");
    }

    return 0;
}

U_BOOT_CMD(
    nt7534, 2, 1, do_nt7534_test,
    "NT7534 LCD test commands",
    "nt7534 <init|clear|show> - NT7534 LCD operations"
);
#endif
