
# RZ/T2H USB Debug Console 分析


## 1. 問題背景

在 RZ/T2H Evaluation Board 上，需要確認並建立穩定可用的 **Debug Console**，同時釐清：

-   Linux `stdout-path` 與實體電路是否一致
-   板上多個 USB port（CN34 / CN33 / CN79）是否可作為 debug console
-   為何部分 USB port 無法在 Windows 枚舉為 COM port

----------

## 2. Linux Console 軟體設定確認

### 2.1 Device Tree 設定
```dts
/chosen {
    stdout-path = "serial0:115200n8";
};
```
Pinmux 定義：
```dts
sci0_pins: sci0 {
    pinmux = <RZT2H_PORT_PINMUX(27, 5, 0x14)>, /* SCI0_TXD */
             <RZT2H_PORT_PINMUX(27, 4, 0x14)>; /* SCI0_RXD */
};
```
### 2.2 Runtime 驗證

`dmesg | grep tty` 

結果顯示：

-   `ttySC0` 為 active console
-   對應 `SCIF@0x80005000`
    

`cat /sys/class/tty/console/active # tty0 ttySC0` 

**結論**  
Linux kernel console 使用 **SCI0 / ttySC0**。

----------

## 3. CN34（FT2232）Debug Console 分析

### 3.1 Windows 端枚舉結果

Windows 裝置管理員顯示：

`VID_0403 & PID_6010` 

→ 對應 **FTDI FT2232 系列**

### 3.2 Schematic 對應關係

電路圖 net name：

-   `P27_5_FT2232_TXD0`
-   `P27_4_FT2232_RXD0`
    

### 3.3 證據鏈總結

| 層級      | 證據內容                               |
|-----------|----------------------------------------|
| DTS       | serial0 → SCI0 → P27_5 / P27_4         |
| Schematic | P27_5 / P27_4 → FT2232 TXD0 / RXD0     |
| PC        | FT2232 枚舉為 COM port                |
| Runtime   | ttySC0 可互動                          |


**結論**  
👉 **CN34 = 主 UART Debug Console（Early boot 可用）**

----------

## 4. USB Gadget（g_serial）軟體能力驗證

### 4.1 Kernel 能力確認
```bash
ls /sys/class/udc # 92041000.usb
```
Kernel config：
```yaml
CONFIG_USB_GADGET=y
CONFIG_USB_G_SERIAL=m
CONFIG_USB_F_ACM=m
```
### 4.2 啟用 gadget serial
```bash
modprobe g_serial

ls /dev/ttyGS0 
```
成功建立：

-   `/dev/ttyGS0`
-   `g_serial ready`
    
**結論**  
👉 Linux USB gadget serial 功能正常

----------

## 5. CN33（USB OTG）為何無法枚舉？

### 5.1 現象

-   Linux 端：`g_serial ready`
-   Windows 端：**完全無 USB 裝置出現**
-   無 `VBUS` / `USB connect` log

### 5.2 Schematic 硬體分析

CN33 為 **USB OTG port**，關鍵硬體條件：

1.  **USB_OTG_ID 腳**
    
    -   由 **CN89 jumper** 控制
    -   ID → GND = Host mode
    -   ID 浮接 = Device mode
2.  **E8 / E9（Jumper_Trace_Cut）**
    -   可能切斷 D+/D− 路徑
        
### 5.3 推論

在目前板子設定下：

-   CN33 被固定為 **USB Host**
-   SoC 未進入 Device mode
-   即使 gadget ready，PC 端也不會枚舉
    

**結論**  
👉 CN33 在目前硬體設定下 **不適合作為 debug console**

----------

## 6. CN79（USB Device）實測結果

### 6.1 行為

-   CN79 插上 Windows
-   **立即出現 COM port**
-   無需調整 ID / jumper

### 6.2 Gadget Console 驗證
```bash
modprobe g_serial
setsid getty -L ttyGS0 115200 vt100 
```
Windows PuTTY / TeraTerm：

`rzt2h login:` 

成功登入。

**結論**  
👉 **CN79 = 可用 USB Device Debug Console（Linux runtime）**

----------

## 7. 最終建議配置（Best Practice）

| Interface        | 用途                              | Linux TTY |
|------------------|-----------------------------------|-----------|
| CN34 (FT2232)    | 主 console / early boot / panic   | ttySC0   |
| CN79 (USB Device)| 第二 console / runtime debug      | ttyGS0   |
| CN33 (USB OTG)   | 需調整 ID / trace                 | 不建議   |


----------

## 8. CN79 Debug Console 指令整理

### 啟用
```bash
modprobe g_serial
systemctl enable serial-getty@ttyGS0.service
systemctl start  serial-getty@ttyGS0.service
```
### Windows

-   新出現 COM port
-   115200 / Serial
