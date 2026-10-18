# 改动清单

全部改动都在 `sprd-linux/u-boot15`（`sprdroid10_trunk`）之上。
按"为什么改"分组，每组都能单独回退。

## 一、让现代 GCC 编得过

| 文件 | 改动 | 原因 |
| --- | --- | --- |
| `include/linux/compiler-gcc16.h` | 新增，复制自 `compiler-gcc5.h` | 没有 gcc6+ 的头就找不到 `compiler-gcc.h` |
| `tools/dtc/dtc` | 换成系统 dtc（`build.sh` 自动做） | 仓库里带的是 x86-64 预编译文件，arm64 上 `Exec format error` |
| `scripts/dtc-version.sh` | 兼容 `Version: DTC v1.8.1` | 老脚本只认不带 `v` 的写法 |
| `include/configs/sl8541e_1h10.h` | 14 处 `//` 注释改 `/* */` | gcc 新标准下 C 文件里的 `//` 会触发 `-Wcomment` 升级成错误 |
| `common/image-fit.c`、`common/image-sig.c` | `fdt_for_each_subnode` 参数顺序（5 处） | 本树 libfdt 的宏签名与调用处不一致 |
| `lib/libfdt/libfdt_env.h` | 新增副本 | 系统 dtc 的头与本树 libfdt 同名冲突 |
| `build.sh` 的 `KCFLAGS` | `-std=gnu11` + 6 个 `-Wno-*` | gcc16 默认 gnu23，`typedef int bool` 变成关键字；其余是历史遗留的隐式声明 |
| `lib/secureboot/sprd/sprd_verify.c` | `"\r\Nosec"` 改 `"\r\nNo sec"` | 非法转义序列 |
| `drivers/video/sprdfb/sprdfb_main.c` | 去掉 `inline` | 新 GCC 不再为 `inline` 函数发射符号，链接时报未定义 |
| `common/loader/loader_nvm.c` | 硬编码 `../../../../kernel/kernel4.14/...` 改 `"soc/sprd/board.h"` | 源码树里没有 kernel 目录 |
| `include/soc/sprd/board.h` | 新增（取自 Watch-GKI 内核树） | 上面那一行引用它，原厂靠 monorepo 相对路径 |
| `include/configs/sl8541e_1h10.h` | 补 `#define ZCFG_TSX_32KLESS` | `sprd_boardid_sp9832e.c` 里有 `#error` |

## 二、屏幕

`drivers/video/sprdfb/lcd/lcd_co5300_oled206_mipi.c`（新增）

原厂 U-Boot 只带这一个面板，但源码里没有。参数从原厂二进制里
逐字段取回：412x502、MIPI **CMD** 模式（不是 VIDEO）、
`phy_feq 409000`、27 条 init 表、复位序列 GPIO33/50
（`1 -> 20ms -> 1 -> 20ms -> 0 -> 20ms -> 1 -> 50ms`）。

面板注册是纯 C 数组，不看 DTS：`sprdfb_panel.c` 的 `panel_cfg[]`
里放一项 `{.lcd_id = 0x530206, .panel = &lcd_co5300_oled206_mipi_spec}`，
`lcd_id` 就是 `co5300_readid()` 的返回值。

## 三、不再卡死

原厂代码里到处都是"出错就 `while (1)`"：

- `lib/trustzone/uboot_drv_api.c`：5 个 SMC 封装
  （`uboot_verify_img`、`uboot_vboot_verify_img`、
  `uboot_verify_product_sn_signature`、`uboot_config_os_version`、
  `uboot_set_root_of_trust`）。去 AVB 的 trustos 不认这些功能号，
  返回非 0 就原地死转，表现为"跳内核前卡住"。
- `lib/secureboot/common/sec_common.c`：
  `vboot_verify_vbmeta()` 的 `fail: while (1);`，
  以及 `secure_load_partition()` 里两份 vbmeta 都对不上时的 `while (1);`。
- `common/cmd_cboot.c`：模式检测循环撞到数组末尾的 NULL 时，
  `bootmode` 被设成 `CMD_POWER_DOWN_DEVICE`，于是**关机**。
  冷启动时 watchdog 检测会提前结束循环所以看不出来，
  软重启后所有检测都判不出，就掉进这个兜底，表现为"黑屏变砖"。

全部改成返回，`bootmode` 兜底改成 `CMD_NORMAL_MODE`。

## 四、校验按解锁处理

`[forceok]`：不动校验流程（照样读分区、照样发 SMC），
只把结果强制成 OK，等价于解锁工具把校验函数改成 `return 0`。
好处是横幅、`verify_base`、modem 镜像的 secure boot 路径都不受影响。

## 五、内核加载

- `KERNEL_BASE_ADDR 0x80000000` + 读 arm64 Image 头
  `text_offset`（`0x08`，u64）决定落点：
  4.4 内核是 `0x80000` → `0x80080000`，7.x 是 `0` → `0x80000000`。
- 跳转指针 `g_kernel_load_addr` 必须跟着落点走。
  之前只改了加载地址没改跳转，等于跳高 32 KB 进镜像腰部，
  表现为 Synchronous Abort。
- gzip 内核：压缩流落 `0x83800000`，`gunzip()` 解到 `0x80000000`，
  再按 `text_offset` 搬（7.x 不需要搬）。

## 六、日志通道

- `common/loader/usblog.c`：`puts()` 末尾的钩子，把控制台镜像到
  u2s（USB CDC-ACM）。按 64 字节分块、每块 10 ms 超时，
  连续 40 次超时自动停用，避免 PC 端没开工具时把开机拖慢。
- `common/console.c` + `CONFIG_UBOOT_LCDLOG`：镜像到面板。
  **必须调 `lcd_enable()`**，CMD 模式面板写帧缓冲不刷新不上屏。
- `arch/arm/lib/interrupts_64.c`：同步异常时打印
  `MPIDR`/`CurrentEL`/`SPSR`/`ELR`/全寄存器，并扫内核栈。

## 七、调试开关

板级头文件末尾：

```c
#define CONFIG_UBOOT_LCDLOG      /* 屏幕日志 */
#define CONFIG_UBOOT_USBLOG      /* u2s 日志 */
/* #define UBOOT_NO_PART_VERIFY */   /* 旧方案，已被 [forceok] 取代 */
```

`UBOOT_NO_PART_VERIFY` 是早先把校验函数整块裁掉的做法，
副作用是横幅和 modem 镜像的 secure boot 路径一起没了，
现在只留注释备查。
