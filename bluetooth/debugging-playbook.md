
# Bluetooth Debugging Playbook


## 1. 本章定位
    

**本章的目標只有一個：**

> 給你一套「不用猜、不用試運氣」  
> **從症狀 → 層級 → 根因 → 修正方向** 的 Bluetooth 除錯手冊

----------

## 2. 除錯的最高原則

### 原則 1：先排除 BlueZ

> **90% 的 bring-up 問題不在 BlueZ**

### 原則 2：先驗證 Control Plane，再看 Data Plane

> hci0 起不來，資料面不可能正常

### 原則 3：UART 問題優先懷疑 transport

> firmware 很少「真的壞掉」

----------

## 3. 標準除錯工具組

| 工具        | 用途說明                                      |
|-------------|-----------------------------------------------|
| btmon       | 觀察 HCI Command / Event / ACL 封包           |
| btmgmt      | 直接操作 kernel mgmt layer（繞過 BlueZ）      |
| hciconfig   | 基本 HCI device 狀態檢查                      |
| dmesg -w    | 追蹤 driver / firmware 即時 log               |
| stty        | UART 參數與 baud rate 檢查                    |
| lsof        | 檢查 tty 裝置是否被多個程序佔用               |

----------

## 4. 除錯總流程
```
[Step 1] hci0 是否存在？
[Step 2] hci0 能否 power on？
[Step 3] HCI command / event 是否成對？
[Step 4] firmware 是否正確載入？
[Step 5] transport 是否穩定？
[Step 6] 才看 BlueZ / profile
```
----------

## 5. Step 1：hci0 不存在

### 5.1 檢查方式
```
hciconfig -a
btmgmt info
```
### 5.2 若 hci0 完全不存在

高機率問題層級：

-   ❌ transport driver 沒 attach
    
-   ❌ UART / USB 硬體未 ready
    
-   ❌ DT / ACPI / power / clock 問題
    

優先查看：

-   `drivers/bluetooth/hci_uart.c`
    
-   `drivers/bluetooth/btusb.c`
    
-   UART driver probe log
    

----------

## 6. Step 2：hci0 存在，但 power on 失敗

### 6.1 驗證方式
```
systemctl stop bluetooth
btmon &
btmgmt power on
```
### 6.2 常見結果與解讀

#### 情況 A：完全沒有 HCI command

-   kernel mgmt 沒送 command
    
-   hci_dev state 不正確
    

👉 檢查：

-   `net/bluetooth/mgmt.c`
    
-   `hci_dev_do_open()`
    

----------

#### 情況 B：有 command，沒有 event

btmon：

`> HCI Command: Reset (no event)` 

👉 **100% 是 transport / firmware / UART 問題**

-   UART：baud / RTS/CTS / framing
    
-   USB：firmware missing / controller crash
    

----------

## 7. Step 3：HCI command / event 對照判斷表

| btmon 行為                 | 判斷方向                         |
|----------------------------|----------------------------------|
| Command 有、Event 無       | Transport 或 Firmware 問題       |
| Event Status ≠ 0           | Firmware 或 Controller state 異常|
| Event 為亂碼               | UART framing / Baud rate 錯誤    |
| 僅 Reset 成功              | Firmware download 流程卡住       |

----------

## 8. Step 4：Firmware 相關問題定位

### 8.1 Kernel 路線（btbcm）

檢查：

`dmesg | grep -i -E "btbcm|firmware"` 

常見錯誤：

`Direct firmware load  for BCMxxxx.hcd failed with error  -2` 

→ rootfs 沒放 / 檔名錯

----------

### 8.2 User space 路線（brcm_patchram_plus）

檢查：

-   patch download 是否完整
    
-   是否在 reset 前後切 baud
    
-   是否與 kernel driver 搶 tty
    

----------

## 9. Step 5：UART 專屬除錯流程（H4）

### 9.1 確認只有一個 UART 使用者

`lsof /dev/ttyS9` 

如果看到：

-   brcm_patchram_plus
    
-   bluetoothd
    
-   hciattach  
    同時存在 → **必爆**
    

----------

### 9.2 確認 UART 參數

`stty -F /dev/ttyS9 -a` 

重點檢查：

-   baud rate
    
-   `crtscts` 是否與硬體一致
    

----------

### 9.3 最小測試法（UART）
```
systemctl stop bluetooth
btmon &
btmgmt power on
```
👉 **只要 Reset 沒 event = UART 問題**

----------

## 10. Step 6：USB 專屬除錯流程

### 10.1 USB enumeration

`lsusb
lsusb -t` 

### 10.2 btusb log

`dmesg | grep -i btusb` 

USB 問題通常非常明確：

-   enumeration 失敗
    
-   firmware missing
    
-   device reset loop
    

----------

## 11. Step 7：確認 Data Plane（scan / connect）

### 11.1 Control OK ≠ Data OK

確認：

`btmgmt find` 

-   find 失敗 → controller / firmware
    
-   find 成功，但 profile 不行 → BlueZ / profile
    

----------

### 11.2 ACL data 是否正常

btmon 中是否看到：

`ACL Data RX/TX` 

沒有 ACL：

-   link layer 沒建立
    
-   pairing / encryption 問題
    

----------

## 12. 常見「假象」與真相對照表


| 假象                     | 真相說明                     |
|--------------------------|------------------------------|
| 換 firmware 就好了       | 80% 問題根因在 UART          |
| BlueZ 壞掉               | 多半是 HCI 層異常            |
| 偶爾成功                 | Race condition（時序問題）   |
| USB OK / UART 不 OK      | UART 問題機率 100%           |

----------

## 13. USB 作為「黃金對照組」

如果同一顆 BT chip：

-   USB 正常
    
-   UART 異常
    

👉 **請立刻停止懷疑 firmware**
