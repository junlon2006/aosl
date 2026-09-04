# AOSL ASR3603S 平台适配

该目录实现 ASR3603S `craneg_modem_watch` 主镜像所需的 AOSL HAL。平台代码由
ARMCC 5 随 watch 工程编译，使用 ThreadX、OSA 和 modem lwIP。

## 构建约束

- `CONFIG_PLATFORM` 设置为 `asr3603`。
- CPU/ABI 继承 watch 工程：Cortex-R5、Thumb、little-endian、soft-float。
- 不定义 `__MICROLIB`；该宏仅属于 updater 子镜像。
- AOSL 的平台配置目录需要先于 `platform/include` 放入头文件搜索路径。
- lwIP 使用 `framework/inc/cp/lwipv4v6` 下与 LWG 固件一致的头文件和配置。

本平台启用 `select`、ThreadX semaphore 和 GEU 硬件随机数，不启用 epoll、poll
和 condition variable。IPv4 DSCP 通过 `IP_TOS` 实现；当前 SDK 的 IPv6 socket
接口没有 `IPV6_TCLASS`，因此 IPv6 DSCP 返回不支持。

## SDK 依赖

平台实现直接依赖以下 SDK 头文件和相应库：

- `tx_api.h`：IRQ 临界区以及 ThreadX 对象；
- `osa.h`：tick、sleep 和内存块大小；
- `pmic_rtc.h`：RTC Unix 秒和 UTC 日历时间；
- `bsp_common.h`：watch UART 日志；
- `lwip/sockets.h`、`lwip/netdb.h`、`lwip/netif.h`：socket、DNS 和默认网卡；
- `geu_random_number()`：GEU 硬件随机数，由 AES 模块导出。

链接时需要保留 watch 已有的 ThreadX/OSA、lwIPv4v6、RTC、AES 和 BSP
依赖。不要把厂商库复制进 AOSL 仓库。

## 板端验收

当前产品不使用 AOSL 文件接口，因此文件 HAL 与 Spreadtrum 平台保持一致，仅提供
无文件系统依赖的占位实现。启用文件日志或文件读写前，必须替换为真实实现。

至少验证以下项目：原子返回值、tick 回绕、RTC 秒边界、UDP loopback +
nonblock + select、DNS、IPv4/IPv6 地址转换、连续 UUID、随机数、
以及重复创建/销毁 MPQ 后的任务、socket 和堆资源回收。
