
# Broadcom bcmdhd (DHD) Wi-Fi Driver — Data Path: TX, RX, and Flow Control


## 1. 本章定位

在 bcmdhd（DHD）架構中：

- **Control Path**：決定「Wi-Fi 要做什麼」
- **Data Path**：決定「封包能不能順利跑」

實務上常見的問題包括：

- Wi-Fi 已連線，但 **完全沒流量**
- throughput 偶發掉到 0
- resume 後 TX 卡死
- AP mode 下 RX 正常、TX 不動

👉 **這些 9 成都與 data path / flow control 有關**

---

## 2. Data Path 的設計哲學（FullMAC 視角）

### 2.1 Linux 不是 MAC owner

在 bcmdhd 中：

- Linux **不做 rate control**
- Linux **不做 aggregation decision**
- Linux **不決定 TX 時機**

Linux driver 只做三件事：

1. **包裝封包**
2. **依 firmware 回報進行 flow control**
3. **把封包送進 bus layer**

---

### 2.2 TX / RX 與 control path 的關係

```
       ┌──────────┐
       │ cfg80211 │
       └────┬─────┘
            │
    control  │   data
            ▼
    ┌────────────────┐
    │   dhd_linux    │
    └────┬─────┬─────┘
         │     │
         ▼     ▼
  control pkt  data pkt
```

📌 **Event 與 data packet 共用 RX 通道**

---

## 3. TX Path（Host → Dongle）

### 3.1 TX Path 高層流程
```
netdev TX
 └─ ndo_start_xmit()
     └─ dhd_start_xmit()
         ├─ skb 分類（priority / ifidx）
         ├─ 加入 BDC header
         ├─ flow control 判斷
         └─ dhd_bus_txdata()
             └─ SDIO / PCIe
```
---

### 3.2 `dhd_start_xmit()`：TX 的第一關

位置：
- `dhd_linux.c`

核心責任：

- 接收 Linux network stack 的 `skb`
- 對應到 DHD 的 internal queue / ring
- 檢查 flow control 狀態

典型結構（簡化）：

```c
netdev_tx_t dhd_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    if (dhd->pub.txoff)
        return NETDEV_TX_BUSY;

    dhd_tx_prepare(skb);
    dhd_bus_txdata(dhd->bus, skb);
    return NETDEV_TX_OK;
}
```

### 3.3 BDC Header（Broadcom Data Channel）

在送到 dongle 前，bcmdhd 會在 skb 前面加上 **BDC header**：

包含資訊：

-   interface index (ifidx)
    
-   priority / AC
    
-   flags
    

📌 **BDC 是 firmware 判斷封包用途的唯一依據**

----------

## 4. RX Path（Dongle → Host）

### 4.1 RX Path 高層流程
```
bus interrupt / poll
  └─ dhd_bus_rxdata()
      └─ dhd_rx_frame()
          ├─ BDC header parsing
          ├─ 判斷 data vs event ├─ data → netif_receive_skb()
          └─ event → dhd_event_process()
```
----------

### 4.2 `dhd_rx_frame()`：RX 分流點

位置：

-   `dhd_linux.c`
    

責任：

-   拆 BDC header
    
-   判斷封包類型
    
-   決定送往：
    
    -   data path
        
    -   control/event path
        

📌 **Event packet 是「偽裝成 data packet」回來的**

----------

### 4.3 RX 與 NAPI（依 tree / platform）

部分 tree 會使用：

-   interrupt-driven RX
    
-   或 NAPI polling
    

但不論哪種：

👉 **RX backlog 卡住 = event 也會卡住**

----------

## 5. Flow Control

### 5.1 為什麼一定要 flow control？

-   dongle firmware 有有限 buffer
    
-   host 若無限制送封包 → firmware overflow
    
-   結果不是 drop，就是 firmware hang
    

👉 **flow control = firmware 生存機制**

----------

### 5.2 Flow Control 的基本模型
```
Host TX queue
   │
   ▼
[ flow control check ]
   │
   ├─ OK    → send skb
   └─ BLOCK → stop netdev queue
```
----------

### 5.3 Flow Control 的資訊來源

Firmware 會透過：

-   TX completion
    
-   credit 回報
    
-   ring status
    

通知 host：

-   哪些 flow / ring 可以繼續送
    

----------

## 6. Flow Ring / Credit 機制

### 6.1 Flow ring 概念（PCIe 常見）

-   每個 destination / priority 對應一個 flow ring
    
-   firmware 回收 ring entry 才代表「可以再送」
    

位置（依 tree）：

-   `dhd_flowring.c`
    
-   `dhd_msgbuf.c`
    

----------

### 6.2 Flow control 與 netdev queue
```
netif_stop_queue(ndev);
netif_wake_queue(ndev);
```
📌 **現象判斷**

-   netdev queue stopped，但永遠沒 wake  
    ➜ credit 沒回來 or event RX 卡死
    

----------

## 7. SDIO vs PCIe：Data Path 差異

### 7.1 SDIO Data Path 特性

-   transaction-based
    
-   RX/TX aggregation
    
-   latency 高、頻繁 wake/sleep
    

常見問題：

-   aggregation overflow
    
-   RX stuck
    
-   resume 後第一包送不出去
    

----------

### 7.2 PCIe Data Path 特性

-   DMA ring buffer
    
-   completion-based
    
-   高效能，但狀態同步複雜
    

常見問題：

-   ring 不前進
    
-   interrupt lost
    
-   DMA mapping mismatch
    

----------

## 8. Data Path 常見故障模式

### 8.1 已連線，但完全沒流量

檢查點：

-   `dhd->pub.txoff`
    
-   netdev queue 是否 stopped
    
-   flow ring credit 是否歸零
    

----------

### 8.2 TX 偶發卡死

可能原因：

-   firmware flow control bug
    
-   RX path 被堵（event 也一起卡）
    
-   resume 後 flow state 未重設
    

----------

### 8.3 RX 正常、TX 不動（AP mode 常見）

-   AP TX flow ring 被 block
    
-   priority mapping 錯誤
    
-   firmware AP buffer 用盡
    

----------

## 9. Debug Data Path 的實用技巧

### 9.1 必 grep 的關鍵字

-   `txoff`
    
-   `flow`
    
-   `ring`
    
-   `netif_stop_queue`
    
-   `netif_wake_queue`
    

----------

### 9.2 問題定位思維

> 「是 **driver 不送**，還是 **firmware 不收**？」

-   如果 `dhd_start_xmit()` 沒被呼叫 → 上層問題
    
-   如果被呼叫但 queue stopped → flow control
    
-   如果送出但沒 completion → firmware / bus
