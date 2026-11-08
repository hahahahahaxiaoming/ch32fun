# USB UART Bridge

## 芯片与接线

使用 CH570（支持 CH570Q），默认串口参数为 `115200 8N1`。

| 信号 | 引脚 | 接线 |
| --- | --- | --- |
| USB D− / D+ | PA0 / PA1 | 接 USB D− / D+ |
| UART1 TX / RX | PA3 / PA2 | TX 接对端 RX，RX 接对端 TX，共地 |
| RS485 方向控制 | PA7 | 接收发器并联的 DE 和 `/RE`，低电平接收，高电平发送 |

RS485 的 A/B 接外部收发器，UART 引脚为逻辑电平。

## 编译与烧录

在 `user/usb_uart_bridge` 运行 `make` 编译两版，或运行 `make uart` / `make rs485` 单独编译。

| 版本 | APP VID:PID | APP 镜像 | 升级 BAT |
| --- | --- | --- | --- |
| UART | `1A86:0001` | `build/CH570/uart/usb_uart.bin` | `tools/flash_CH570_uart.bat` |
| RS485 | `1A86:0002` | `build/CH570/rs485/usb_rs485.bin` | `tools/flash_CH570_rs485.bat` |

Bootloader 烧到 `0x0000`，APP 烧到 `0x1000`。已有 Bootloader 时，进入下文的 HID Boot 后运行对应 BAT。

客户驱动安装包为 `tools/driver/output/USB_Bridge_Driver_Setup.exe`，单独分发此 EXE 即可。重新生成时运行 `tools/driver/build_setup.ps1`。

## 首次通过 USB 一次烧录 Bootloader 和 APP

在 `user/usb_uart_bridge` 编译 Bootloader 和 APP。

```powershell
make -C ../usb_bootloader CH570
make
```

让目标板进入**原厂 USB ISP**，按板型双击对应脚本，一次写入 Bootloader 和 APP。

- UART 使用 `tools/flash_factory_CH570_uart.bat`。
- RS485 使用 `tools/flash_factory_CH570_rs485.bat`。

当前 `EraseCodeFlash=$true`，会先整片擦除 CodeFlash。

## 通过 USB 单独烧录 Bootloader

在 `tools/flash_bootloader.ps1` 中将 `EraseCodeFlash` 设为 `$false`，让目标板进入**原厂 USB ISP**，然后在 `user/usb_uart_bridge` 运行以下命令。

```powershell
make -C ../usb_bootloader CH570
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/flash_bootloader.ps1
```

## 通过 HID Boot 手动恢复 APP

按住目标板的 **PA7 Boot 键**上电，上电后松开。确认只有目标板枚举为 `1209:B003`，再运行对应 BAT，或在仓库根目录按板型选一条命令。

**UART**

```powershell
.\minichlink\minichlink.exe -C b003boot -w .\user\usb_uart_bridge\build\CH570\uart\usb_uart.bin 0x1000 -b
```

**RS485**

```powershell
.\minichlink\minichlink.exe -C b003boot -w .\user\usb_uart_bridge\build\CH570\rs485\usb_rs485.bin 0x1000 -b
```

## 生成 Bootloader 与 APP 合并镜像

在 `user/usb_uart_bridge` 运行以下命令，生成对应 APP 目录下的 `*_factory.bin`。

```powershell
make -C ../usb_bootloader CH570
make
py -3 tools/download.py --mode uart --factory-image
py -3 tools/download.py --mode rs485 --factory-image
```

按板型选择镜像，通过编程器或原厂 USB ISP 从 `0x0000` 烧录。
