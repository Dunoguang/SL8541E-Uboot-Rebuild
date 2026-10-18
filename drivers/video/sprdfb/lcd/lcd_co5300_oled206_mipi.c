/*
 * drivers/video/sprdfb/lcd/lcd_co5300_oled206_mipi.c
 *
 * Support for co5300 AMOLED mipi panel (412x502, oled206)
 * for SL8541E watch platform.
 *
 * Sequence extracted byte-exactly from vendor U-Boot binary.
 */

#include "../sprdfb_chip_common.h"
#include "../sprdfb.h"
#include "../sprdfb_panel.h"

#define printk printf

#define MAX_DATA   56

typedef struct LCM_Init_Code_tag {
unsigned int tag;
unsigned char data[MAX_DATA];
}LCM_Init_Code;

#define LCM_TAG_SHIFT 24
#define LCM_TAG_MASK  ((1 << 24) -1)
#define LCM_SEND(len) ((1 << LCM_TAG_SHIFT)| len)
#define LCM_SLEEP(ms) ((2 << LCM_TAG_SHIFT)| ms)

#define LCM_TAG_SEND  (1 << 0)
#define LCM_TAG_SLEEP (1 << 1)

/* panel power / reset gpio (from vendor power-on-sequence) */
#define LCM_GPIO_PWR    33
#define LCM_GPIO_RSTN   50

/* readid prepare payload, datatype 0x37 (set max return packet size) */
static unsigned char rd_prep_data[] = {0x03, 0x00};

static LCM_Init_Code init_data[] = {
{LCM_SEND(2), {0xFE, 0x20}},
{LCM_SEND(2), {0xF4, 0x5A}},
{LCM_SEND(2), {0xF5, 0x59}},
{LCM_SEND(2), {0xFE, 0x40}},
{LCM_SEND(2), {0x5D, 0x24}},
{LCM_SEND(2), {0x60, 0x08}},
{LCM_SEND(2), {0x61, 0x02}},
{LCM_SEND(2), {0x62, 0x03}},
{LCM_SEND(2), {0x0C, 0xD7}},
{LCM_SEND(2), {0x0D, 0xF0}},
{LCM_SEND(2), {0xFE, 0xD0}},
{LCM_SEND(2), {0x2A, 0x80}},
{LCM_SEND(2), {0x2B, 0x02}},
{LCM_SEND(2), {0xFE, 0x90}},
{LCM_SEND(2), {0x4E, 0x1F}},
{LCM_SEND(2), {0xFE, 0x00}},
{LCM_SEND(2), {0x35, 0x00}},
{LCM_SEND(2), {0x3A, 0x77}},
{LCM_SEND(2), {0x53, 0x20}},
{LCM_SEND(2), {0x51, 0x78}},
{LCM_SEND(2), {0x63, 0xFF}},
{LCM_SEND(7), {5, 0, 0x2A, 0x00, 0x16, 0x01, 0xB1}},
{LCM_SEND(7), {5, 0, 0x2B, 0x00, 0x00, 0x01, 0xF5}},
{LCM_SEND(1), {0x11}},
{LCM_SLEEP(120)},
{LCM_SEND(1), {0x29}},
{LCM_SLEEP(20)},
};

static int32_t co5300_reset(struct panel_spec *self)
{
sprd_gpio_request(NULL, LCM_GPIO_RSTN);
sprd_gpio_request(NULL, LCM_GPIO_PWR);

sprd_gpio_direction_output(NULL, LCM_GPIO_PWR, 1);
mdelay(20);
sprd_gpio_direction_output(NULL, LCM_GPIO_RSTN, 1);
mdelay(20);
sprd_gpio_direction_output(NULL, LCM_GPIO_RSTN, 0);
mdelay(20);
sprd_gpio_direction_output(NULL, LCM_GPIO_RSTN, 1);
mdelay(50);

return 0;
}

static int32_t co5300_mipi_init(struct panel_spec *self)
{
int32_t i;
LCM_Init_Code *init = init_data;
unsigned int tag;

mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
mipi_gen_write_t mipi_gen_write = self->info.mipi->ops->mipi_gen_write;
mipi_eotp_set_t mipi_eotp_set = self->info.mipi->ops->mipi_eotp_set;

printk("uboot co5300_mipi_init\n");

mipi_set_cmd_mode();
mdelay(5);
mipi_eotp_set(1, 0);

for (i = 0; i < ARRAY_SIZE(init_data); i++) {
tag = (init->tag >> 24);
if (tag & LCM_TAG_SEND) {
mipi_gen_write(init->data, (init->tag & LCM_TAG_MASK));
udelay(20);
} else if (tag & LCM_TAG_SLEEP) {
mdelay((init->tag & LCM_TAG_MASK));
}
init++;
}

mdelay(5);
mipi_eotp_set(1, 1);

return 0;
}

static uint32_t co5300_readid(struct panel_spec *self)
{
unsigned char read_data[4] = {0};
int32_t retry = 4;

mipi_set_cmd_mode_t mipi_set_cmd_mode = self->info.mipi->ops->mipi_set_cmd_mode;
mipi_force_write_t mipi_force_write = self->info.mipi->ops->mipi_force_write;
mipi_force_read_t mipi_force_read = self->info.mipi->ops->mipi_force_read;

mipi_set_cmd_mode();

while (1) {
mipi_force_write(0x37, rd_prep_data, sizeof(rd_prep_data));
mdelay(50);

mipi_force_read(0x04, 4, read_data);
printk("co5300_readid read id value is 0x%x,0x%x,0x%x,0x%x!  and return counter:%d\n",
       read_data[0], read_data[1], read_data[2], read_data[3], 0);

if (read_data[0] == 0x33 && read_data[1] == 0x11)
break;

if (--retry == 0)
return 0;
}

return 0x05300206;
}

static struct panel_operations lcd_co5300_oled206_mipi_operations = {
.panel_init = co5300_mipi_init,
.panel_reset = co5300_reset,
.panel_readid = co5300_readid,
};

static struct timing_rgb lcd_co5300_oled206_mipi_timing = {
.hfp = 16,
.hbp = 24,
.hsync = 4,
.vfp = 9,
.vbp = 8,
.vsync = 1,
};

static struct info_mipi lcd_co5300_oled206_mipi_info = {
.work_mode  = SPRDFB_MIPI_MODE_CMD,
.video_bus_width = 24,
.lan_number = 1,
.phy_feq = 409000,
.h_sync_pol = SPRDFB_POLARITY_POS,
.v_sync_pol = SPRDFB_POLARITY_POS,
.de_pol = SPRDFB_POLARITY_POS,
.te_pol = SPRDFB_POLARITY_POS,
.color_mode_pol = SPRDFB_POLARITY_NEG,
.shut_down_pol = SPRDFB_POLARITY_NEG,
.timing = &lcd_co5300_oled206_mipi_timing,
.ops = NULL,
};

struct panel_spec lcd_co5300_oled206_mipi_spec = {
.width = 412,
.height = 502,
.fps = 60,
.type = LCD_MODE_DSI,
.direction = LCD_DIRECT_NORMAL,
.is_need_reset = 1,
.info = {
.mipi = &lcd_co5300_oled206_mipi_info
},
.ops = &lcd_co5300_oled206_mipi_operations,
};
