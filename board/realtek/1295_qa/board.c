/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2012 by Chuck Chen <ycchen@realtek.com>
 *
 * Time initialization.
 */
#include <common.h>
#include <asm/arch/sys_proto.h>
#include <common.h>
#include <gpio.h>

#ifdef CONFIG_RTK_POWER
extern void RTK_power_saving(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

const struct rtd1295_sysinfo sysinfo = {
	"Board: Realtek QA Board\n"
};

/**
 * @brief checkboard
 *
 * @return 0
 */
int checkboard(void)
{
	printf(sysinfo.board_string);
	return 0;
}

/**
 * @brief board_init
 *
 * @return 0
 */
int board_init(void)
{
	//gd->bd->bi_arch_number = MACH_TYPE_RTK_RTD1295;
	/* boot param removed since ATAG is not used anymore*/

	return 0;
}

/**
 * @brief dram_init_banksize
 *
 * @return 0
 */
/*void dram_init_banksize(void)
{
	// Bank 1
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = CONFIG_SYS_RAM_DCU1_SIZE;

#if (CONFIG_NR_DRAM_BANKS > 1)
	// Bank 2
	gd->bd->bi_dram[1].start = CONFIG_SYS_SDRAM_DCU2_BASE;
	gd->bd->bi_dram[1].size = CONFIG_SYS_RAM_DCU2_SIZE;
#endif

#if (CONFIG_NR_DRAM_BANKS > 2)
	// Bank 3
#if defined(CONFIG_SYS_SDRAM_DCU_OPT_BASE) && defined(CONFIG_SYS_RAM_DCU_OPT_SIZE)
	gd->bd->bi_dram[2].start = CONFIG_SYS_SDRAM_DCU_OPT_BASE;
	gd->bd->bi_dram[2].size = CONFIG_SYS_RAM_DCU_OPT_SIZE;
#endif
#endif

}*/

int board_eth_init(bd_t *bis)
{
	return 0;
}

/**
 * @brief misc_init_r - Configure Panda board specific configurations
 * such as power configurations, ethernet initialization as phase2 of
 * boot sequence
 *
 * @return 0
 */
int misc_init_r(void)
{
#ifdef CONFIG_RTK_POWER
	RTK_power_saving();
#endif
	return 0;
}

/*
 * get_board_rev() - get board revision
 */
u32 get_board_rev(void)
{
	uint revision = 0;

	revision = (uint)simple_strtoul(CONFIG_VERSION, NULL, 16);

	return revision;
}

// 初始化SPI相关GPIO
void nt7534_spi_gpio_init(void)
{
    // 配置SCK为输出（SPI时钟）
    gpio_request(NT7534_SPI_SCK, "spi_sck");
    gpio_direction_output(NT7534_SPI_SCK, 0);  // 初始低电平

    // 配置MOSI为输出（SPI数据发送）
    gpio_request(NT7534_SPI_MOSI, "spi_mosi");
    gpio_direction_output(NT7534_SPI_MOSI, 0);

    // 配置CS为输出（片选，初始高电平无效）
    gpio_request(NT7534_SPI_CS, "spi_cs");
    gpio_direction_output(NT7534_SPI_CS, 1);

    // 配置复位引脚（若有）
    gpio_request(NT7534_RST, "lcd_rst");
    gpio_direction_output(NT7534_RST, 1);  // 初始高电平（未复位）
}

// 在板级初始化函数中调用
int board_init(void)
{
    // ... 其他初始化 ...
    nt7534_spi_gpio_init();  // 初始化SPI GPIO
    return 0;
}
