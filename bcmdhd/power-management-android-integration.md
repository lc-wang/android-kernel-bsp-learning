
# Broadcom bcmdhd (DHD) Wi-Fi Driver — Power Management and Android Integration
---

## 1. 本章定位

在 bcmdhd（FullMAC）系統中，**Power Management（PM）是穩定性問題的最大來源之一**：

- 待機後 Wi-Fi 偶發完全不動
- resume 後已連線但沒流量
- 螢幕關閉就斷線，亮屏才恢復
- Android 上「偶爾需要重開 Wi-Fi 才好」

這些問題的共同特徵是：

> **PM 狀態不同步，而不是功能邏輯錯誤。**

本章將拆解：

- bcmdhd 的 PM 模型（runtime / system）
- firmware 與 driver 的 power 分工
- Android userspace 對 Wi-Fi PM 的實際影響
- 常見 PM 故障模式與判斷方式

---

## 2. bcmdhd 的 Power Management 架構總覽

### 2.1 三層 PM 同時存在
```
Android userspace  
	└─ (suspend policy / power hints)  
		Linux kernel PM  
			└─ (runtime PM / system suspend)  
				Wi-Fi firmware PM  
					└─ (power save / WOWLAN / sleep)
```


📌 **任一層狀態不同步，都可能導致 Wi-Fi 行為異常**

---

### 2.2 FullMAC 的 PM 核心現實

- Linux **不控制 RF sleep**
- Linux **不能強制 firmware 醒來**
- Linux 只能「請求」與「配合」

👉 **PM 是協調問題，不是單一模組問題**

---

## 3. Runtime PM（閒置省電）

### 3.1 Runtime PM 的目的

- 在 Wi-Fi 閒置時降低功耗
- 避免頻繁 RF / bus 活動

在 bcmdhd 中，runtime PM 影響：

- bus（SDIO / PCIe）是否可進入低功耗
- firmware 是否允許 power save mode

---

### 3.2 常見 Runtime PM 行為

- 無流量 → firmware 進入 power save
- 有 TX/RX → firmware wake up
- Linux driver 僅追蹤「是否活躍」

📌 **問題關鍵**  
> firmware 已 sleep，但 Linux 仍嘗試送 control / data

---

### 3.3 Runtime PM 常見故障模式

- 第一包 TX 卡住（firmware 尚未 wake）
- RX event 丟失（bus 尚未恢復）
- PM state 永遠卡在「suspended」

---

## 4. System Suspend / Resume（系統睡眠）

### 4.1 Suspend 流程（高層）

```
system suspend
 └─ dhd_suspend()
     ├─ 停止 TX
     ├─ 設定 firmware power save
     ├─ bus suspend
     └─ (optional) enable WOWLAN
 ```    

### 4.2 Resume 流程（高層）
```
system resume
 └─ dhd_resume()
     ├─ bus resume
     ├─ firmware wake
     ├─ restore control state
     └─ resume TX/RX
```
📌 **resume 的順序極其重要**

----------

### 4.3 Resume 常見問題

-   bus 已醒，firmware 未醒
    
-   firmware 已醒，control state 未恢復
    
-   flow control / ring state 未重建
    

👉 **症狀通常是「已連線但沒流量」**

----------

## 5. WOWLAN（Wake on Wireless LAN）

### 5.1 WOWLAN 的角色

WOWLAN 允許：

-   系統 suspend 狀態下
    
-   由 Wi-Fi event（magic packet / pattern）喚醒系統
    

在 bcmdhd 中：

-   WOWLAN **完全由 firmware 實作**
    
-   Linux 只設定 pattern / enable
    

----------

### 5.2 WOWLAN 的現實限制

-   開啟 WOWLAN ≠ firmware 一定穩定
    
-   某些 firmware 在 WOWLAN 下：
    
    -   RX 正常
        
    -   TX 被限制
        
-   resume 後需完整重新初始化 control state
    

📌 **WOWLAN 是 PM 複雜度放大器**

----------

## 6. Android 整合：PM 問題的放大來源

### 6.1 Android userspace 會做什麼？

Android 透過：

-   Wi-Fi HAL
    
-   Power HAL
    
-   ConnectivityService
    

動態改變：

-   power save policy
    
-   roaming 行為
    
-   suspend 條件
    

👉 **Linux driver 並不知道「為什麼」狀態被改**

----------

### 6.2 wakelock（Android 專屬）

-   bcmdhd 可能持有 wakelock
    
-   防止在關鍵時刻進入 suspend
    
-   錯誤使用會導致：
    
    -   永不 suspend（耗電）
        
    -   過早 suspend（Wi-Fi 掛）
        

----------

### 6.3 Android PM 常見陷阱

-   螢幕關閉 → Wi-Fi 進 aggressive power save
    
-   Doze 模式干擾 runtime PM
    
-   userspace 與 kernel PM 狀態不同步
    

----------

## 7. PM × Data / Control Path 的交互影響

### 7.1 PM 與 Control Path

-   control iovar 在 suspend/resume 間送出
    
-   firmware 忽略或丟失
    
-   導致 cfg80211 state 與實際不符
    

----------

### 7.2 PM 與 Data Path

-   TX 在 firmware sleep 時送出
    
-   flow control 永久 block
    
-   RX event 永遠不回
    

📌 **PM 問題經常「假裝成 data path bug」**

----------

## 8. 常見 PM 故障模式

### 8.1 待機後 Wi-Fi 偶發死亡

-   firmware 未正確 wake
    
-   WOWLAN state 未清
    
-   bus state 與 firmware 不一致
    

----------

### 8.2 螢幕關閉就斷線

-   aggressive power save
    
-   userspace policy 強制變更
    
-   firmware roam / disconnect
    

----------

### 8.3 只有 reboot 才能救回

-   recovery path 無法重設 PM state
    
-   firmware PM 狀態卡死
