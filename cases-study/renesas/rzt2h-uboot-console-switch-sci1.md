
# 🧩 Renesas RZ/T2H

## U-Boot Console 從 SCI0 切換至 SCI1 技術紀錄


## 📌 背景說明

在 Renesas **RZ/T2H** 平台中：

-   Boot ROM
    
-   Trusted Firmware-A
    
-   U-Boot
    

皆預設使用 **SCI0** 作為開機序列主控台（boot console）。

然而在實際硬體設計上：

-   SCI0 腳位未接出
    
-   Debug UART 實際連接於 **SCI1**
    

因此需將 **U-Boot console 由 SCI0 改為 SCI1**，以便在 TF-A 後仍可持續看到 UART 訊息。

----------

## 🎯 修改目標


| 項目            | SCI0（預設） | SCI1（目標） |
|-----------------|---------------|---------------|
| UART IP         | RSCIF         | RSCIF         |
| Base address    | 0x80005000    | 0x80005400    |
| TX pin          | P27_5         | P11_1         |
| RX pin          | P27_4         | P11_0         |
| U-Boot console  | ttySC0        | ttySC1        |


----------

## 🔍 初始修改內容

### 1️⃣ 新增 SCI1 device node
```
sci1: serial@80005400 {
        compatible = "renesas,r9a09g077-rz-rscif",
                     "renesas,rz-rscif";
        reg = <0 0x80005400 0 0x400>;
        interrupts = <GIC_SPI 594 IRQ_TYPE_LEVEL_HIGH>,
                     <GIC_SPI 595 IRQ_TYPE_EDGE_RISING>,
                     <GIC_SPI 596 IRQ_TYPE_EDGE_RISING>,
                     <GIC_SPI 597 IRQ_TYPE_LEVEL_HIGH>;
        clocks = <&cpg CPG_MOD R9A09G077_SCI1_CLK>;
        clock-names = "fck";
        power-domains = <&cpg>;
        status = "disabled";
};
```
----------

### 2️⃣ 指定 serial0 alias

`aliases {
        serial0 = &sci1;
};` 

----------

### 3️⃣ 啟用 SCI1

`&sci1 {
        status = "okay";
};` 

----------

## ❌ 問題現象

即使完成以上設定：

-   TF-A 可正常由 SCI1 輸出
    
-   進入 U-Boot 後 **完全沒有任何 UART 訊息**
    

----------

## 🧠 問題根因分析

### ❗ 關鍵原因：

**U-Boot serial driver 並不支援該 compatible 字串。**

----------

### 🔍 實際使用的 driver

在本 U-Boot tree 中，Renesas UART 使用：

`drivers/serial/serial_sh.c` 

其 `of_match_table` 僅支援：
```
"renesas,sci"
"renesas,scif"
"renesas,scifa"
"renesas,rsci"
```

----------

### ❌ SCI1 DTS 使用的 compatible
```
"renesas,r9a09g077-rz-rscif"
"renesas,rz-rscif"
```

上述兩者 **皆未被 serial_sh driver 支援**。

----------

### 🔥 結果

-   SCI1 節點存在
    
-   alias 正確
    
-   clock / reg 正確
    

但：

> **U-Boot 找不到可 bind 的 serial driver**

因此：
```
serial device probe = skipped
console = none
```

導致 UART 無任何輸出。

----------

## ✅ 正確修正方式

讓 SCI1 **與 SCI0 使用相同 compatible**，走同一條 driver path。

----------

### ✅ 修正後 SCI1 DTS 節點
```
sci1: serial@80005400 {
        compatible = "renesas,r9a09g077-rsci",
                     "renesas,rsci";
        reg = <0 0x80005400 0 0x400>;
        interrupts = <GIC_SPI 594 IRQ_TYPE_LEVEL_HIGH>,
                     <GIC_SPI 595 IRQ_TYPE_EDGE_RISING>,
                     <GIC_SPI 596 IRQ_TYPE_EDGE_RISING>,
                     <GIC_SPI 597 IRQ_TYPE_LEVEL_HIGH>;
        clocks = <&cpg CPG_MOD R9A09G077_SCI1_CLK>;
        clock-names = "fck";
        power-domains = <&cpg>;
        status = "disabled";
};
```
----------

## ✅ 修改後結果

U-Boot 成功於 SCI1 顯示訊息：
```
U-Boot 2024.xx
CPU: Renesas RZ/T2H
DRAM: 4096 MiB
MMC:  sdhi0@11c00000
```
----------

## 🧠 為什麼 SCI0 一開始可以正常工作？

因為原始 DTS 中：
```
sci0: serial@80005000 {
        compatible = "renesas,r9a09g077-rsci",
                     "renesas,rsci";
};
```
此 compatible **正好被 serial_sh driver 支援**。

SCI1 若未使用相同 compatible，U-Boot 將完全無法識別。
