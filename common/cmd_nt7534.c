#include <common.h>
#include <command.h>

/* 声明NT7534函数 */
int nt7534_init(void);
void nt7534_clear(void);
void nt7534_draw_string(int x, int y, const char *str);

int do_nt7534(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
    if (argc < 2) {
        printf("Usage:\n");
        printf("nt7534 init   - Init NT7534 screen\n");
        printf("nt7534 clear  - Clear screen\n");
        printf("nt7534 show <str> - Show string (e.g., nt7534 show U-Boot123)\n");
        return -1;
    }

    if (!strcmp(argv[1], "init")) {
        return nt7534_init();
    } else if (!strcmp(argv[1], "clear")) {
        nt7534_clear();
        printf("NT7534: Screen cleared\n");
    } else if (!strcmp(argv[1], "show") && argc == 3) {
        nt7534_draw_string(0, 0, argv[2]);
        printf("NT7534: Show string: %s\n", argv[2]);
    } else {
        printf("Invalid arguments\n");
        return -1;
    }

    return 0;
}

U_BOOT_CMD(
    nt7534, 3, 1, do_nt7534,
    "NT7534 LCD control",
    "init   - Initialize NT7534\n"
    "clear  - Clear screen\n"
    "show <str> - Display string on screen\n"
);
