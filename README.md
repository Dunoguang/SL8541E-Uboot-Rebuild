# SL8541E-Uboot-Rebuild

展锐 SL8541E / SC9832E（sharkle，sprd,sl8541e_1h10）智能手表
U-Boot 2015.07 的源码级重建。

- 基线：`sprd-linux/u-boot15`，分支 `sprdroid10_trunk`
- 验证设备：vp19 手表（412x502 co5300 AMOLED）
- 工具链：aarch64-linux-gnu-gcc 13.3 / 16.1
- 产物：`out/u-boot-dtb.bin`（约 547 KB，含设备树）

这不是一个新的 bootloader，而是把展锐那份只在内部使用的
老源码树重新弄成"能编、能跑、能看见日志"的状态，
并把踩过的坑写成补丁留在树里。

## 为什么有这个仓库

展锐的 SL8541E 手表方案只对外发 U-Boot 二进制，
源码树虽然挂在 GitHub 上，但直接 `make` 是编不过的：

1. 用现代 GCC 编，十几处错误（C++ 注释、隐式声明、
   `bool` 关键字、x86 预编译 `dtc`、dtc 头冲突）。
2. 屏幕驱动只有二进制，源码里没有 co5300 这个面板。
3. 校验链大量 `while (1);`，出错就原地死转，
   屏幕不提示、串口没输出，看起来就是"开机卡 LOGO"。
4. 没有可用的日志通道，调试基本靠猜。

以上四条都在本仓库里解决了。

## 已实现的功能

| 功能 | 位置 | 说明 |
| --- | --- | --- |
| 现代 GCC 编译 | 多处，见下节 | gcc 13/16 直接过 |
| co5300 面板驱动 | `drivers/video/sprdfb/lcd/lcd_co5300_oled206_mipi.c` | 412x502，MIPI CMD，27 条 init，GPIO33/50 复位 |
| 屏幕日志 (lcdlog) | `common/console.c` + `CONFIG_UBOOT_LCDLOG` | 控制台同时输出到面板 |
| USB 串口日志 (usblog) | `common/loader/usblog.c` + `CONFIG_UBOOT_USBLOG` | 控制台镜像到 u2s (VID 1782/PID 4d00)，分块 + 超时自停 |
| gzip 内核解压 | `common/loader/loader_nvm.c` | 压缩流落 `0x83800000`，解压到 `0x80000000` |
| 内核落点自动识别 | 同上 + `loader_common.c` | 读 arm64 Image 头 `text_offset`，4.4/7.x 通吃 |
| 软重启变砖修复 | `common/cmd_cboot.c` | 判不出模式时正常启动，不再关机 |
| 校验链去死循环 | `lib/trustzone/uboot_drv_api.c`、`lib/secureboot/common/sec_common.c` | 出错返回，不原地死转 |
| 校验结果强制 OK | `sec_common.c` 的 `[forceok]` | 等价于解锁工具改校验函数 |
| 异常现场打印 | `arch/arm/lib/interrupts_64.c` | 同步异常时打印 ELR/SPSR/寄存器 |

## 编译

```sh
./build.sh            # 需要 aarch64-linux-gnu-gcc、dtc、bison、flex
```

`build.sh` 会做三件容易被忽略的事：

- 把仓库里那个 x86 的 `tools/dtc/dtc` 换成系统的 dtc
  （这是对已跟踪文件的修改，`git checkout tools/dtc/dtc` 可还原）
- 加 `-std=gnu11`（gcc 16 默认 gnu23，`typedef int bool` 会炸）
- 传入展锐 `BspUBoot.mk` 里的那批 `-DCONFIG_*` 安全启动开关

产物：`out/u-boot.bin`（不带 DTB）、`out/u-boot-dtb.bin`（带 DTB，
打包刷机要用这一个）。

## 打包成 1 MiB 分区镜像

分区镜像不是裸 bin，外面还有 DHTB 头和签名块：

```
0x000  DHTB 头 512 字节，只有 [0x30] = 载荷长度 有效
0x200  u-boot-dtb.bin（16 字节对齐）
+0x2B4 SEC 块（sechdr 0x60 + cert 0x254）
```

SEC 块必须从原厂镜像原样保留（里面有 TOS 认的公钥位置和偏移），
只改长度字段：

```sh
python3 tools/sprd_pack.py 原厂uboot.bin out/u-boot-dtb.bin uboot-new.img
```

脚本会在末尾打印 md5。它和本仓库之前手工封装的产物逐字节一致。

## 刷机

```sh
adb push uboot-new.img /data/local/tmp/
adb shell 'dd if=/data/local/tmp/uboot-new.img \
           of=/dev/block/by-name/uboot bs=1048576 count=1'
adb shell sync
```

注意 toybox 的 `dd` 不认 `conv=fsync`，写完单独 `sync`。
写完回读一次核对 md5 再重启。

## 调试通道

- 开机后屏幕左上角会滚动控制台日志（lcdlog）。
- 插 USB，PC 端会枚举一个 `SPRD U2S Diag` 串口，
  115200 打开即可收 U-Boot 日志（usblog）。
- 日志里带进度标记：`[K1]` 读内核前、`[K2]` 读完、
  `[K3]` 跳转前、`[L1]` 内核头、`[autoplace]` 落点计算。
- 跳到错误地址时异常向量表会打印 `ELR`/`SPSR`/全寄存器，
  这比黑屏好定位得多。

## 已知问题

- 加载 7.2/7.3 主线内核后没有可见输出。
  U-Boot 侧交接已验证正确（落点、DT、寄存器都对），
  卡在内核早期，缺 UART 通道无法继续定位。
- 只移植了 co5300 oled206 一个面板。原厂 U-Boot 里也只有这一个。
- `g_DeviceStatus` 被读成 LOCK，INFO 横幅不显示，
  system/vendor 不被跳过（与解锁状态无关，原厂同样如此）。
- `secure_get_partition_size("system"/"vendor")` 返回 0。
- usblog 在 `is_bat_low` 之后会停发（TX 完成中断不再来），
  所以主力观测通道是 lcdlog。

## 免责声明

仅供学习、研究与修复自己设备之用。
刷机有风险，变砖自负。仓库里的 SEC 块来自原厂固件，
版权归原厂所有，只作为占位保留。

## 相关仓库

- [SL8541E-BootChain](https://github.com/Dunoguang/SL8541E-BootChain)
  启动链五件套二进制 + IDA 全量伪 C
- [8541E-Uboot-Patcher](https://github.com/Dunoguang/8541E-Uboot-Patcher)
  不编译、直接给原厂镜像打补丁（去保护 + usblog + 横幅）
- [uboot-usblog](https://github.com/Dunoguang/uboot-usblog)
  u2s 日志通道的原始逆向与读取工具
