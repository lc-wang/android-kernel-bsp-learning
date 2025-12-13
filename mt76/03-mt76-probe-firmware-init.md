
第 3 章：Probe、Firmware 載入與 MCU 初始化流程*
----------

## 3.1 本章目標與閱讀指引

本章聚焦於 **mt76 driver 在裝置被 kernel 偵測後，到 Wi-Fi 硬體真正可用之前** 的完整流程，包含：

-   PCIe / USB / SDIO 裝置 probe
-   `struct mt76_dev` 與 bus abstraction 建立
-   firmware（FW）載入流程
-   MCU（Micro Controller Unit）初始化
-   WFDMA / RX / TX path 啟動前置條件
    

**這一章是理解後續 DMA / TX / RX / MCU command 的關鍵地基**。

----------

## 3.2 Linux driver probe 的角色定位（總覽）

在 Linux driver 模型中，`probe()` 的責任是：

1.  **確認裝置存在且可用**
2.  **配置最小可運作硬體狀態**
3.  **建立 driver 的軟體物件**
4.  **將裝置註冊到上層 subsystem（mac80211）**
    

對 mt76 而言，probe 並不只是「掛上 netdev」，而是：
> **完成 Wi-Fi SoC 的 SoC-level bring-up**

----------

## 3.3 mt76 driver 的 probe 分層設計

### 3.3.1 bus-specific probe（第一層）

不同匯流排有不同的 probe 入口：

| 匯流排 | Probe 函式（範例）        |
|--------|---------------------------|
| PCIe   | mt76_pci_probe()          |
| USB    | mt76u_probe()             |
| SDIO   | mt76s_probe()             |


這一層負責：

-   啟用裝置（PCI BAR / USB endpoint / SDIO func）
-   建立 `struct mt76_dev`
-   初始化 **bus ops**
-   設定 IRQ / DMA capability
    
```c
struct mt76_dev {
    struct device *dev;
    const struct mt76_bus_ops *bus;
    void __iomem *mmio;
    ...
};
    ...
};` 
```
👉 **關鍵概念**：  
mt76 從一開始就將「匯流排差異」封裝在 `mt76_bus_ops`。

----------

### 3.3.2 mt76 core initialization（第二層）

完成 bus probe 後，會進入 mt76 core 初始化流程：
```yaml
mt76_alloc_device()
  ├─ 初始化 spinlock / mutex
  ├─ 建立 workqueue
  ├─ 設定 DMA ops
  └─ 建立 NAPI context
```
此階段 **尚未啟用 RX/TX**，僅建立「可配置的軟體骨架」。

----------

## 3.4 firmware 載入流程（mt76_connac 系列）

### 3.4.1 為什麼需要 firmware？

MediaTek Wi-Fi 晶片為 **Hybrid architecture**：

-   **Host driver（mt76）**
    -   管理 DMA、mac80211、Linux 介面 
-   **On-chip MCU + firmware**
    -   管理 PHY / RF / rate / power / calibration
    -   執行 timing-critical 任務

👉 mt76 driver **沒有**直接操作 PHY，而是透過 MCU command。

----------

### 3.4.2 firmware 檔案位置與命名

Firmware 來自 `linux-firmware` 專案，常見位置：
```yaml
/lib/firmware/mediatek/
  ├─ mt7915_rom_patch.bin
  ├─ mt7915_ram.bin
  ├─ mt7921_fw.bin
  ├─ mt7996_fw.bin
```
Probe 時會呼叫：
```c
request_firmware(&fw, fw_name, dev);
```
若 firmware 缺失，probe **會直接失敗**。

----------

### 3.4.3 ROM patch vs RAM firmware

多數 mt79xx 晶片有 **兩階段 firmware**：

1.  **ROM patch**
    -   修補晶片內建 ROM bug
    -   非常早期載入
2.  **RAM firmware**
    -   主執行邏輯
    -   支援 MCU command / events
```yaml
Host
 └─ upload ROM patch
     └─ reset MCU
         └─ upload RAM firmware
             └─ MCU ready
```
----------

## 3.5 MCU 初始化流程（Connac / UniCmd）

### 3.5.1 MCU 初始化順序

以 mt7915 / mt7921 / mt7996 為例，流程大致為：
```yaml
1. MCU reset
2. Firmware download
3. MCU start
4. Wait MCU ready event
5. Query firmware version
6. Initialize MCU queues
```
對應程式碼多集中於：

-   `mt76_connac_mcu.c`
-   `mt7915/mcu.c`
-   `mt7996/mcu.c` 
----------

### 3.5.2 MCU event 同步機制

MCU 與 host 之間為 **非同步通訊**：

-   Host 發送 command
-   MCU 回傳 event
-   Driver 使用 completion / wait_event
    
```c
wait_for_completion_timeout(&dev->mcu.cmpl, timeout);
```
若 MCU 未回應：
-   probe 失敗
-   裝置無法上線
-   常見於 firmware mismatch

----------

## 3.6 mac80211 註冊（但尚未啟用資料流）

在 firmware 與 MCU ready 後，driver 才會：

`ieee80211_register_hw(hw);` 

此時：

-   mac80211 **知道這張卡存在** 
-   但 **RX/TX 尚未啟用**
-   DMA ring 仍可能是 disabled 狀態
    

真正開始資料流，是在 **interface up + channel set** 之後。

----------

## 3.7 Probe 階段常見失敗類型

### 3.7.1 firmware load failed

`Direct firmware load failed with error -2` 

原因：

-   `linux-firmware` 太舊
-   檔名不匹配
-   rootfs 未安裝 firmware
----------

### 3.7.2 MCU not responding

`MCU response timeout` 

可能原因：
-   firmware 與 driver 版本不相容
-   ROM patch 未正確載入
-   bus（PCIe/USB）reset 問題
    

----------

### 3.7.3 Probe 成功但無法掃描

多半是：
-   EEPROM / EFUSE 尚未正確初始化
-   regulatory / power table 尚未設定
-   RX path 尚未 enable（DMA 尚未啟動）

