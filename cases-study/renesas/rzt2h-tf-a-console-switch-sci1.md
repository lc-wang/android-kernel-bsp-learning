
# 🧩 Renesas RZ/T2H Trusted Firmware-A

## Console 從 SCI0 切換至 SCI1 技術紀錄

## 📌 背景說明

在 **Renesas RZ/T2H** 平台中，官方提供的 **Trusted Firmware-A（TF-A）** 預設序列主控台（console）設定為：

-   UART IP：**RSCIF / SCI0**
    
-   Base address：`0x80005000`
    
-   Pin：
    
    -   TX：P27_5
        
    -   RX：P27_4
        

然而在實際硬體設計上：

-   SCI0 腳位並未接出至 debug UART
    
-   板端實際連接的是 **SCI1**
    

因此若不調整 TF-A，將會在最早期開機階段（BL2 / BL31）**完全沒有序列輸出**。

----------

## 🎯 修改目標


| 項目           | SCI0（預設）     | SCI1（目標）     |
|----------------|------------------|------------------|
| Base address   | 0x80005000       | 0x80005400       |
| TX pin         | P27_5            | P11_1            |
| RX pin         | P27_4            | P11_0            |
| Linux 裝置     | /dev/ttySC0      | /dev/ttySC1      |

----------

## 🔍 初期嘗試與問題

一開始僅修改以下兩個部分：

### 1️⃣ 修改 SCIF base address
```
- #define RZT2H_SCIF_BASE  0x80005000
+ #define RZT2H_SCIF_BASE  0x80005400
```

----------

### 2️⃣ 修改 pinmux 為 SCI1
```
{11, 1, 20, ...}, /* TXD1 */
{11, 0, 20, ...}, /* RXD1 */
```

----------

### ❌ 結果

系統 **完全沒有任何 UART 輸出**。

----------

## 🧠 問題根因分析

雖然：

-   base address 已改為 SCI1
    
-   pinmux 也已正確
    

但 **Renesas TF-A 的 console 初始化流程中，仍完全以 SCI0 為前提設計**。

----------

### TF-A console 初始化流程如下：
```
BL2 / BL31
 └─ rz_console_init()
     ├─ cpg_mstop_scif()      ← 解除 module stop
     ├─ pfc_scif_setup()      ← pinmux
     └─ console_rz_register() ← console driver
```
----------

### 問題點在於：

-   `cpg_mstop_scif()` **只解除 SCIF0 的 module stop**
    
-   SCI1 的 clock 仍處於停止狀態
    

導致：

> 即使程式存取 SCI1 暫存器  
> 但實際硬體模組尚未供電

因此 UART 無法運作。

----------

## ❌ 原始程式限制

在原始 TF-A 程式碼中：

### `sys_regs.h`
```
/* only SCIF0 defined */
#define MSTPCRA_MSTPCRA08   (8)
```

----------

### `cpg.c`
```
/* 固定解除 SCIF0 */ mmio_write_32(MSTPCRA,
    mmio_read_32(MSTPCRA) & ~BIT_32(MSTPCRA_MSTPCRA08));
```
----------

換句話說：

> **整份 TF-A 原生僅支援 console = SCI0**

----------

## ✅ 最終修正內容

## 1️⃣ 修改 SCIF base address

**檔案：**

`plat/renesas/rz/soc/t2h/include/rz_soc_def.h` 

`#define RZT2H_SCIF_BASE  UL(0x80005400) /* SCI1 */` 

----------

## 2️⃣ 修改 pinmux 為 SCI1

**檔案：**

`plat/renesas/rz/soc/t2h/drivers/pfc.c` 
```
static const PORT_SETTINGS sci_pins[] = {
    {11, 1, 20, DRCTL_SRm0_MSK | DRCTL_Em0_DRIVE_HI_MSK}, /* TXD1 */
    {11, 0, 20, DRCTL_SRm0_MSK | DRCTL_Em0_DRIVE_HI_MSK}, /* RXD1 */
};
```
----------

## 3️⃣ 新增 SCI1 的 Module-Stop 定義

**檔案：**

`plat/renesas/rz/soc/t2h/include/sys_regs.h` 
```
/* SCIF0 */
#define MSTPCRA_MSTPCRA08        (8)

/* SCIF1 */
#define MSTPCRA_MSTPCRA09        (9)
#define MSTPCRA_MSTPCRA09_MSK    (1U << MSTPCRA_MSTPCRA09)
```
----------

## 4️⃣ 解除 SCI1 module stop

**檔案：**

`plat/renesas/rz/soc/t2h/drivers/cpg.c` 
```
static void cpg_mstop_scif(void)
{
    uint32_t bit = MSTPCRA_MSTPCRA08; /* 預設 SCI0 */

    if (RZT2H_SCIF_BASE == UL(0x80005400))
        bit = MSTPCRA_MSTPCRA09;      /* SCI1 */

    sys_base_unlock(PRCRx_LOW_POWER);

    mmio_write_32(MSTPCRA,
        mmio_read_32(MSTPCRA) & ~BIT_32(bit));

    sys_base_lock(PRCRx_LOW_POWER);
}
```
----------

## ✅ 修改結果

成功於 SCI1 看到 TF-A console：
```
NOTICE:  BL2: v2.7(release)
NOTICE:  BL2: Built : 10:39:57, Jan 23 2026
NOTICE:  BL2: Booting BL31
NOTICE:  BL31: v2.7(release)
NOTICE:  BL31: Built : 10:39:59, Jan 23 2026
```
----------

## 🧠 重點整理

### ✅ 僅修改 UART base 與 pinmux 並不足夠

TF-A 尚需：

-   module clock enable
    
-   MSTP bit 解除
    
-   BL2 / BL31 同步初始化
    

----------

### ✅ Renesas TF-A console 為「固定 instance 設計」

-   原始程式僅支援 SCI0
    
-   非動態選擇 UART instance
    
-   若需切換 UART，必須同步修改 CPG 與 sys_regs
