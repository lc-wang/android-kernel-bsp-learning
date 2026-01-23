
# Broadcom bcmdhd (DHD) Wi-Fi Driver — Concurrency, Watchdog, and Recovery
---

## 1. 本章定位

在 bcmdhd（FullMAC）中，**最難 debug 的問題不是功能錯誤，而是「偶發卡死」**：

- 已連線、已拿 IP，但突然 **TX 停止**
- resume 後 Wi-Fi 偶爾完全不動
- 長時間跑流量後 firmware 無回應
- watchdog 有跑，但 recovery 沒發生（或發生後更糟）

這些問題的共同點是：
> **並行（concurrency）＋ 狀態不同步 ＋ firmware 黑盒**

本章將拆解：

- bcmdhd 的並行模型（context / locks / workqueue）
- watchdog 的檢查邏輯與觸發條件
- reset / recovery 的實際流程與失敗模式

---

## 2. bcmdhd 的並行模型總覽

### 2.1 同時存在的執行 context

bcmdhd 不是單一執行緒模型，實際同時存在：

| Context | 來源 |
|---|---|
| Process context | cfg80211 ops、ioctl |
| Softirq / TX | `ndo_start_xmit()` |
| IRQ | SDIO / PCIe interrupt |
| Workqueue | event / deferred work |
| Timer | watchdog |

📌 **任何共享狀態，幾乎都可能被多個 context 同時碰觸**

---

### 2.2 主要共享狀態

- `dhd_pub_t`
- flow control state
- firmware up/down 狀態
- bus state（sleep / wake / reset）

---

## 3. Locking 與同步策略

### 3.1 常見同步原語

在 bcmdhd 中會看到：

- `spinlock`
- `mutex`
- `atomic`
- completion
- wait queue

📌 **鎖粒度不小，且跨層使用**

---

### 3.2 典型高風險區域

#### TX path × flow control
- TX 在 softirq
- flow credit 在 IRQ / RX
- credit state 不一致 → TX 永久停住

#### Event handling × cfg80211
- RX interrupt 收 event
- workqueue 處理 event
- cfg80211 state 在 process context

---

### 3.3 常見 concurrency bug 型態

- double stop queue（stop 了但沒人 wake）
- state flag race（up / down 不一致）
- recovery 與正常路徑同時執行

---

## 4. Watchdog：bcmdhd 的健康檢查機制

### 4.1 Watchdog 的目的

Watchdog 並不是效能優化工具，而是：

- 偵測 firmware 是否「還活著」
- 偵測 TX/RX 是否有 forward progress
- 決定是否觸發 recovery

---

### 4.2 Watchdog 的執行來源

- 以 timer 或 delayed work 形式執行
- 週期性（數百 ms ～ 數秒）

你會在這些地方看到：

- `dhd_watchdog()`
- `dhd_health_check()`

---

### 4.3 Watchdog 常見檢查項目

- TX counter 是否前進
- RX counter 是否更新
- firmware heartbeat
- bus state 是否異常

📌 **Watchdog 只能看到「症狀」，不知道「原因」**

---

## 5. Recovery 流程

### 5.1 高層 recovery 流程

```
watchdog detect stall
 └─ stop netdev
     └─ block TX
         └─ reset dongle
             ├─ bus reset
             ├─ firmware reload
             ├─ nvram reload
             └─ reinit cfg80211
```

### 5.2 Recovery 涉及的關鍵動作

-   停止 TX queue
    
-   清空 flow ring / credits
    
-   reset bus
    
-   重新 download firmware / NVRAM
    
-   重新註冊 netdev / wiphy
    

----------

## 6. Recovery 常見失敗模式

### 6.1 Recovery 沒被觸發

可能原因：

-   watchdog 條件太寬鬆
    
-   RX 還在進（但 TX 已死）
    
-   counter 沒歸零
    

----------

### 6.2 Recovery 被觸發，但沒救回來

-   firmware reset 失敗
    
-   bus 沒完全 reset
    
-   舊 state 沒清乾淨（flow ring / flags）
    

----------

### 6.3 Recovery 與正常路徑 race

-   recovery 執行時，cfg80211 仍在下指令
    
-   TX/RX 同時進入 reset path
    
-   導致二次 crash 或永久 dead state
    

----------

## 7. SDIO vs PCIe：Recovery 的差異

### 7.1 SDIO Recovery 特性

-   reset 成本低
    
-   timing / sleep 狀態高度敏感
    
-   常見「reset 後第一包送不出去」
    

----------

### 7.2 PCIe Recovery 特性

-   reset 成本高
    
-   ring 需完整重建
    
-   DMA state 若殘留，後果嚴重
    

----------

## 8. Debug Concurrency / Watchdog 的實務方法

### 8.1 建議觀察順序

1.  TX 是否真的停了？
    
2.  RX / event 是否還在？
    
3.  watchdog 是否有觸發？
    
4.  recovery 是否完整跑完？
    

----------

### 8.2 實用 debug 手段

-   在 watchdog 加詳細 counter log
    
-   記錄每次 netif_stop / wake
    
-   標記 firmware reset 的開始與結束
    

📌 **沒有完整 timeline，很難 debug concurrency 問題**
