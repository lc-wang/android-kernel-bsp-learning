
# Broadcom bcmdhd (DHD) Wi-Fi Driver — Control Path: ioctl, iovar, and Event Flow

## 1. 本章定位

在 bcmdhd（DHD）架構中：

- **資料平面（Data Path）**：負責搬封包
- **控制平面（Control Path）**：負責「指揮 firmware 做事」

而 **90% 的 Wi-Fi 行為問題（scan 失敗、連線異常、roam 怪異）  
其實都出在 control path，而不是 data path。**

本章將拆解：

- ioctl 與 iovar 的差異與用途
- cfg80211 → DHD → firmware 的實際呼叫路徑
- firmware event 如何回到 Linux 並影響系統狀態

---

## 2. Control Path 的核心設計哲學

### 2.1 Host 不做決策，只做轉送

在 FullMAC 模型中：

- Linux **不維護 MAC state machine**
- Linux **不判斷 scan / assoc / roam 成敗**
- Linux **不執行連線策略**

👉 Host 的角色只有三個：
1. **送指令**
2. **收事件**
3. **轉譯成 cfg80211 語意**

---

### 2.2 Control Path 的三個元件
```
cfg80211 ops
│
▼
ioctl / iovar (Host → Firmware)
│
▼
Firmware execution
│
▼
Event packet (Firmware → Host)
```
---

## 3. ioctl 與 iovar：兩種控制介面

### 3.1 wl ioctl（舊式控制介面）

特性：

- 使用數值型 command ID
- 固定格式
- 擴充性差

典型呼叫：

```c
dhd_wl_ioctl(dhd_pub, cmd, buf, len, set);
```

用途：

-   舊功能
    
-   相容性保留


### 3.2 iovar（主流、也是最重要的介面）

**iovar = I/O Variable**

-   以「字串名稱」識別功能
    
-   payload 格式彈性
    
-   幾乎所有新功能都用 iovar
    

範例：

`wldev_iovar_setbuf(dev, "country", &ccode, sizeof(ccode), buf, buflen);` 

常見 iovar：

| 類型      | iovar 範例              |
|-----------|-------------------------|
| Scan      | escan, scan             |
| Connect   | join, ssid              |
| Security  | wpa_auth, wsec          |
| Power Mgmt| mpc, keep_alive         |
| AP        | bss, up                 |


📌 重要事實

iovar 的「語意與行為」完全定義在 firmware 裡
driver 只是把 name + payload 送出去

## 4. cfg80211 → iovar 的實際呼叫路徑

### 4.1 Scan 的完整 control flow
```
cfg80211_ops->scan
  └─ wl_cfg80211_scan()
      └─ wl_do_escan()
          └─ wldev_iovar_setbuf("escan")
              └─ dhd_wl_ioctl()
                  └─ dhd_prot_ioctl()
                      └─ dhd_bus_txctl()
                          └─ firmware
```

對應檔案：

-   `wl_cfg80211.c`
    
-   `wldev_common.c`
    
-   `dhd_common.c`
    
-   `dhd_sdio.c` / `dhd_pcie.c`

###  4.2 Connect（join）流程
```
cfg80211_connect()
  └─ wl_cfg80211_connect()
      ├─ set auth / akm / wsec
      ├─ set PMK (必要時)
      └─ wldev_iovar_setbuf("join")
 ```

📌 **重點**

-   Linux 不等待「結果」
    
-   真正結果由 **event** 回報

## 5. dhd_common.c：Control Path 核心
### 5.1 ioctl / iovar 的統一入口
```
int dhd_wl_ioctl(dhd_pub_t *dhdp, int cmd, void *buf, int len, bool set)
````

職責：

-   command 封裝
    
-   protocol header 填寫
    
-   呼叫 bus layer 傳送 control frame

### 5.2 protocol layer（與 bus 無關）
```
dhd_prot_ioctl()
```

-   不知道是 SDIO / PCIe
    
-   只負責「邏輯格式」


### 5.3 bus layer 的 control 傳送

-   SDIO：CMD52 / CMD53
    
-   PCIe：msgbuf / DMA
    

----------

## 6. Firmware Event：真正的「狀態來源」

### 6.1 為什麼 event 這麼重要？

在 FullMAC 架構中：

-   **Event = 真實世界**
    
-   ioctl / iovar 只是「請求」
    

如果：

-   指令送成功
    
-   但事件沒回來
    

👉 **等同於什麼都沒發生**

----------

### 6.2 Event packet 的來源

-   event 是 **從 RX data path 回來**
    
-   與一般資料封包共用通道
    
```
RX packet
  ├─ normal data
  └─ event packet
```
----------

### 6.3 Event 判斷與解析流程
```
dhd_rx_frame()
  └─ dhd_event_process()
      └─ wl_cfg80211_event()
          └─ cfg80211_*()
```
----------

### 6.4 常見 Event 類型

| Event                 | 意義說明              |
|-----------------------|-----------------------|
| WLC_E_SCAN_COMPLETE   | 掃描完成              |
| WLC_E_ASSOC           | Association 完成      |
| WLC_E_LINK            | Link up / down        |
| WLC_E_DEAUTH          | 被 AP deauth / 踢除   |
| WLC_E_ROAM            | Firmware 觸發漫遊     |


📌 **關鍵觀念**

> cfg80211 的狀態完全取決於 event  
> 不是取決於你「送了什麼指令」

----------

## 7. Event 與 cfg80211 的對應關係

### 7.1 Link / Disconnect
```
cfg80211_connect_result()
cfg80211_disconnected()
```
### 7.2 Roam

`cfg80211_roamed()` 

### 7.3 Scan result

-   BSS entry 由 event 逐筆回報
    
-   scan complete event 結束流程
    

----------

## 8. Control Path 常見問題模式

### 8.1 指令送成功，但 Wi-Fi 沒動

可能原因：

-   firmware 忽略指令（狀態不允許）
    
-   iovar sequence 錯誤
    
-   前一個動作未完成
    

----------

### 8.2 Scan / connect 偶發失敗

-   event 丟失
    
-   RX 被 flow control 卡住
    
-   firmware 忙於 roam / power transition
    

----------

### 8.3 Resume 後 control path 失效

-   firmware state 與 host 不同步
    
-   control channel blocked
    
-   PM iovar 未正確重設
