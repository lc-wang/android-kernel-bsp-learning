
# Broadcom bcmdhd (DHD) Wi-Fi Driver — Debugging Playbook


## 1. 本章定位：為什麼需要 Playbook？

在 bcmdhd（FullMAC）世界中，Wi-Fi 問題常見特徵是：

- **不可重現**
- **重開 Wi-Fi / reboot 就好**
- **log 看起來都正常**
- **cfg80211 state 與實際行為不一致**

原因不是因為你不會 debug，而是因為：

> **問題往往出在「你一開始選錯層級」**

本章提供一套：
- **層級導向（layer-driven）**
- **症狀導向（symptom-driven）**
- **可實際執行的 Debug 決策流程**

---

## 2. Debug 的第一原則：先選對層級

### 2.1 bcmdhd 的五個 Debug 層級
```
[ L1 ] cfg80211 / userspace
[ L2 ] Control Path (ioctl / iovar / event)
[ L3 ] Data Path (TX / RX / flow control)
[ L4 ] Bus Layer (SDIO / PCIe)
[ L5 ] Firmware / NVRAM / PM
```

📌 **錯誤示範**
- 在 L1（cfg80211）找一個其實是 L5（firmware / PM）的問題

📌 **正確示範**
- 先用症狀判斷在哪一層，再深入

---

## 3. 症狀 → 層級 對照表（快速入口）

| 症狀 | 優先懷疑層級 |
|---|---|
| Scan callback 有回，但結果為空 | L2 / L5 |
| 已 connected，但完全沒流量 | L3 / L4 / L5 |
| TX 偶發卡死 | L3 / L4 |
| Resume 後 Wi-Fi 偶爾不動 | L4 / L5 |
| cfg80211 state 明顯錯亂 | L2 / L3 |
| 重開 Wi-Fi 才好 | L5（PM / firmware） |

---

## 4. 標準 Debug 決策流程

### Step 1：cfg80211 是否只是「看起來不對」？

問自己三個問題：

1. firmware 有沒有回 event？
2. data path 有沒有在跑？
3. bus 有沒有真的動？

👉 **只看 cfg80211 state 永遠不夠**

---

### Step 2：Control Path 是否正常？

檢查：

- iovar / ioctl 是否送成功？
- firmware event 是否回來？
- event 是否被正確轉譯？

常見檢查點：

- `dhd_wl_ioctl()`
- `dhd_event_process()`
- `wl_cfg80211_event()`

---

### Step 3：Data Path 是否 forward progress？

檢查：

- `dhd_start_xmit()` 是否被呼叫？
- netdev queue 是否被 stop？
- flow credit / ring 是否前進？

📌 **TX 卡住 ≠ RX 不動，但 RX 卡住 = TX + Control 一起死**

---

### Step 4：Bus 是否在「假醒」狀態？

- SDIO interrupt 是否真的進來？
- PCIe ring index 是否前進？
- DMA / CMD53 是否有 error / retry？

👉 **Bus 問題常被誤判成 data path bug**

---

### Step 5：PM / Firmware 是否不同步？

- firmware 是否還在 sleep？
- WOWLAN 是否影響行為？
- runtime PM / system suspend 是否交錯？

📌 **「待機後才發生」幾乎必是 PM**

---

## 5. 常見 Debug Case 與正確切入點

### Case 1：已連線、有 IP，但 ping 不通

優先順序：

1. L3：TX 是否被 block？
2. L4：bus TX 是否成功？
3. L5：firmware 是否 sleep？

❌ 不要先看 cfg80211

---

### Case 2：Scan 正常 callback，但永遠掃不到 AP

優先順序：

1. L5：regulatory / NVRAM
2. L2：scan event 是否真的回來
3. L1：userspace 是否過濾結果

---

### Case 3：Resume 後偶發 Wi-Fi 死亡

優先順序：

1. L5：firmware PM state
2. L4：bus resume 是否完整
3. L3：flow control 是否 reset

---

## 6. 必備 Debug Instrument

### 6.1 必 grep 關鍵字（全系列通用）

- `txoff`
- `flow`
- `ring`
- `credit`
- `event`
- `suspend`
- `resume`
- `watchdog`

---

### 6.2 建議一定要加的 log

- 每次 `netif_stop_queue()` / `netif_wake_queue()`
- firmware reset start / end
- PM state transition
- watchdog trigger 條件

📌 **沒有 timeline 的 log，等於沒有 log**

---

## 7. Recovery Debug 的現實認知

- recovery 本身就是高風險動作
- recovery 失敗 ≠ recovery 沒做
- 有些 firmware 卡死 **只能 reboot**

👉 **不要過度神話 watchdog / recovery**

