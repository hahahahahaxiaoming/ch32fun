# CH570 USB CMSIS-DAP v2

与 `../usb_uart_bridge` 使用相同的 CH570 平台和前置 4 KiB Bootloader。
这里的 `usb_daplink` 是 SWD 调试探针：仅提供 CMSIS-DAP v2 WinUSB Bulk 接口，
不包含 HID DAP v1、CDC 串口或拖拽烧录 U 盘。

系统主频固定为 **100 MHz**（`CLK_SOURCE_PLL_100MHz`），在 `funconfig.h`
配置。DAP 延时计算和 SysTick/LED 定时统一使用 `FUNCONF_SYSTEM_CORE_CLOCK`。
CPU 主频不是 SWD 时钟；默认请求 SWD 时钟为 4 MHz。

## 接线

| 信号 | CH570 引脚 | 说明 |
| --- | --- | --- |
| USB D− / D+ | PA0 / PA1 | 与串口桥相同 |
| SWCLK | PA2 | 接目标的 SWCLK |
| SWDIO | PA3 | 接目标的 SWDIO，双向 |
| 活动 LED | PA7 | 高电平亮，串限流电阻 |
| GND | GND | 与目标共地 |

使用与目标兼容的 3.3 V 逻辑电平；本工程没有电平转换或目标供电检测。
断开调试时 PA2、PA3 为浮空输入。默认请求 SWD 时钟为 4 MHz，主机可以更改；
GPIO 软件产生时钟，实际频率受指令、Flash 等待和中断影响，尚未用示波器标定。

PA7 在有效 DAP 收发时活动，持续传输时以约 50 ms 亮 / 50 ms 灭闪烁，
最后一次数据交互后约 120 ms 熄灭。PA7 仍兼容原 Bootloader 的启动按键用途。

未分配 nRESET，因此不支持硬件复位或 connect-under-reset；调试软件需使用
通过 SWD 的软件复位。`DAP_ResetTarget` 不执行自定义硬件复位。
支持标准 ARM SWD，不是 WCH RISC-V 专用的单线调试协议。

## 编译及升级

在本目录执行：

```powershell
make
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/firmware.ps1
```

输出 `build/CH570/usb_daplink.bin`、`.hex`、`.elf`、`.map` 和 `.lst`。
APP 链接地址为 `0x1000`，`0x1014` 保存 `0xF3F9BDA9` 标记。
Flash 上界为 `0x3C000`，RAM 为 12 KiB，向量表保持 1 KiB 对齐。

已有本仓库 Bootloader：按住 PA7 Boot 键上电、松开，确认目标进入
`1209:B003` HID Boot，然后双击 `tools/flash_CH570.bat`，或执行：

```powershell
make flash
```

只连接一块待升级的 Bootloader 设备。此操作将 APP 写到 `0x1000` 后启动。
APP 的 WinUSB 接口本身不提供升级入口，仍通过 PA7 上电进入 HID Boot。

首次烧录可生成合并镜像：

```powershell
make -C ../usb_bootloader CH570
make
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/firmware.ps1 -FactoryImage
```

生成 `build/CH570/usb_daplink_factory.bin`，将其通过 CH570 原厂 USB ISP
烧到 `0x0000`。合并脚本仅生成文件，不操作硬件；不要将合并镜像烧到 `0x1000`。
本工程没有复制串口桥的原厂 ISP 自动烧录脚本，避免其中 UART/RS485 镜像选择逻辑误选。

## Windows / 调试软件

产品字符串为 `CH570 CMSIS-DAP v2`，序列号为 `DAP-` 加芯片工厂 MAC 的 12 位十六进制值。
默认 APP VID:PID 为 `1A86:0003`，与本地 UART/RS485 示例区分；这是开发用标识，
没有声明该 PID 已获得厂商分配。发布产品前在 `usb_config.h` 中换成有权使用的 VID/PID，
同时修改 `tools/firmware.ps1` 的镜像标识校验。

USB 接口为 `FF/00/00`，EP2 OUT 和 EP1 IN 均为 64 字节 Bulk。
提供 Microsoft OS 1.0 描述符（`0xEE` 字符串、WINUSB Compatible ID、
`DeviceInterfaceGUIDs` 多字符串属性），GUID 为
`{CDB3B5AD-293B-4663-AA36-1AAE46463776}`。
Windows 10/11 应据此自动绑定 WinUSB；仍需实板确认枚举。
调试软件选择 CMSIS-DAP v2 / USB Bulk 和 SWD，首次连目标建议从 100 kHz 开始。

## 下载速度优化

### Keil + STM32

`Options for Target → Debug` 选择 `CMSIS-DAP Debugger`，进入 `Settings`：

| 设置 | 值 |
| --- | --- |
| 调试探针 | CH570 CMSIS-DAP v2（对应 DAP- 序列号） |
| Port | SW |
| Max Clock | 4 MHz；若该版本菜单没有此档，先选不超过 4 MHz 的邻近档 |
| Connect | Normal |
| Reset | SYSRESETREQ |

在 `Flash Download` 选择与具体 STM32 型号、Flash 容量匹配的烧录算法，
使用 `Erase Sectors`、`Program`、`Verify`；按需选择 `Reset and Run`。
没有 nRESET 接线，不能选 `Under Reset` 或依赖 `HW RESET`。
如果 Keil 只支持 HID CMSIS-DAP v1，则需更新至支持 CMSIS-DAP v2 的版本；
本固件不会枚举 HID 接口。

设置参考：
[Keil CMSIS-DAP 设置示例（芯片厂商指南）](https://www.infineon.com/assets/row/public/documents/30/44/infineon-keil-uvision-for-modustoolbox-user-guide-usermanual-en.pdf)。

### 固件侧优化

- SWD 传输、SWJ/SWD 序列在 RAM 中执行，使用 `O2` 优化，减少 Flash 取指等待。
- USB IN 直接使用对齐的响应缓冲区做 DMA，省去发送前的一次复制。
  缓冲区直到主机完成接收才释放，USB 复位时丢弃旧队列。
- IN 完成中断直接续发已经准备好的下一包，避免等待主循环完成当前 SWD 操作。
- 向主机报告 8 个流水包，另留未报告的缓冲空间接收异步 Abort。
- 默认 SWD 请求频率提高到 4 MHz，保留主机的频率控制，不强制忽略低速设置。

需要在下载软件中也选择合适的 SWD 频率；软件发送的时钟设置会覆盖固件默认值。
连线和目标确认稳定后可选择 4 MHz；若目标或连线不支持，应降低频率。
最终吞吐还受目标 Flash 擦写算法、目标 WAIT 响应和主机是否使用流水/块传输影响。
这里没有跳过校验，也没有实测 KB/s 或保证固定的提速倍数。

当前优化版链接占用：APP Flash 10,540 字节，静态 RAM 7,468 字节，
剩余 RAM 4,820 字节供运行栈等使用。RAM 中的代码计入静态 RAM，且其加载镜像也计入 Flash。

协议使用 [ARM 官方 CMSIS-DAP](https://github.com/ARM-software/CMSIS-DAP)，
WinUSB 接口遵循其 [USB 配置说明](https://arm-software.github.io/CMSIS-DAP/latest/dap_firmware.html)。
源代码固定版本及修改见 `vendor/CMSIS-DAP/README.md`，无需构建时联网下载。
支持 DAP Info、连接、SWD 配置/序列、寄存器传输和块传输、WAIT 重试与异步 Abort。
主机可流水发送 8 个包。未启用 JTAG、SWO、UART、时间戳或原子命令队列。

## 验证范围

已完成交叉编译、APP 入口/标记及 WinUSB 描述符检查、合并镜像生成，
以及主机 C 测试覆盖截断包、传输计数上限、SWD 序列边界和不支持的原子命令。
上述主机测试已执行通过，测试源码现已从精简工程中移除。

共享 `../../extralibs/fsusb.c` 有两处配套修改：自定义控制 IN 请求的首包只复制
最多 64 字节；添加可选的 USB 总线复位回调以清理 DAP 队列。
串口桥不启用此回调。

尚未进行实板 Windows 枚举、SWD 时序测量、目标芯片读 ID / 烧录 / 单步调试，
以及拔插、流水传输和 Abort 的硬件测试。编译器现有的
`.data.vector_handler` section 属性警告仍存在；链接器的向量表对齐断言通过。
