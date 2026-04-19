# GD32H759 MCUBoot Bootloader

基于 [MCUBoot](https://github.com/mcu-tools/mcuboot) 的 GD32H759IMK6 安全引导程序，运行在 GD32H7xx Cortex-M7 微控制器上，支持固件签名验证与安全升级。

## 快速开始

```bash
# 克隆仓库（含 submodule）
git clone --recurse-submodules <repo-url>

# 使用 Keil MDK 打开工程
# Project/gd32h759temple.uvprojx → Build (F7)
```

或使用 [Just](https://github.com/casey/just) 命令行构建：

```bash
cd tools
just build      # 构建
just rebuild    # 清理 + 构建
just errors     # 查看错误
just warnings   # 查看警告
just clean      # 清理输出
```

## 目标硬件

| 参数 | 值 |
|------|-----|
| MCU | GD32H759IMK6 |
| 内核 | ARM Cortex-M7 (双精度 FPU) |
| 主频 | 600 MHz（PLL0 × HXTAL 25 MHz） |
| Flash | 3840 KB（960 × 4 KB 扇区），双字（8B）写入粒度 |
| SRAM | 512 KB AXI SRAM + 64 KB ITCM + 64 KB DTCM |
| 调试接口 | JTAG / SWD |
| 开发板 | GD32H759I-EVAL-V2.0（参考） |

## 构建环境

| 工具 | 版本 | 说明 |
|------|------|------|
| Keil MDK | μVision 5 | IDE / 链接器 / 调试器 |
| ARM Compiler | V6.23 (AC6) | 基于 Clang/LLVM |
| GD32H7xx DFP | 1.4.0 | GigaDevice 设备支持包 |
| Git | 2.x | 拉取 MCUBoot submodule |
| Just | 可选 | 命令行构建工具 |

全局宏定义：`MBEDTLS_CONFIG_FILE=<mcuboot-mbedtls-cfg.h>`

## Flash 分区布局

```
GD32H759 Flash 总计 3840 KB (0x0800_0000 - 0x083B_FFFF)
┌──────────────┬───────────┬──────────────────────────────┐
│   偏移地址    │   大小     │          用途                 │
├──────────────┼───────────┼──────────────────────────────┤
│ 0x0000_0000  │  128 KB   │ Bootloader（本工程）           │
│ 0x0002_0000  │  640 KB   │ Primary Slot（Slot 0，运行固件）│
│ 0x000C_0000  │  640 KB   │ Secondary Slot（Slot 1，升级）  │
│ 0x0016_0000  │  128 KB   │ Scratch（仅 Swap 模式使用）     │
│ 0x0018_0000  │  剩余空间  │ 保留 / 用户空间                 │
└──────────────┴───────────┴──────────────────────────────┘

绝对地址：
  Bootloader  : 0x0800_0000 - 0x0801_FFFF
  Primary Slot: 0x0802_0000 - 0x080B_FFFF  ← APP_ADDRESS
  Secondary   : 0x080C_0000 - 0x0815_FFFF
  Scratch     : 0x0816_0000 - 0x0817_FFFF
```

## RAM 分配

Scatter 文件（`Project/Output/gh32h759temple.sct`）定义了以下内存区域：

| 区域 | 地址 | 大小 | 用途 |
|------|------|------|------|
| ITCM | 0x0000_0000 | 64 KB | 指令紧耦合内存 |
| DTCM | 0x2000_0000 | 64 KB | 数据紧耦合内存（主 RW/ZI） |
| AXI SRAM | 0x2400_0000 | 896 KB | 大容量数据 |
| SRAM1 | 0x3000_0000 | 16 KB | 通用 |
| SRAM2 | 0x3000_4000 | 16 KB | 通用 |

## 目录结构

```
gd32h759boot/
├── App/                    # 引导加载程序入口
│   ├── main.c              #   主函数：复位源检测 → 跳转 App
│   └── gd32h7xx_libopt.h  #   外设库配置
│
├── Port/                   # MCUBoot 平台适配层
│   ├── mcuboot_glue.h      #   统一包含头（应用代码只需 include 此文件）
│   ├── include/
│   │   ├── flash_map_backend/   # Flash 抽象接口
│   │   ├── mcuboot_config/      # MCUBoot 配置与日志适配
│   │   ├── os/                  # 裸机内存分配（映射到 stdlib）
│   │   └── sysflash/            # Flash 分区表定义
│   └── src/
│       ├── flash_map_backend.c  # Flash 读写擦除（FMC 驱动）
│       ├── bootutil_keys.c      # ECDSA-P256 公钥（占位）
│       └── mbedtls_platform.c   # mbedTLS 平台适配
│
├── Firmware/               # 芯片厂商固件库 (V1.4.0)
│   ├── CMSIS/GD/GD32H7xx/  #   CMSIS 核心 + 启动文件 + 系统初始化
│   └── GD32H7xx_standard_peripheral/  # 53 个标准外设驱动
│
├── FMC/                    # GD32H7xx 官方 FMC 示例（仅供参考，不参与编译）
│   ├── Erase_Program/      #   Flash 擦除/编程
│   └── Write_protection/   #   Flash 写保护
│
├── console/                # 串口控制台
│   ├── console_usart.c     #   USART0 初始化 + printf 重定向
│   └── console_usart.h     #   PA9(TX) PA10(RX) 115200 8N1
│
├── logging/                # 轻量级日志框架
│
├── mcuboot/                # MCUBoot 上游仓库 (Git submodule, v2.4.0-rc1)
│
├── Project/                # Keil μVision 5 工程文件
│   ├── gd32h759temple.uvprojx
│   ├── Output/             #   编译输出 (.hex, .axf, .sct)
│   └── Listings/           #   链接映射文件
│
├── tools/
│   ├── justfile            #   Just 构建脚本
│   └── code.clang-format   #   代码风格配置
│
├── .gitmodules
└── build_log.htm           # 构建日志
```

## MCUBoot 移植配置

定义在 `Port/include/mcuboot_config/`：

| 配置项 | 值 | 说明 |
|--------|-----|------|
| 签名算法 | ECDSA-P256 | `MCUBOOT_SIGN_EC256` |
| 哈希算法 | SHA-256 | `MCUBOOT_SHA256` |
| 密码学后端 | mbedTLS | `MCUBOOT_USE_MBED_TLS` |
| 镜像模式 | 单镜像 | `MCUBOOT_IMAGE_NUMBER=1` |
| 升级模式 | Swap-using-scratch | `MCUBOOT_SWAP_USING_SCRATCH`（支持回滚，使用Scratch区） |
| 启动验证 | 启用 | `MCUBOOT_VALIDATE_PRIMARY_SLOT` |
| 日志 | 启用 | `[MCUBOOT][ERR/WRN/INF/DBG]` 前缀 |
| 最大扇区数 | 128 | `MCUBOOT_MAX_IMG_SECTORS` |
| 写入对齐 | 8 字节 | `MCUBOOT_BOOT_MAX_ALIGN` |

mbedTLS 仅启用 SHA-256、ECP(P-256)、ECDSA、PK、ASN1、BIGNUM 等必要模块，TLS、X509、RSA、AES 等均已裁剪。

## Flash 操作接口

`Port/src/flash_map_backend.c` 基于 GD32H7xx FMC 实现：

| API | 说明 |
|-----|------|
| `flash_area_open(id, &area)` | 按 ID 打开 Flash 区域 |
| `flash_area_close(area)` | 关闭区域（空操作） |
| `flash_area_read(area, off, dst, len)` | 读取数据 |
| `flash_area_write(area, off, src, len)` | 写入数据（8B 对齐，非对齐走 read-modify-write） |
| `flash_area_erase(area, off, len)` | 擦除（128KB 扇区对齐） |
| `flash_area_get_sectors(id, &cnt, sectors)` | 获取扇区信息 |

底层 FMC 调用：`fmc_unlock/lock` → `fmc_sector_erase` → `fmc_doubleword_program` → `fmc_flag_clear`

## 当前状态

已完成：
- MCUBoot 平台适配层（配置、日志、Flash 抽象、分区表、mbedTLS 裁剪）
- GD32H7xx FMC Flash 后端实现
- 串口控制台与日志输出
- Keil MDK 工程配置（编译通过）
- 简单的引导跳转（复位源检测 → 地址校验 → 跳转 App）

待完成：
- 集成 MCUBoot `boot_go()` 完整安全引导流程（签名验证 → 升级判定 → 跳转）
- 替换 `bootutil_keys.c` 中的占位公钥为真实密钥
- 实际固件签名与 OTA 升级测试

## 密钥管理与固件签名

### 生成密钥对

```bash
pip install imgtool

# 生成 ECDSA-P256 私钥
imgtool keygen -t ecdsa -k signing_key.pem

# 导出公钥 C 数组（替换 bootutil_keys.c 中的占位值）
imgtool getpub -k signing_key.pem -o pub_key.c
```

### 签名固件

```bash
imgtool sign \
    --key signing_key.pem \
    --version 1.0.0 \
    --align 8 \
    --header-size 0x200 \
    --slot-size 0xA0000 \
    --pad \
    app.bin \
    app_signed.bin
```

> 私钥 `signing_key.pem` 不得嵌入固件或随产品发布。

## 串口控制台

USART0，PA9(TX) / PA10(RX)，115200 8N1。`printf` 通过 `fputc()` 重定向至 USART0。

## 许可证

- 本项目适配层代码：**Apache-2.0**
- MCUBoot 上游代码：参见 `mcuboot/LICENSE`
- GD32H7xx 固件库及 FMC 示例：**BSD-3-Clause**（GigaDevice Semiconductor Inc.）
