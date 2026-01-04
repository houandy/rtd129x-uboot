#include <common.h>
#include <gpio.h>
#include <configs/rtd1296_qa_spi_16MB_64.h>  // 或实际使用的配置头文件

int spi_gpio_init(void)
{
    /* 配置SCK为输出（初始低电平） */
    if (gpio_request(SPI_SCK_GPIO, "spi_sck") != 0) {
        printf("SPI: Failed to request SCK GPIO\n");
        return -1;
    }
    gpio_direction_output(SPI_SCK_GPIO, 0);

    /* 配置MOSI为输出（初始低电平） */
    if (gpio_request(SPI_MOSI_GPIO, "spi_mosi") != 0) {
        printf("SPI: Failed to request MOSI GPIO\n");
        return -1;
    }
    gpio_direction_output(SPI_MOSI_GPIO, 0);

    /* 配置MISO为输入 */
    if (gpio_request(SPI_MISO_GPIO, "spi_miso") != 0) {
        printf("SPI: Failed to request MISO GPIO\n");
        return -1;
    }
    gpio_direction_input(SPI_MISO_GPIO);

    /* 配置CS为输出（初始高电平，未选中） */
    if (gpio_request(SPI_CS_GPIO, "spi_cs") != 0) {
        printf("SPI: Failed to request CS GPIO\n");
        return -1;
    }
    gpio_direction_output(SPI_CS_GPIO, 1);

    /* 配置复位引脚（初始高电平，未复位） */
    if (gpio_request(SPI_RST_GPIO, "spi_rst") != 0) {
        printf("SPI: Failed to request RST GPIO\n");
        return -1;
    }
    gpio_direction_output(SPI_RST_GPIO, 1);

    printf("SPI: GPIO initialized successfully\n");
    return 0;
}
