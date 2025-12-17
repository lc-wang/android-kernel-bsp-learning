
# Linux Bus Subsystems（匯流排子系統）

> 本章定位：
> 
> -   站在 **Linux / BSP Engineer** 視角，理解各種 bus 子系統如何在 driver model 下運作
>     
> -   說清楚 platform / PCI / USB / I2C / SPI 的差異與共通點
>     
> -   能實際用於 debug：driver 不 probe、順序錯誤、DTS 看起來對但裝置不起來
>     

----------

## 1. 為什麼要理解 Bus Subsystem

在 Linux 中，driver **不是直接跟硬體綁定**，而是透過 bus 進行配對。

很多 BSP 問題表面上看起來像是：

-   driver bug 
-   DTS 寫錯
    

但實際上常常是：

> **對 bus 的角色與限制理解不足。**

----------

## 2. Bus 在 Driver Model 中的角色

在 `device-driver-model.md` 中，我們提過三個核心結構：

```text
struct device
struct device_driver
struct bus_type
```

Bus 的責任不是資料傳輸細節，而是：

-   device 與 driver 的 match 規則
-   probe / remove 的時機
-   裝置生命週期的邊界

👉 **Bus 是 driver model 的調度層。**

----------

## 3. Platform Bus：SoC 世界的核心

### 3.1 Platform bus 是什麼

-   不可熱插拔
-   裝置來自 DTS / ACPI
-   幾乎所有 SoC IP 都掛在 platform bus
    

常見裝置：

-   UART
-   I2C controller
-   SPI controller
-   display / multimedia IP

----------

### 3.2 Platform driver 的特性

-   初始化順序高度依賴 DTS
-   大量依賴 clock / reset / regulator
-   probe defer 非常常見    

👉 **大多數 BSP 問題都發生在 platform bus。**

----------

## 4. PCI Bus：枚舉與資源分配

### 4.1 PCI 的關鍵特性

-   硬體自我描述（configuration space）
-   kernel 枚舉裝置
-   BAR / IRQ 由系統分配

----------

### 4.2 PCI driver 行為特徵

-   probe 順序相對穩定 
-   資源衝突較少
-   熱插拔支援良好
    

👉 **PCI 問題多半不是 DTS，而是 driver 或 firmware。**

----------

## 5. USB Bus：動態與使用者空間互動

### 5.1 USB 的特殊性

-   完全熱插拔
-   裝置在 runtime 出現
-   user space 影響大

----------

### 5.2 USB driver debug 心法

-   確認 enumeration 是否完成
-   區分 host / gadget 模式
-   注意 power / reset 行為

----------

## 6. I2C / SPI Bus：控制器與裝置的分離

### 6.1 Controller vs Device

在 I2C / SPI 中：

-   controller 本身是 platform device
-   外掛裝置掛在 controller 之下

----------

### 6.2 DTS 常見錯誤

-   address / chip-select 錯誤
-   pinctrl 設定不完整
-   bus frequency 不符硬體需求

👉 **裝置不起來，多半是 bus 描述問題。**

----------

## 7. Bus 與 Power / Reset / Clock 的關係

-   bus 本身不管理 power 
-   但會決定裝置何時 probe
    
如果：
-   clock 尚未 ready
-   reset 尚未 release

probe 就會失敗或 defer。

----------

## 8. 常見 BSP 問題分類

| 現象             | 可能原因                              |
|------------------|---------------------------------------|
| Driver 不 probe  | Bus / Device match 失敗               |
| Probe defer      | Clock / Regulator 尚未 Ready          |
| 裝置偶爾失效     | Power sequencing 問題                 |
