# CH570 / CH592 USB Bootloader（CH592 固件兼容 CH591）

## CH570 配置

- `make CH570` 或 `make build TARGET_MCU=CH570` 生成 `build/CH570/usb_bootloader.bin`，同目录另有 HEX、ELF、MAP 和 LST。
- CH570 使用独立固件，按 240 KiB 用户 Flash、12 KiB RAM 链接。Bootloader 占前 4 KiB，应用从 `0x1000` 开始，最多 236 KiB。
- 系统时钟使用 `CLK_SOURCE_PLL_60MHz`。CH570Q 的 USB 使用 PA0（D−，3 脚）和 PA1（D+，4 脚），与两线调试复用。Bootloader 在启用 USB 前清除 `RB_PIN_DEBUG_EN`，释放这两个引脚。
- **CH570 固定 `BOOT_LOG_ENABLE=0`**，不初始化日志 UART，不编译日志代码及字符串。
- **按键和 LED 均为 PA7，按键低电平有效，LED 高电平点亮。** 上电先将 PA7 配置为上拉输入、读取按键；进入 USB Boot 后将 PA7 配置为推挽输出高电平，之后不再检测按键。
- **共用 PA7 的硬件限制：** 若按键直接将 PA7 接地，LED 输出高电平时继续按住或再次按下会造成输出冲突，外部电路必须解决该冲突。高电平点亮的 LED 支路也可能影响上拉输入时的电压，必须实测未按下时能读到高电平。仅切换输入/输出模式不能保证该接线可靠。
- CH570 应用入口同样检查 `0x1000` 的跳转指令，以及 `0x1014` 的 SDK 向量表标记 `0xF3F9BDA9`；CH592 保持 `0xF5F9BDA9`。CH570 应用需自行链接到 `0x1000` 并保留对应标记，不能直接烧录链接到 0 的应用。
- `BOOT_FORCE_STAY` 默认为 `0`，允许启动有效应用。排查 USB 枚举时可临时改为 `1` 并重新编译，此时上电和空闲超时都不会跳到应用，无需按住 PA7。升级协议、进入请求 `0xB7`、运行请求 `0xB8` 和 30 秒空闲超时沿用原逻辑。

以下 PB4、PB23、UART1 PA9 及 CH592F_DEMO 说明适用于 CH592/CH591。

**当前 `BOOT_FORCE_STAY=0`：按住 PB4 上电/复位，或应用写入进入请求标记时，停留 Bootloader；否则检查 `0x1000` 的应用入口，有应用则跳转，没有则停留。排查时可将此项改为 1，禁止所有应用跳转。**

参考 `examples_usb/bootloader`，通过 USB HID 与本仓库的 minichlink `b003boot` 后端通信。
配套 CH592F_DEMO 应用使用 CDC 输出日志，并通过 HID 接收进入 Bootloader 的命令。

## 地址与启动

| 区域 | 地址 | 容量 |
| --- | --- | --- |
| 自定义 Bootloader | `0x00000000–0x00000FFF` | 4 KiB |
| 应用 | `0x00001000–0x0002FFFF` | 188 KiB |

先按两种芯片共用的 192 KiB Flash 布局处理，不使用原厂 ISP 区域。

- 正常上电经过 Bootloader，有应用则立即执行应用的复位入口，由应用重新初始化 RAM、中断向量和外设。
- 跳转前关闭 CPU 全局中断，停止并清除 ch32fun 启动代码启用的 SysTick，关闭 SysTick/USB IRQ 并清除挂起状态，然后执行屏障并跳到应用启动入口。应用启动文件负责恢复 mstatus、mtvec、SP、GP 和 RAM；APP 保持原 SDK 的 SysTick 初始化写法。
- 按住 PB4 上电/复位，Bootloader 自行读取按键并强制停留在 USB 模式，不依赖应用处理按键。进入后可松开。连续 30 秒没有 minichlink 命令且应用入口有效时，Bootloader 断开 USB 并复位进入应用，即使此时 PB4 仍按住。每次接收命令和执行结束后重新计时。
- USB 模式收到 HID 升级命令后调用 `JumpToUsbIsp()`：日志发送完毕后，将 `R8_GLOB_RESET_KEEP` 写为 `0xB7`，再软件复位。Bootloader 清除标记后停留在 USB 模式，PB23 LED 高电平点亮。
- minichlink `-b` 通过原有 b003 ABI 调用复位函数，使用标记 `0xB8` 复位运行应用。
- 启动和 USB 超时均检查应用入口指令及 CH592F_DEMO 向量表标记（地址 `0x1014`，值 `0xF5F9BDA9`）。任一检查失败即停留 USB 模式。当前仅检查这两处，没有完整镜像 CRC 或断电升级恢复机制。

`boot_config.h` 中 `BOOT_LOG_ENABLE` 默认 1，上电初始化时钟后，通过 UART1 PA9 TX（115200、8N1）以一位十进制数字显示复位标志并输出一行启动日志，例如 `[BOOT] V1.0 RST=1 -> APP`。进入 USB Boot 时，行末为 `-> BOOT`。串口发送完成后执行相应分支。版本号由 `BOOT_VERSION` 设置。`RST` 在 main 入口读取 `R8_RESET_STATUS & RB_RESET_FLAG`：0 为软件复位、1 为上电复位、2 为看门狗复位、3 为外部手动复位、5 为休眠唤醒复位；该字段是硬件记录值，软件复位未必总覆盖先前的原因（例如 SDK 开启看门狗复位使能时）。其他调试日志已移除。将 `BOOT_LOG_ENABLE` 改为 0 后，UART 初始化、日志代码和字符串均不参与编译。Boot 模式使用 PB23 LED 指示，并在 USB 初始化前主动断开 100ms。应用自身的日志由应用提供。

主机 `-b` 调用的复位入口延迟 50ms，给最后的控制传输留出完成时间；随后关闭 USB 和其中断、清除挂起状态，再保持断开 10ms，写入运行标记并复位。看门狗复位自动进入 Boot 和完整镜像校验尚未实现。

## 编译

在本目录运行 `make`，生成 CH570 固件和 CH591、CH592 共用的一份 CH592 固件：

```text
build/CH570/usb_bootloader.bin   .hex / .elf / .map
build/CH592/usb_bootloader.bin   .hex / .elf / .map
```

只编译 CH592 可使用 `make CH592` 或 `make build TARGET_MCU=CH592`，输出目录为 `build/CH592`。

已适配外部项目 `D:/Flowtech/2602ch592/software/CH592F_DEMO`：

- `make LOG_USB=1`（默认）选择 USB CDC 日志和 HID 升级，应用从 `0x1000` 启动，容量 188 KiB。
- `make LOG_USB=0` 选择 UART 日志和原厂 ISP，应用独立链接到 `0x0000`，容量 192 KiB。切换模式会覆盖同名产物。
- `JumpToUsbIsp()` 在 USB 模式写入 `0xB7` 并复位，在 UART 模式执行原有的 `R16_INT32K_TUNE = 0xFFFF` 复位流程。
- 默认 `make` 编译四个应用版本，`build/<版本>.bin` 是 `build/make/<版本>.bin` 的裸应用副本，起点均为 `0x1000`。
- SDK 暂不依赖此 Bootloader 工程，也不自动编译或合并 Bootloader。待应用稳定后再将 Bootloader 复制到应用项目。

## USB 模式首次烧录和升级测试

1. 在 SDK 目录执行 `make -j4`。
2. 首次先用编程器将本工程 `build/CH592/usb_bootloader.bin` 烧录到 `0x00000000`，然后用 minichlink 将 SDK 裸应用写入 `0x1000`（见下节）。已有 Bootloader 时直接更新应用。应用异常时可按住 PB4 上电强制进入 Bootloader。
3. 复位后检查原应用正常运行，执行下方连接命令，由 minichlink 请求进入 HID Bootloader。
4. 在 ch32fun 根目录执行：

```powershell
.\minichlink\minichlink.exe -C b003boot -i
.\minichlink\minichlink.exe -C b003boot -b
```

第一条检查连接，第二条复位返回应用。USB 标识沿用参考工程 `VID 1209 / PID B003`，设备名称 `bootloader`，使用系统 HID 驱动。这里不是 CDC 串口，不输出 USB 串口日志。

USB 模式下，SDK **`build/<版本>.bin` 和 `build/make/<版本>.bin` 都是裸应用，起点为 `0x1000`**；对应 HEX/ELF 也只有应用。不要将裸应用按地址 0 烧录。

## 经 USB 更新裸应用

按住 PB4 上电进入 Bootloader，或在 USB 模式应用正常运行时直接调用 minichlink：

```powershell
D:\github\ch32fun\minichlink\minichlink.exe -C b003boot -w "D:\Flowtech\2602ch592\software\CH592F_DEMO\build\make\smartkey_V1.0.2_ble.bin" 0x1000 -b
```

该命令将裸应用写入 `0x1000`，写入完成后执行 `-b` 启动应用，不执行额外的完整回读比对。

这是开发阶段接口：参考协议允许主机上传并执行 RAM 代码，设备没有强制的 Bootloader 写保护。不要使用整片擦除 `-E`，也不要向地址 0 写入裸应用，否则可能覆盖 Bootloader。更新中断后当前入口检查不能保证应用完整；可用编程器分别重写 Bootloader（地址 0）和应用（地址 `0x1000`）恢复。

请使用本仓库修正并重新编译后的 `minichlink.exe`。旧版 b003 写入入口会对 CH5xx 错误调用 CH32 Flash 解锁流程，出现 `Flash is not unlocked (CTLR = ...)`；CH5xx 应由自身的擦除、写入 RAM 代码解锁 `R8_GLOB_ROM_CFG`。

配套新版 APP 枚举为 `1209:D003`（CDC ACM + 厂商 HID）。打开 COM 口可看后续日志，日志队列为 128 字节，队满时丢弃新日志。运行上述升级命令时，minichlink 会向 APP 发送 Feature Report `FD 12 34 AA BB CC DD`，等待设备重新枚举为 `1209:B003` 后写入。首次从旧 APP 升级时，先按住 PB4 上电进入 Boot。

## 验证

- CH570Q 已在 60 MHz、`BOOT_FORCE_STAY=1` 的调试配置下验证 USB HID 枚举。默认已恢复为 `BOOT_FORCE_STAY=0`。板上 USB 升级和应用跳转待验证。
- CH592 已完成编译及镜像布局检查，板上已验证 USB 枚举、minichlink 写入及完整回读。应用启动交接仍在排查。
