
# Broadcom bcmdhd (DHD) Wi-Fi Driver — Bus Layer: SDIO vs PCIe


## 1. 本章定位

在 bcmdhd 架構中，**Bus Layer 是最貼近硬體、也最容易導致系統不穩定的層級**。  
同一套 DHD core，換成不同 bus（SDIO / PCIe），行為、效能、debug 難度都會出現巨大差異。

本章目標：

- 清楚比較 **SDIO 與 PCIe 的設計取向**
- 建立 **bring-up 與故障定位的判斷模型**

---

## 2. Bus Layer 在 DHD 中的角色

### 2.1 Bus Layer 的責任邊界

Bus layer 負責：

- 與硬體介面直接互動（SDIO / PCIe）
- 封裝與傳送：
  - control packet（ioctl / iovar）
  - data packet（TX / RX）
- firmware download / reset / power state 切換

Bus layer **不負責**：

- MAC / MLME 邏輯
- cfg80211 語意
- flow control policy（但要回報狀態）

---

### 2.2 共通的 Bus 抽象介面

不論 SDIO 或 PCIe，DHD core 只透過抽象 API 呼叫：

```
dhd_bus_txdata()
dhd_bus_txctl()
dhd_bus_rxctl()
dhd_bus_start()
dhd_bus_stop()
```
👉 差異全部藏在 bus-specific 檔案中


## 3. SDIO Bus（`dhd_sdio.c`）

### 3.1 SDIO 的設計特性

SDIO 是 **transaction-based** 介面：

-   CMD52：register read/write
    
-   CMD53：data transfer（block / byte mode）
    
-   無真正 DMA ring
    
-   高度依賴 **aggregation** 與 **timing**
    

特性總結：

項目

SDIO

傳輸模型

Transaction

效能

中

延遲

高

CPU 負擔

高

Debug 難度

中

穩定性風險

高（PM / timing）

----------

### 3.2 SDIO 資料流概觀
```
Host
 └─ CMD53 write/read
     └─ SDIO function
         └─ Dongle firmware
```
-   TX：Host 主動 push
    
-   RX：依 interrupt / polling 讀回
    

----------

### 3.3 Aggregation：效能與災難的分水嶺

SDIO 為了效能，會：

-   將多個 packet aggregation 成一筆 CMD53
    
-   RX/TX 都可能 aggregation
    

問題點：

-   aggregation size 過大 → buffer overflow
    
-   aggregation timing 不佳 → latency 飆高
    
-   resume 後 aggregation state 錯亂 → RX 卡死
    

📌 **SDIO 的問題 8 成來自 aggregation 與 power transition**

----------

### 3.4 SDIO 常見故障模式

#### 1) Resume 後 Wi-Fi 完全沒反應

-   SDIO function 未正確 wake
    
-   firmware 還在 sleep
    
-   RX interrupt 沒再進來
    

#### 2) 偶發 timeout / data corruption

-   CMD53 retry
    
-   block size mismatch
    
-   host timing 與 firmware 不同步


## 4. PCIe Bus（`dhd_pcie.c` / `dhd_msgbuf.c`）

### 4.1 PCIe 的設計特性

PCIe 是 **DMA-based ring architecture**：

-   Host 與 firmware 共用 memory
    
-   以 ring buffer 溝通
    
-   interrupt + doorbell 機制

### 特性總結（PCIe）

| 項目         | PCIe        |
|--------------|-------------|
| 傳輸模型     | DMA Ring    |
| 效能         | 高          |
| 延遲         | 低          |
| CPU 負擔     | 低          |
| Debug 難度   | 高          |
| 穩定性風險   | Ring 同步問題 |



### 4.2 PCIe 資料流概觀

`Host memory  (TX/RX rings) ⇄ DMA
Dongle firmware` 

-   TX：填 ring entry → doorbell
    
-   RX：firmware 填 completion → interrupt
    

----------

### 4.3 msgbuf Protocol（核心）

PCIe bcmdhd 使用 **msgbuf protocol**：

-   TX ring
    
-   RX ring
    
-   completion ring
    
-   event ring
    

每一種 ring 都有：

-   write index
    
-   read index
    
-   credit / quota
    

📌 **任一 ring 停止前進 = 整個 Wi-Fi 停擺**

----------

### 4.4 PCIe 常見故障模式

#### 1) TX ring 不前進

-   firmware 不回 completion
    
-   interrupt lost
    
-   doorbell 未觸發
    

#### 2) RX event 卡住

-   completion ring 塞滿
    
-   RX interrupt 被 mask
    
-   memory corruption

## 5. SDIO vs PCIe：實務比較

| 面向           | SDIO        | PCIe        |
|----------------|-------------|-------------|
| Bring-up 難度  | 低          | 中          |
| 效能調校       | 困難        | 彈性高      |
| Resume 穩定性  | 易出問題    | 較佳        |
| Debug 透明度   | 較高        | 較低        |
| 大流量表現     | 差          | 佳          |



📌 **選擇原則**

-   IoT / 低功耗：SDIO
    
-   高 throughput / AP / STA heavy load：PCIe


## 6. Firmware Download 與 Reset 差異

### 6.1 SDIO

-   透過 CMD53 寫入 firmware
    
-   時間長、易受 timing 影響
    
-   reset 成本低
    

### 6.2 PCIe

-   透過 memory window / BAR
    
-   較快
    
-   reset 成本高（需重建 ring）
    

----------

## 7. Debug Bus Layer 的實戰指引

### 7.1 SDIO Debug Checklist

-   SDIO interrupt 是否進來
    
-   CMD53 retry / error count
    
-   RX aggregation size
    
-   resume 後第一包是否成功
    

### 7.2 PCIe Debug Checklist

-   ring index 是否前進
    
-   completion 是否回來
    
-   interrupt 是否觸發
    
-   DMA mapping 是否正確
    

----------

## 8. 問題定位快速判斷法

> **「看起來像 data path 問題，實際是 bus 卡住」**

-   TX 送不出去？
    
    -   看 bus TX 是否成功
        
-   RX 沒 event？
    
    -   看 bus RX / interrupt
        
-   resume 後死？
    
    -   先懷疑 bus power state
