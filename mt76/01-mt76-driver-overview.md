
# **第 1 章：mt76 家族與驅動定位**

## 1.1 mt76 是什麼？

mt76 是 MediaTek 開源 Wi-Fi 的 SoftMAC 驅動，主線 Linux **唯一官方支援的 MTK mac80211 Wi-Fi 驅動**。

屬性：
-   **SoftMAC**（所有 MLME 都由 mac80211 控制）
-   **非 FullMAC**（不像 ath10k、iwlwifi）
-   **使用 Unified MCU Command 與韌體溝通**


### 1.2 支援晶片家族

以下整理 mt76 驅動目前支援的晶片家族與其對應規格：

| 家族              | Wi-Fi 代數             | 介面             | 代表晶片 / 平台            |
|------------------|------------------------|------------------|----------------------------|
| **mt76x2**        | 802.11ac               | PCIe             | MT7612                     |
| **mt7603**        | 802.11n                | PCIe             | MT7603                     |
| **mt7615**        | 802.11ac Wave2         | PCIe             | MT7615                     |
| **mt7915 / mt7916** | 802.11ax (Wi-Fi 6)     | PCIe             | Filogic 830                |
| **mt7921 (E/U/SDIO)** | 802.11ax               | PCIe / USB / SDIO | MT7921K                    |
| **mt7996**        | 802.11be (Wi-Fi 7)     | PCIe             | Filogic 880                |
| **mt7981 / mt7986** | SoC 內建 AX             | SoC (內建 MAC/PHY) | OpenWrt 熱門平台            |


## 1.3 SoftMAC 架構定位

SoftMAC 驅動的責任清楚：

使用者 → nl80211 → cfg80211 → mac80211 → 驅動 → MCU → Wi-Fi HW

mt76 位於最底層：
-   處理 DMA/TX/RX
-   MCU 指令
-   EEPROM/EFUSE
-   PHY/RF 參數
    
mac80211 則負責：
-   STA 建立/刪除
-   AP beacon 設定
-   Channel switch
-   AMPDU/AMSDU
