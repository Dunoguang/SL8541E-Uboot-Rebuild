#include "loader_common.h"
#include <asm/global_data.h>
DECLARE_GLOBAL_DATA_PTR;
#include <mmc.h>
#include <fat.h>
#if defined(CONFIG_OF_LIBFDT)
#include <libfdt.h>
#include <fdt_support.h>
#endif
#include <sprd_common_rw.h>
#include <linux/usb/usb_uboot.h>

#ifdef CONFIG_MINI_TRUSTZONE
#include "trustzone_def.h"
#endif

#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
#include "cp_mem_decoupling.h"
#endif
#include "sprd_cpcmdline.h"
#include <asm/arch/sprd_reg.h>
#include <hwfeature.h>

#include "soc/sprd/board.h"
#ifdef ZCFG_KEY_VOLUME_DOWN_ANTI_SHAKE
#include <asm/io.h>
#include <asm/arch/pinmap.h>

#endif

#define DECOUPLING_INFO_PARTITION "modem"
#ifdef CONFIG_SUPPORT_NR
#define DECOUPLING_INFO_PARTITION_NRPHY "nrphy"
#define DECOUPLING_INFO_PARTITION_V3PHY "v3phy"
#endif


unsigned char raw_header[8192];
unsigned int g_charger_mode = 0;
char serial_number_to_transfer[SP15_MAX_SN_LEN];

extern int charger_connected(void);
extern void modem_entry(void);
#ifdef ZCFG_BOOT_DDR_MEMTEST
extern void ddr_memtester(void);
#endif
unsigned short calc_checksum(unsigned char *dat, unsigned long len)
{
	unsigned short num = 0;
	unsigned long chkSum = 0;
	while (len > 1) {
		num = (unsigned short)(*dat);
		dat++;
		num |= (((unsigned short)(*dat)) << 8);
		dat++;
		chkSum += (unsigned long)num;
		len -= 2;
	}
	if (len) {
		chkSum += *dat;
	}
	chkSum = (chkSum >> 16) + (chkSum & 0xffff);
	chkSum += (chkSum >> 16);
	return (~chkSum);
}

unsigned char _chkNVEcc(uint8_t *buf, uint64_t size, uint32_t checksum)
{
	uint16_t crc;

	crc = calc_checksum(buf, (unsigned long)size);
	debugf("_chkNVEcc calcout 0x%x, org 0x%x\n", crc, (uint16_t)checksum);
	return (crc == (uint16_t) checksum);
}

/*modif to support the sp15 64 bit sn NO */
char *get_product_sn(void)
{
	SP09_PHASE_CHECK_T phase_check_sp09;
	SP15_PHASE_CHECK_T phase_check_sp15;
	uint32_t magic;

	memset(serial_number_to_transfer, 0x0, SP15_MAX_SN_LEN);

	strcpy(serial_number_to_transfer, "0123456789ABCDEF");
	if (0 != common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(magic), (uint64_t)0, (char *)&magic)) {
		errorf("read miscdata error.\n");
		return serial_number_to_transfer;
	}
	if(magic == SP09_SPPH_MAGIC_NUMBER){
		if(common_raw_read(PRODUCTINFO_FILE_PATITION,sizeof(phase_check_sp09), (uint64_t)0, (char *)&phase_check_sp09)){
			debugf("sp09 read miscdata error.\n");
			return serial_number_to_transfer;
		}
		if(strlen(phase_check_sp09.SN1)){
			memcpy(serial_number_to_transfer, phase_check_sp09.SN1, SP09_MAX_SN_LEN);
		}
	}else if(magic == SP15_SPPH_MAGIC_NUMBER){
		if(common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp15), (uint64_t)0 ,(char *)&phase_check_sp15)){
			debugf("sp15 read miscdata error.\n");
			return serial_number_to_transfer;
		}
		if(strlen(phase_check_sp15.SN1)){
			memcpy(serial_number_to_transfer, phase_check_sp15.SN1, SP15_MAX_SN_LEN);
		}
	}
	return serial_number_to_transfer;
}

extern int fdt_chosen_bootargs_append(void *fdt, char *append_args, int force);

void fdt_fixup_all(u8 *fdt_blob)
{
	uint32_t fdt_size;
	boot_img_hdr *hdr = raw_header;
	int err;
	char  *boot_mode_type_str;

	if (fdt_check_header(fdt_blob) != 0) {
		errorf("image is not a fdt\n");
	}

	/* [fbcon] kernel log to the panel via DRM fbdev */
	fdt_chosen_bootargs_append(fdt_blob, (char *)" console=tty0", 1);

#ifdef CONFIG_SPLASH_SCREEN
	fdt_fixup_lcdid(fdt_blob);
	fdt_fixup_lcdname(fdt_blob);
	fdt_fixup_lcdbase(fdt_blob);
	fdt_fixup_lcdsize(fdt_blob);
	fdt_fixup_pixelclock(fdt_blob);
	fdt_fixup_lcdbpix(fdt_blob);
#endif

#ifndef CONFIG_ZEBU
	fdt_fixup_adc_calibration_data(fdt_blob);
#endif
	//fdt_fixup_dram_training(fdt_blob);
	fdt_fixup_ddr_size(fdt_blob);
	fdt_fixup_ro_boot_ramsize(fdt_blob);
	fdt_fixup_ddrsize_range(fdt_blob);
	fdt_fixup_hwfeature(fdt_blob);
	fdt_fixup_sysdump_uboot(fdt_blob);
	fdt_fixup_sysdump_magic(fdt_blob);
#ifdef CONFIG_SECURE_BOOT
	fdt_fixup_secureboot_param(fdt_blob);
#endif
#ifdef CONFIG_NAND_BOOT
	fdt_fixup_mtd(fdt_blob);
#ifdef CONFIG_UBI_ATTACH_MTD
	fdt_fixup_ubi_ai(fdt_blob);
#endif
#endif

#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION);
#ifdef CONFIG_SUPPORT_NR
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION_NRPHY);
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION_V3PHY);
#endif
#endif

#if defined (CONFIG_SP_DDR_BOOT) && defined (CONFIG_MEM_LAYOUT_DECOUPLING)
	/* sp ddr boot info */
	fdt_fixup_sp_info(fdt_blob);
#endif

	/*max let cp_cmdline_fixup befor fdt_fixup_cp_boot*/
	cp_cmdline_fixup();
	fdt_fixup_cp_boot(fdt_blob);

	boot_mode_type_str = getenv("bootmode");
	if (NULL != boot_mode_type_str)
	{
		if(!strncmp(boot_mode_type_str, "sprdisk", 7)) {
			fdt_fixup_memleakon(fdt_blob);
		}
	}

	/*for verified boot*/
	fdt_fixup_verified_boot(fdt_blob);
	fdt_fixup_flash_lock_state(fdt_blob);
#ifndef CONFIG_ZEBU
	fdt_fixup_serialno(fdt_blob);
#endif
	fdt_fixup_chosen_bootargs_board_private(fdt_blob);

#ifdef CONFIG_SANSA_SECBOOT
	//fdt_fixup_socid(fdt_blob);
#endif

	/* for non iq mode, remove reserved iq memory from dtb */
	fdt_fixup_iq_reserved_mem(fdt_blob);

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
	/* board kernel cmdline in header of boot.img */
	fdt_fixup_board_kernel_cmdline(fdt_blob);
#endif
#ifdef CONFIG_UDC
		fdt_fixup_udc_cmdline(fdt_blob);
#endif

	/* if enabled, set loglevel = 7*/
	fdt_fixup_loglevel(fdt_blob);
	fdt_fixup_system_mount_part_num(fdt_blob);

#ifdef CONFIG_GET_CPU_SERIAL_NUMBER
	fdt_fixup_cpu_serial_number(fdt_blob);
#endif

#ifdef CONFIG_VBOOT_V2
    fdt_fixup_vboot(fdt_blob);
#endif

	fdt_fixup_first_mode(fdt_blob);

#ifdef CONFIG_TEECFG_CUSTOM
	if(reboot_mode_check() != CMD_TOS_PANIC_MODE)
		fdt_fixup_tee_reserved_mem(fdt_blob);
#endif

	return;
}

void fdt_fixup_all_x86(u8 *fdt_blob)
{
	uint32_t fdt_size;
	boot_img_hdr *hdr = raw_header;
	int err;
	char  *boot_mode_type_str;

	if (fdt_check_header(fdt_blob) != 0) {
		errorf("image is not a fdt\n");
	}

#ifdef CONFIG_SPLASH_SCREEN
	fdt_fixup_lcdid(fdt_blob);
	fdt_fixup_lcdname(fdt_blob);
	fdt_fixup_lcdbase(fdt_blob);
	fdt_fixup_lcdsize(fdt_blob);
	fdt_fixup_pixelclock(fdt_blob);
	fdt_fixup_lcdbpix(fdt_blob);
#endif

	fdt_fixup_adc_calibration_data(fdt_blob);

	fdt_fixup_serialno(fdt_blob);

#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION);
#ifdef CONFIG_SUPPORT_NR
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION_NRPHY);
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION_V3PHY);
#endif
#endif

	/*max let cp_cmdline_fixup befor fdt_fixup_cp_boot*/
	cp_cmdline_fixup();

	fdt_fixup_cp_boot(fdt_blob);

	/*for verified boot*/
	fdt_fixup_verified_boot(fdt_blob);
	fdt_fixup_flash_lock_state(fdt_blob);

	/* for non iq mode, remove reserved iq memory from dtb */
	fdt_fixup_iq_reserved_mem(fdt_blob);

	fdt_fixup_pmic_wa(fdt_blob);

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
	/* board kernel cmdline in header of boot.img */
	fdt_fixup_board_kernel_cmdline(fdt_blob);
#endif

	/* if enabled, set loglevel = 7*/
	fdt_fixup_loglevel(fdt_blob);
	fdt_fixup_system_mount_part_num(fdt_blob);

#ifdef CONFIG_VBOOT_V2
    fdt_fixup_vboot(fdt_blob);
#endif
}

#ifdef CONFIG_X86
static int start_linux_x86()
{
	struct boot_params *base_ptr;
	struct boot_img_hdr *boot_hdr = (void *)raw_header;
	uint64_t load_address = 0;
	char *bzImage_addr = KERNEL_ADR;
	ulong bzImage_size = 0;
	ulong initrd_addr = RAMDISK_ADR;
	ulong initrd_size = PAD_SIZE(boot_hdr->ramdisk_size, KERNL_PAGE_SIZE);
	int image_64bit = 0;

	debugf("initrd_size: 0x%lx\n", initrd_size);
	disable_interrupts();
	wbinvd();

#ifndef CONFIG_ZEBU
	/*start modem CP */
	modem_entry();
#endif

	/* Setup board for maximum PC/AT Compatibility */
	setup_pcat_compatibility();
	/* Lets look for */
	base_ptr = load_zimage(bzImage_addr, bzImage_size, &load_address);

	if (!base_ptr) {
		puts("## Kernel loading failed ...\n");
		return -1;
	}
	if (setup_zimage(base_ptr, (char *)base_ptr + COMMAND_LINE_OFFSET,
			0, initrd_addr, initrd_size)) {
		puts("Setting up boot parameters failed ...\n");
		return -1;
	}
	image_64bit = image_check64(base_ptr);
	/* we assume that the kernel is in place */
	return boot_linux_kernel((ulong)base_ptr, load_address, image_64bit);
}
#endif

#ifdef CONFIG_ARM
static int start_linux(uchar *dt_addr)
{
#ifdef CONFIG_OF_LIBFDT
	void (*theKernel) (int zero, int arch, u32 params);
	theKernel = (void (*)(void *, int, int, int))g_kernel_load_addr;	/* [autoplace] */

	/*disable all caches */
	cleanup_before_linux();

	/*start modem CP */
#ifndef CONFIG_ZEBU
	modem_entry();
#endif

#ifdef CONFIG_MINI_TRUSTZONE
	trustzone_entry(TRUSTZONE_ADR + 0x200);
#endif
	/* jump to kernel with register set */
	theKernel(0, machine_arch_type, (u32)dt_addr);
#else
	void (*theKernel) (int zero, int arch, u32 params);
	u32 machine_type = 0;

	machine_type = machine_arch_type;	/* get machine type */
	theKernel = (void (*)(int, int, u32))KERNEL_ADR;	/* set the kernel address */

	/*start modem CP */
	cp_cmdline_fixup();

	/*disable all caches */
	cleanup_before_linux();

	/*start modem CP */
	modem_entry();

	/* jump to kernel with register set */
	theKernel(0, machine_type, VLX_TAG_ADDR);
#endif /*CONFIG_OF_LIBFDT */
	while (1) ;
	return 0;
}
#endif




static void arm_probe(uchar *dt);
static void probe_dump(void);

/* [probe] print CPU state at the kernel entry, then continue */
static void probe_dump(void)
{
    unsigned long sctlr = 0, mmfr0 = 0, el = 0, mpidr = 0;

    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    printf("[K] sctlr0=%08lx sctlr=%08lx mmfr0=%08lx parange=%lu el=%lu mpidr=%08lx\n",
           *(volatile unsigned long *)0x84000200UL,
           sctlr, mmfr0, (mmfr0 >> 28) & 0xfUL, (el >> 2) & 3UL, mpidr);
}

#define PROBE_STUB    0x84000100UL
#define PROBE_TARGET  0x81aec0c0UL     /* after __enable_mmu */

static void arm_probe(uchar *dt)
{
    u32 w[17] = {
        0xd10183ff, 0xa90007e0, 0xa9010fe2, 0xa90217e4, 0xa9031fe6,
        0xa9047bf3, 0xd503201f, 0xd503201f, 0xd503201f, 0xa9447bf3,
        0xa9431fe6, 0xa94217e4, 0xa9410fe2, 0xa94007e0, 0x910183ff,
        0xd503201f, 0x176bafe1
    };
    volatile u32 *stub = (volatile u32 *)PROBE_STUB;
    u32 tgt = (u32)(ulong)&probe_dump;
    u32 delta, i;

    w[6] = 0xd2800000u | (((tgt & 0xffffu) << 5)) | 16u;
    w[7] = 0xf2800000u | (1u << 21) | ((((tgt >> 16) & 0xffffu) << 5)) | 16u;
    w[8] = 0xd63f0200u;            /* blr x16 */

    for (i = 0; i < 17; i++)
        stub[i] = w[i];

    delta = (u32)(PROBE_STUB - PROBE_TARGET) >> 2;
    *(volatile u32 *)PROBE_TARGET = 0x14000000u | (delta & 0x3FFFFFFu);

    {
        int noff = fdt_path_offset((void *)dt, "/memory");
        const u32 *r;
        int len = 0;
        if (noff < 0)
            noff = fdt_path_offset((void *)dt, "/memory@80000000");
        printf("[mem] gd->ram_size=0x%llx\n", (u64)gd->ram_size);
        if (noff >= 0 && (r = fdt_getprop((void *)dt, noff, "reg", &len)) && len >= 16)
            printf("[mem] dt /memory: base=0x%llx size=0x%llx end=0x%llx\n",
                   ((u64)r[0] << 32) | r[1], ((u64)r[2] << 32) | r[3],
                   (((u64)r[0] << 32) | r[1]) + (((u64)r[2] << 32) | r[3]));
        else
            printf("[mem] dt /memory node not found (noff=%d len=%d)\n", noff, len);
    }
    printf("[probe] entry stub=0x%lx dump=0x%x target=0x%lx\n",
           (ulong)PROBE_STUB, tgt, PROBE_TARGET);
    flush_dcache_range(PROBE_STUB, PROBE_STUB + 0x100);
    flush_dcache_range(PROBE_TARGET & ~0xfffUL,
                       (PROBE_TARGET & ~0xfffUL) + 0x1000UL);
}


static int start_linux_armv8(uchar *dt_addr)
{
	void (*theKernel) (void *dtb_addr, int zero, int arch, int reserved);
	theKernel = (void (*)(void *, int, int, int))g_kernel_load_addr;	/* [fix] was KERNEL_ADR */
	printf("[KERNEL CHK] @0x80000000: w0=%08x magic=%08x text_off=%llx size=%llx\n",
	       *(volatile u32 *)0x80000000UL, *(volatile u32 *)0x80000038UL,
	       *(volatile unsigned long long *)0x80000008UL,
	       *(volatile unsigned long long *)0x80000010UL);
	printf("[jmp] theKernel=0x%lx dt=0x%lx\n", (ulong)theKernel, (ulong)dt_addr);
	arm_probe(dt_addr);
/* [klog] drop a marker outside the kernel image so the next boot can
 * tell whether DRAM survived the reset */
*(volatile u32 *)0x8F000000 = 0xDEADBEEF;
	*(volatile u32 *)0x824d9918 = 0xCAFEB0BA;	/* inside __log_buf (.bss) */
memcpy((void *)0x8F000010, "UBOOTMARKER12345", 16);

	/*before switch to el2,flush all cache */
	/*FIXME: cleanup_before_linux() will cause panic here, we need to find the solution*/
	flush_dcache_range(CONFIG_SYS_SDRAM_BASE, CONFIG_SYS_SDRAM_END);
#ifdef CONFIG_DUAL_DDR
	flush_dcache_range(CONFIG_SYS_SDRAM_BASE2, CONFIG_SYS_SDRAM_END2);
#endif

	/*sharkle and sharklj1 need disable mmu and cache before jump into kernel*/
	cleanup_before_linux();

	/*start modem CP */
	modem_entry();

#ifdef CONFIG_MINI_TRUSTZONE
	trustzone_entry(TRUSTZONE_ADR + 0x200);
#endif

	/*kernel must run in el2, so here switch to el2 */
	armv8_switch_to_el2();
	theKernel(dt_addr, 0, 0, 0);

	/*never enter here*/
	while (1) ;
	return 0;
}

extern uint32_t uboot_start_time;

void vlx_entry(uchar *dt_addr)
{
	char *bootmode = getenv("bootmode");
	uint32_t consume_time;

	consume_time = SCI_GetTickCount() - uboot_start_time;
	printf("enter mode %s, consume time: %dms\n",
		!bootmode ? "normal" : bootmode, consume_time);

    #ifdef ZCFG_KEY_VOLUME_DOWN_ANTI_SHAKE
    __raw_writel(0x10, CTL_PIN_BASE + 0x208);
    #endif

	/* the last time to write log */
	write_log_last();

#ifdef DFS_ON_ARM7
	cp_dfs_param_for_arm7();
#endif
	/*shutdown usb ldo, can't shutdown it in the ldo_sleep.c because download mode must use usb */
	usb_driver_exit();

	/* start ddr memtest */
#ifdef ZCFG_BOOT_DDR_MEMTEST
	if (!bootmode) 
	{
		ddr_memtester();
	}
#endif
#ifdef CONFIG_ARM64
	smp_kick_all_cpus();
	start_linux_armv8(dt_addr);
#endif
#ifdef CONFIG_ARM
	start_linux(dt_addr);
#endif
#ifdef CONFIG_X86
	start_linux_x86();
#endif
}
