
# HCI over UART 深入解析


## 1. 本章定位：Bluetooth 最容易「看起來像 firmware 壞掉」的一層

在實務經驗中：

> **超過一半的 Bluetooth bring-up 失敗，根本不是 firmware 或 BlueZ 問題**  
> 而是 **HCI over UART 的 framing / baud / flow control 出錯**

常見症狀：

-   `hciconfig hci0 up` timeout
    
-   btmon 看到第一個 command，之後全消失
    
-   brcm_patchram_plus 偶爾成功、偶爾失敗
    
-   換 firmware 沒差，換 kernel 版本沒差
    

👉 這一章專門拆解 **UART 層真正會壞的地方**

----------

## 2. HCI over UART 的本質：沒有封包邊界的世界

### 2.1 UART 是「純 byte stream」

UART 的特性：

-   沒有封包邊界
    
-   沒有 CRC
    
-   沒有 retry
    
-   所有 framing 都靠 **軟體協議**
    

對 Bluetooth 而言，這個協議就是 **HCI H4**。

----------

## 3. HCI H4 協議：一切從第一個 byte 開始

### 3.1 H4 Packet Type

每一個 HCI packet 皆以 **1 byte type** 作為開頭：

| Type         | 值    | 說明                         |
|--------------|-------|------------------------------|
| HCI Command  | 0x01  | Host → Controller            |
| ACL Data     | 0x02  | 雙向資料傳輸                 |
| SCO Data     | 0x03  | 語音資料                     |
| HCI Event    | 0x04  | Controller → Host            |


👉 **只要第一個 byte 錯，整個 stream 都會崩**

----------

### 3.2 H4 封包長度完全仰賴 header

以 HCI Command 為例：

`| Type | Opcode (2) | Param Len (1) | Params |` 

ACL packet：

`| Type | Handle (2) | Data Len (2) | Payload |` 

📌 關鍵問題

> UART **不知道** 封包結束在哪  
> parser 必須「完全相信 header」

----------

## 4. hci_uart 架構總覽

### 4.1 關鍵檔案
```
drivers/bluetooth/
├─ hci_uart.c        # HCI UART core
├─ hci_ldisc.c       # TTY line discipline (N_HCI)
```
hci_uart 負責：

-   HCI device lifecycle
    
-   將 HCI packet 交給 HCI core
    
-   protocol abstraction（H4 / BCSP / etc）
    

----------

### 4.2 line discipline（N_HCI）的角色
```
/dev/ttyS9
   │
   └─ N_HCI (hci_ldisc)
         │
         └─ hci_uart
               │
               └─ net/bluetooth/hci_core
```
N_HCI 做的事：

-   接管 tty 的 read/write
    
-   把 byte stream 丟給 hci_uart parser
    

👉 **任何其他 process 開 tty 都會破壞這個模型**

----------

## 5. brcm_patchram_plus vs kernel：為什麼會打架？

### 5.1 兩個「master」搶同一條 tty

典型災難配置：
```
Process A: brcm_patchram_plus
Process B: hci_uart (kernel)
```
兩邊都：

-   設 baud rate
    
-   設 flow control
    
-   送 HCI command
    

結果：

-   framing 混亂
    
-   command/event 對不上
    
-   表現為「玄學不穩」
    

📌 **硬規則**

> 同一時間，只能有一個 entity 控制該 UART

----------

### 5.2 正確策略（只能二選一）

**方案 A：User space 初始化**

-   brcm_patchram_plus 完成 firmware + baud
    
-   再 attach hci_uart
    

**方案 B：Kernel 全權處理**

-   serdev + btbcm
    
-   user space 不碰 tty
    

👉 混用 = 必爆

----------

## 6. baud rate mismatch：最常見、最難一眼看出的錯

### 6.1 mismatch 的真實樣貌

常見錯誤：

-   Host 切到 3M
    
-   Controller 還在 115200（或反過來）
    

後果：

-   byte stream 立刻變亂碼
    
-   parser 讀到錯誤 packet type
    
-   HCI core 再也等不到正確 event
    

----------

### 6.2 btmon 的經典症狀

`> HCI Command: Reset (no event forever)` 

或：

`< HCI Event: Unknown (garbage)` 

👉 **不是 controller 掛掉，是 UART 對話壞了**

----------

### 6.3 下載 firmware 時用高 baud 是風險操作

實務建議：

-   firmware download：115200（穩定）
    
-   運行時再切高 baud（如 3M）
    

因為：

-   firmware download 階段 packet 多、密
    
-   framing error 成本極高
    

----------

## 7. RTS / CTS Flow Control：第二大隱形殺手

### 7.1 軟體有開，硬體沒接

最典型錯誤：

-   `crtscts` = on
    
-   板子根本沒接 RTS/CTS
    

後果：

-   Host 永遠等 CTS
    
-   或 controller TX overflow
    
-   結果 = packet 丟失
    

----------

### 7.2 Flow control 壞掉的表現

-   有些 command 回得來，有些不行
    
-   小 command OK，大 packet（ACL）開始炸
    
-   表現「極不穩定」
    

----------

### 7.3 必做檢查清單

`stty -F /dev/ttyS9 -a` 

確認：

-   baud rate
    
-   `-crtscts` 或 `crtscts` 是否符合硬體
    

----------

## 8. serdev vs line discipline：為什麼 serdev 比較安全

### 8.1 serdev 的優點

-   kernel 單一 owner
    
-   power / clock / reset 整合
    
-   不需 user space 開 tty
    

👉 **更適合 BSP / 量產系統**

----------

### 8.2 line discipline 的風險

-   user space 容易誤觸 tty
    
-   service 啟動順序容易 race
    
-   debug 成本高
    

----------

## 9. UART 層 debug 的「黃金流程」

當你懷疑 UART 層時：

1.  停 bluetoothd
    
2.  確保只有一個 entity 使用 tty
    
    `lsof /dev/ttyS9` 
    
3.  確認 baud / flow
    
4.  開 `btmon`
    
5.  只測 `btmgmt power on`
    

👉 **只要 HCI Reset 沒回 event，就 100% 是 UART 層**
