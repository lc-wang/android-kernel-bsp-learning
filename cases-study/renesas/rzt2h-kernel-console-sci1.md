
# 🧠 RZ/T2H Kernel Console 從 SCI0 切換至 SCI1 技術紀錄

## 📌 文件目的

本文件紀錄在 **Renesas RZ/T2H（r9a09g077）平台**上，  
將 Linux kernel console 輸出 UART 由：

`SCI0 (0x80005000)` 

成功切換為：

`SCI1 (0x80005400)` 

此修改用於因應客戶硬體設計，debug UART 實際連接至 **SCI1** 的情境。

----------

## ✅ 最終成果

Linux kernel 開機 log 已可完整從 **SCI1 UART** 輸出：
```
earlycon: rscif at MMIO 0x80005400
printk: bootconsole [rscif] enabled
console [ttySC1] enabled
```
SCI0 可完全 disabled，不再參與 kernel console。

----------

## 🧩 測試環境

| 項目         | 說明                                   |
|--------------|----------------------------------------|
| SoC          | Renesas RZ/T2H                         |
| Part number  | r9a09g077                              |
| Board        | r9a09g077m44-dev（EVK）                |
| Kernel       | Linux 5.10.145-cip17                   |
| 架構         | ARM64                                  |
| UART IP      | Renesas RSCIF                          |
| Toolchain    | aarch64-poky-linux-gcc                 |

----------

## 🔍 原始系統行為


預設 Kernel Console 設定如下：

| 項目         | 值                         |
|--------------|----------------------------|
| UART         | SCI0                       |
| Base         | 0x80005000                 |
| Device node  | serial@80005000            |
| Linux device | /dev/ttySC0                |


Kernel log：

`earlycon: sci0 at MMIO  0x80005000  console [ttySC0] enabled` 

----------

## 🎯 修改目標


| 項目         | 設定              |
|--------------|-------------------|
| UART         | SCI1              |
| Base         | 0x80005400        |
| Linux device | /dev/ttySC1       |
| Kernel log   | 由 SCI1 輸出      |


----------

# 🛠 Kernel 修改內容


## ✅ 一、設定 SCI1 pinmux

### 檔案位置

`arch/arm64/boot/dts/renesas/r9a09g077m44-dev.dts` 
```
&pinctrl {
        sci1_pins: sci1 {
                pinmux = <RZT2H_PORT_PINMUX(11, 1, 4)>, /* SCI1_TXD */
                         <RZT2H_PORT_PINMUX(11, 0, 4)>; /* SCI1_RXD */
                bias-pull-up;
        };
};

&sci1 {
        pinctrl-0 = <&sci1_pins>;
        pinctrl-names = "default";
        status = "okay";
}; 
```
----------

## ✅ 二、停用 SCI0（避免 fallback）
```
&sci0 {
        status = "disabled";
};
```
若未 disable，kernel 可能仍會自動選擇 SCI0。

----------

# 🧭 Kernel console 指定方式（重點）

⚠️ **本次實作並未修改 kernel DTS 的 `/chosen` node。**

----------

### kernel console 完全由 **U-Boot bootargs 傳入**

U-Boot 設定：
```
--- a/include/configs/rzt2h-dev.h
+++ b/include/configs/rzt2h-dev.h
@@ -61,7 +61,7 @@
 #define CONFIG_EXTRA_ENV_SETTINGS \
        "usb_pgood_delay=2000\0" \
        "bootm_size=0x10000000\0" \
-       "prodsdbootargs=setenv bootargs rw rootwait earlycon root=/dev/mmcblk1p2 \0" \
+       "prodsdbootargs=setenv bootargs rw rootwait console=ttySC1,115200n8 earlycon=rscif,80005400 root=/dev/mmcblk1p2 \0" \
        "prodemmcbootargs=setenv bootargs rw rootwait earlycon root=/dev/mmcblk0p2 \0" \

```
----------

### 傳入 kernel 的 bootargs
```
console=ttySC1,115200n8
earlycon=rscif,80005400
```
----------

## 🔎 Kernel log 驗證

開機訊息顯示：
```
[    0.000000] earlycon: rscif at MMIO 0x0000000080005400
[    0.000000] printk: bootconsole [rscif] enabled
[    0.016869] printk: console [ttySC1] enabled
```
代表：

-   early console 使用 SCI1
    
-   kernel console 綁定 ttySC1
    
-   SCI0 已完全未被使用
    

----------

## 🧪 Runtime 驗證
```
# 確認 SCI1 device tree 狀態
cat /sys/firmware/devicetree/base/soc/serial@80005400/status
okay
```
```
# Kernel console device
dmesg | grep ttySC
```
----------

## 🚫 常見錯誤整理

### ❌ 只改 bootargs 不會成功

必須同時具備：

-   UART node 存在
    
-   pinctrl 設定正確
    
-   clock / power domain 啟用
    
-   status = "okay"
    

----------

### ❌ kernel DTS 與 U-Boot DTS 不共用

| Stage   | DTS 路徑                         |
|---------|----------------------------------|
| U-Boot  | arch/arm/dts/                    |
| Kernel  | arch/arm64/boot/dts/             |


修改 kernel DTS **不會影響 U-Boot console**。

----------

### ❌ `/chosen` 非必要

本案例中：

-   kernel **完全依賴 bootargs**
    
-   `/chosen { stdout-path }` 未修改
    
-   行為仍完全正確

