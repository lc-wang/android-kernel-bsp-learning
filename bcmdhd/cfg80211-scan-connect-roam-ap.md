
# Broadcom bcmdhd (DHD) Wi-Fi Driver — cfg80211 Operations: Scan, Connect, Roam, and AP Mode
---

## 1. 本章定位

在 Linux 無線架構中，**cfg80211 是使用者空間與 driver 的唯一正式介面**。  
但在 bcmdhd（FullMAC）架構下：

> **cfg80211 並不是「控制 Wi-Fi 的核心」，而是「描述結果的翻譯層」。**

本章將說明：

- cfg80211 ops 在 bcmdhd 中實際扮演的角色
- scan / connect / roam / AP mode 的完整 call flow
- 哪些 cfg80211 行為「看起來有效，其實只是被動回報」

---

## 2. cfg80211 在 FullMAC 架構中的真實地位

### 2.1 cfg80211 的原始設計假設（SoftMAC）

cfg80211 假設：

- driver 掌握 MAC state
- driver 能決定 scan / assoc / roam
- cfg80211 是「控制介面」

---

### 2.2 bcmdhd 對 cfg80211 的實際使用方式

在 bcmdhd 中：

| cfg80211 行為 | 實際意義 |
|---|---|
| `scan()` | 請 firmware 掃描 |
| `connect()` | 請 firmware 嘗試 join |
| `disconnect()` | 請 firmware 中斷 |
| `roam` | firmware 主動行為 |
| AP mode | firmware 切換角色 |

👉 **cfg80211 只負責「請求」與「回報」，不負責「決策」**

---
## 3. Scan 流程

### 3.1 Scan 的完整 call flow

```
cfg80211_ops->scan
 └─ wl_cfg80211_scan()
     └─ wl_do_escan()
         └─ wldev_iovar_setbuf("escan")
             └─ dhd_wl_ioctl()
                 └─ firmware scan engine
```

### 3.2 Scan result 如何回到 Linux

-   firmware 掃描到 BSS
    
-   逐筆透過 **event packet** 回傳
    
-   driver 呼叫：
    

`cfg80211_inform_bss()` 

-   scan complete event → 結束 scan
    

📌 **關鍵觀念**

> cfg80211 的 BSS table ≠ firmware 的真實狀態  
> 只是「曾經回報過的結果」

----------

### 3.3 常見 scan 問題誤區

-   scan callback 有回來 ≠ firmware 掃到 AP
    
-   scan result 為空 ≠ scan 失敗（可能被 regulatory 擋）
    

----------

## 4. Connect（STA Join）流程

### 4.1 Connect 的控制流程
```
cfg80211_ops->connect
 └─ wl_cfg80211_connect()
     ├─ 設定 auth / akm / wsec
     ├─ 設定 PMK（必要時）
     └─ wldev_iovar_setbuf("join")
```
📌 **Linux 不會同步等待結果**

----------

### 4.2 Connect 結果的來源

-   firmware 嘗試 association
    
-   成功 / 失敗 → event 回報
    
-   driver 轉譯為：
    

`cfg80211_connect_result()` 

----------

### 4.3 常見 connect 問題

-   join iovar 成功，但永遠等不到 event
    
-   firmware 因 state 不允許而忽略 join
    
-   NVRAM / regulatory 導致 join 被拒
    

----------

## 5. Disconnect 與 Link State

### 5.1 Disconnect 行為

-   cfg80211 `disconnect()`
    
-   driver 發送 disconnect iovar
    
-   firmware 回報 link down event
    

----------

### 5.2 非預期斷線

-   AP deauth
    
-   roaming fail
    
-   power save timeout
    

👉 **所有斷線都以 event 為準**

----------

## 6. Roaming

### 6.1 bcmdhd 的 roam 模型

-   roaming 完全由 firmware 決定
    
-   host 只接收 roam event
    

----------

### 6.2 Roam event 流程
```
firmware roam
 └─ WLC_E_ROAM
     └─ wl_cfg80211_event()
         └─ cfg80211_roamed()
```
📌 **cfg80211 無法阻止 firmware roam**

----------

### 6.3 Roam 相關誤解

-   cfg80211 無法設定 roam threshold
    
-   roam policy 大多是 firmware 私有邏輯
    
-   Linux 端只能開 / 關 roam
    

----------

## 7. AP Mode

### 7.1 啟動 AP 的流程
```
cfg80211_ops->start_ap
 └─ wl_cfg80211_start_ap()
     ├─ 設定 beacon
     ├─ 設定 security
     ├─ 設定 channel
     └─ 啟動 firmware AP mode
```
----------

### 7.2 AP mode 的實際控制權

| 項目          | 控制者   |
|---------------|----------|
| Beacon        | Firmware |
| Association   | Firmware |
| Rate control  | Firmware |
| Power save    | Firmware |


👉 **Linux 只是設定者，不是 AP controller**

----------

### 7.3 AP mode 常見問題

-   AP 起來但 client 掃不到
    
-   client 連上但 throughput 極低
    
-   AP 在 DFS channel 行為異常
    

----------

## 8. cfg80211 與 firmware state 不同步的問題

### 8.1 為什麼會不同步？

-   event 丟失
    
-   RX path 被 flow control 卡住
    
-   firmware reset 未同步通知
    

----------

### 8.2 常見症狀

-   cfg80211 顯示 connected，但實際沒流量
    
-   cfg80211 顯示 disconnected，但 firmware 還在送封包
    

📌 **cfg80211 是「觀察者」，不是「事實來源」**

----------

## 9. Debug cfg80211 × bcmdhd

### 9.1 不要只看 cfg80211 state

請同時檢查：

-   firmware event log
    
-   data path 是否正常
    
-   bus layer 是否卡住
    

----------

### 9.2 Debug 問題時的正確順序

1.  firmware 是否收到指令？
    
2.  firmware 是否回 event？
    
3.  driver 是否正確轉譯 event？
    
4.  cfg80211 是否正確更新 state？

