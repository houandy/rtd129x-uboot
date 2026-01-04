#include <common.h>
#include <lcd.h>
#include <gpio.h>
#include <delay.h>
#include <linux/types.h>

// ==================== 手动指定SPI GPIO（根据BPI-W2实际引脚修改） ====================
#define NT7534_SCK_GPIO       GPIOB(0)  // 手动指定SCK引脚
#define NT7534_MOSI_GPIO      GPIOB(1)  // 手动指定MOSI引脚
#define NT7534_CS_GPIO        GPIOB(2)  // 手动指定CS引脚
#define NT7534_DC_GPIO        GPIOB(3)  // 手动指定DC引脚
#define NT7534_RST_GPIO       GPIOB(4)  // 手动指定RST引脚
// ==================================================================================

// NT7534 固定参数（无需修改）
#define NT7534_WIDTH          128     
#define NT7534_HEIGHT         64      
#define NT7534_PAGE_CNT       (NT7534_HEIGHT / 8)

// NT7534 指令（不变）
#define NT7534_CMD_DISPLAY_OFF    0xAE
#define NT7534_CMD_DISPLAY_ON     0xAF
#define NT7534_CMD_SET_PAGE       0xB0
#define NT7534_CMD_SET_COL_LOW    0x00
#define NT7534_CMD_SET_COL_HIGH   0x10
#define NT7534_CMD_SET_ADC_DIR    0xA0
#define NT7534_CMD_SET_COM_DIR    0xC0
#define NT7534_CMD_SET_CONTRAST   0x81
#define NT7534_CMD_SOFT_RESET     0xE2
#define NT7534_CMD_SET_DISP_START 0x40

// 显示缓存（不变）
static u8 nt7534_framebuf[NT7534_PAGE_CNT * NT7534_WIDTH] __aligned(4);

// 8×16 ASCII点阵库（不变）
static const u8 nt7534_font_8x16[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 空格
    0x00,0x00,0x7C,0x12,0x11,0x12,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0
    0x00,0x00,0x10,0x18,0x14,0x12,0xFF,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 1
    0x00,0x00,0x7C,0x02,0x01,0x02,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 2
    0x00,0x00,0x7C,0x02,0x31,0x02,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 3
    0x00,0x00,0x04,0x0C,0x14,0x24,0xFF,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 4
    0x00,0x00,0x7C,0x80,0x7F,0x01,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 5
    0x20,0x54,0x54,0x54,0x78,0x54,0x54,0x54,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // A
    0x7F,0x48,0x44,0x44,0x78,0x44,0x44,0x48,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // B
};

/**
 * @brief GPIO模拟SPI - 初始化所有SPI相关GPIO为输出模式
 */
static void nt7534_spi_gpio_init(void)
{
    // 申请GPIO资源
    gpio_request(NT7534_SCK_GPIO, "nt7534_sck");
    gpio_request(NT7534_MOSI_GPIO, "nt7534_mosi");
    gpio_request(NT7534_CS_GPIO, "nt7534_cs");
    gpio_request(NT7534_DC_GPIO, "nt7534_dc");
    gpio_request(NT7534_RST_GPIO, "nt7534_rst");

    // 配置为输出模式，初始电平（CS拉高=未选中，SCK拉低=空闲）
    gpio_direction_output(NT7534_SCK_GPIO, 0);
    gpio_direction_output(NT7534_MOSI_GPIO, 0);
    gpio_direction_output(NT7534_CS_GPIO, 1);  // CS默认拉高
    gpio_direction_output(NT7534_DC_GPIO, 0);
    gpio_direction_output(NT7534_RST_GPIO, 1);
}

/**
 * @brief GPIO模拟SPI - 发送单个字节（CPOL=0, CPHA=0，NT7534标准时序）
 * @param val: 要发送的字节（高位先出）
 */
static void nt7534_spi_send_byte(u8 val)
{
    u8 i;

    // 拉低CS，选中NT7534
    gpio_set_value(NT7534_CS_GPIO, 0);
    udelay(1); // 短延时保证时序

    for (i = 0; i < 8; i++) {
        // 1. SCK拉低，准备数据
        gpio_set_value(NT7534_SCK_GPIO, 0);
        udelay(1);

        // 2. 输出1位数据（高位先送）
        if (val & 0x80) {
            gpio_set_value(NT7534_MOSI_GPIO, 1);
        } else {
            gpio_set_value(NT7534_MOSI_GPIO, 0);
        }
        udelay(1);

        // 3. SCK拉高，让NT7534采样数据
        gpio_set_value(NT7534_SCK_GPIO, 1);
        udelay(1);

        // 4. 移位，处理下一位
        val <<= 1;
    }

    // 4. SCK拉低，释放CS
    gpio_set_value(NT7534_SCK_GPIO, 0);
    gpio_set_value(NT7534_CS_GPIO, 1);
    udelay(1);
}

/**
 * @brief 向NT7534发送命令/数据（封装SPI发送）
 * @param is_data: 1=数据，0=命令
 * @param val: 待发送字节
 */
static void nt7534_send_byte(u8 is_data, u8 val)
{
    // 设置DC引脚：1=数据，0=命令
    gpio_set_value(NT7534_DC_GPIO, is_data);
    udelay(1);

    // 模拟SPI发送字节
    nt7534_spi_send_byte(val);
}

/**
 * @brief NT7534硬件复位
 */
static void nt7534_hw_reset(void)
{
    gpio_set_value(NT7534_RST_GPIO, 0);
    mdelay(10);
    gpio_set_value(NT7534_RST_GPIO, 1);
    mdelay(10);
}

/**
 * @brief 刷新帧缓存到屏幕
 */
void nt7534_refresh(void)
{
    u8 page, col;

    for (page = 0; page < NT7534_PAGE_CNT; page++)
    {
        // 设置页地址
        nt7534_send_byte(0, NT7534_CMD_SET_PAGE | page);
        // 设置列地址低4位
        nt7534_send_byte(0, NT7534_CMD_SET_COL_LOW | 0);
        // 设置列地址高4位
        nt7534_send_byte(0, NT7534_CMD_SET_COL_HIGH | 0);

        // 发送当前页数据
        for (col = 0; col < NT7534_WIDTH; col++)
        {
            nt7534_send_byte(1, nt7534_framebuf[page * NT7534_WIDTH + col]);
        }
    }
}

/**
 * @brief 清屏
 */
void nt7534_clear(void)
{
    memset(nt7534_framebuf, 0, sizeof(nt7534_framebuf));
    nt7534_refresh();
}

/**
 * @brief 绘制单个字符（8×16）
 */
void nt7534_draw_char(u8 x, u8 y, char c)
{
    if (x >= NT7534_WIDTH || y >= NT7534_PAGE_CNT || c > 'Z' || c < ' ')
        return;

    u8 char_idx = (c - ' ') * 16;
    u8 *fb_ptr = &nt7534_framebuf[y * NT7534_WIDTH + x];

    // 上8行
    for (u8 i = 0; i < 8; i++)
    {
        fb_ptr[i] = nt7534_font_8x16[char_idx + i];
    }

    // 下8行（若未超页）
    if (y + 1 < NT7534_PAGE_CNT)
    {
        fb_ptr = &nt7534_framebuf[(y + 1) * NT7534_WIDTH + x];
        for (u8 i = 8; i < 16; i++)
        {
            fb_ptr[i - 8] = nt7534_font_8x16[char_idx + i];
        }
    }
}

/**
 * @brief 显示字符串
 */
void nt7534_draw_string(u8 x, u8 y, const char *str)
{
    while (*str)
    {
        nt7534_draw_char(x, y, *str);
        x += 8;
        if (x >= NT7534_WIDTH)
            break;
        str++;
    }
    nt7534_refresh();
}

/**
 * @brief LCD初始化（适配U-Boot框架）
 */
int lcd_init(void)
{
    // 1. 初始化SPI GPIO
    nt7534_spi_gpio_init();

    // 2. 硬件复位
    nt7534_hw_reset();

    // 3. 发送初始化指令
    nt7534_send_byte(0, NT7534_CMD_SOFT_RESET);
    nt7534_send_byte(0, NT7534_CMD_SET_ADC_DIR);
    nt7534_send_byte(0, NT7534_CMD_SET_COM_DIR);
    nt7534_send_byte(0, NT7534_CMD_SET_DISP_START);
    nt7534_send_byte(0, NT7534_CMD_SET_CONTRAST);
    nt7534_send_byte(1, 0x20); // 对比度
    nt7534_send_byte(0, NT7534_CMD_DISPLAY_ON);

    // 4. 清屏+测试显示
    nt7534_clear();
    nt7534_draw_string(0, 0, "BPI-W2 GPIO SPI!");
    printf("NT7534: GPIO SPI初始化完成\n");

    return 0;
}

// U-Boot LCD框架接口（不变）
void lcd_clear(void) { nt7534_clear(); }
void lcd_puts(const char *str) {
    nt7534_draw_string(0, 2, str);
    printf("%s", str);
}
void lcd_setcol(unsigned int col) {}
void lcd_setrow(unsigned int row) {}
void lcd_enable(void) {}
void lcd_disable(void) {}
struct lcd_display_info lcd_display_info = {
    .type = LCD_TYPE_MONO,
    .width = NT7534_WIDTH,
    .height = NT7534_HEIGHT,
};
