# BSP I2C Interface（I2C 裝置整合實務）

> 本章定位：
>
> -   站在 **BSP / SoC Bring-up Engineer** 視角，理解 I2C 在實際系統中的整合方式
>
> -   不講 protocol waveform，而是聚焦 **為什麼 I2C 裝置常常「看起來正常但實際不工作」**
>
> -   能實際用於 debug：sensor 讀不到、資料錯誤、偶發 NACK、resume 後失效
>

----------

## 1. 為什麼 I2C 是 BSP 最容易踩雷的介面

I2C 在 BSP 世界中有幾個致命特性：

-   幾乎所有外部裝置都用它（sensor / PMIC / touch / codec）
-   protocol 簡單，**但系統依賴非常多**
-   driver 往往「probe 成功」，卻在 runtime 才壞


👉 **多數 I2C 問題不是 driver bug，而是 BSP 整合問題。**

----------

## 2. I2C Interface ≠ I2C Bus

在 linux 中談的是 **I2C bus subsystem**，
但在 BSP 層，面對的是：

> 一組實體腳位 + clock + 電源 + 板子連線。

因此必須把概念拆開：

| 層級            | 角色說明                     |
|-----------------|------------------------------|
| I2C bus         | Driver model 與裝置 match    |
| I2C interface   | SoC 腳位設定與實體硬體連接   |

👉 BSP debug 時，**interface 永遠比 bus 更重要**。

----------

## 3. I2C 架構：Controller vs Device

### 3.1 I2C Controller（SoC 內部）

-   通常是 platform device
-   需要：
    -   clock
    -   reset
    -   pinmux

如果 controller 本身不穩：
-   底下所有 device 都會出問題

----------

### 3.2 I2C Device（外掛裝置）

-   掛在 controller 之下
-   依賴 controller 正常工作

常見錯誤心態：

> 一直改 device driver，卻忽略 controller 狀態
----------

## 4. DTS 中 I2C 最常出錯的地方

### 4.1 pinctrl
-   SDA / SCL 沒 mux 對
-   pull-up 設定錯誤

結果：
-   probe 可能成功
-   但 transaction 不穩定
----------

### 4.2 clock 與 bus speed
-   clock 未 enable
-   bus frequency 過高

結果：
-   偶發 NACK
-   讀值錯誤

----------

### 4.3 address / reg

-   7-bit / 10-bit 搞混
-   與板子實際接線不符
----------

## 5. Power 與 Reset：常被忽略的關鍵

### 5.1 裝置其實沒上電
-   regulator 宣告存在
-   但 enable 時機錯

結果：
-   probe 成功
-   第一次讀取就 timeout
----------

### 5.2 suspend / resume 後壞掉

常見原因：
-   resume 沒重新上電
-   I2C controller clock 沒恢復

👉 **I2C 是 suspend/resume 非常脆弱的介面。**

----------

## 6. 為什麼 probe 成功卻不能用

這是 BSP 最常見的誤判點。

probe 成功只代表：
-   driver bind 成功
-   address 有 ACK


不代表：
-   裝置功能正常
-   timing 正確

----------

## 7. Debug Checklist

### 7.1 軟體層
-   controller driver 是否 probe
-   device 是否出現在 /sys/bus/i2c

----------

### 7.2 硬體層

-   SDA / SCL 是否有 pull-up
-   scope / logic analyzer 是否看到正確波形
----------

### 7.3 系統層

-   clock 是否 enable
-   power 是否穩定
----------

## 8. 常見錯誤歸因

| 現象          | 常見誤判        | 真正原因            |
|---------------|-----------------|---------------------|
| Sensor 讀不到 | Driver bug      | Pinmux / Power      |
| 偶發錯誤      | I2C speed 問題  | Clock / EMI         |
| Resume 壞掉   | Kernel bug      | Power sequence 問題 |

----------

## 9. I2C Debug Toolbox

### 9.1 確認 I2C controller 是否存在且啟用

```bash
ls /sys/class/i2c-adapter/
i2cdetect -l
```

觀察重點：
-   是否有對應的 `i2c-X`
-   adapter 名稱是否符合預期的 SoC controller

若這一步沒有看到：
-   問題通常在 **controller driver / DTS / clock / reset**


### 9.2 確認 bus 上是否看到裝置（address level）
```bash
i2cdetect -y 1
```
注意事項：
-   掃不到 **不一定代表裝置不存在** 
    -   可能已被 driver claim  
-   若完全沒有 ACK，優先檢查：
    -   pinmux
    -   pull-up
    -   power


### 9.3 驗證基本 I2C 通訊是否穩定
```bash
i2cget -y 1 0x48 0x00
i2cset -y 1 0x48 0x01 0x80
```
用途：
-   驗證 read / write 是否能穩定完成
-   排除「probe 成功但 runtime 存取失敗」


### 9.4 Kernel 層錯誤觀察
```bash
dmesg | grep -i i2c
```
常見訊息：
-   timeout 
-   NACK
-   arbitration lost
    
這類訊息多半指向：
-   clock 不穩
-   bus speed 過高
-   電氣 / EMI 問題


### 9.5 Suspend / Resume 專用檢查
```bash
echo mem > /sys/power/state
```
resume 後立刻測試：
```bash
i2cget -y 1 0x48 0x00
```
如果 **resume 後第一次 I2C 存取就失敗**：
-   通常是 **power / clock 沒有正確恢復**

### 9.6 快速問題定位表

| 現象              | 最可能問題層級        |
|-------------------|-----------------------|
| i2c-X 不存在      | I2C Controller / Clock|
| 掃不到 Address    | Pinmux / Power        |
| 偶發 Timeout      | Clock / EMI           |
| Resume 後失效     | Power Sequence        |
