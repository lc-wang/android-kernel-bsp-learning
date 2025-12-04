
# **RZ/T2H：使用 e² studio 建立 CR52 Firmware 並以 Linux Remoteproc 啟動**


# 目錄

1.  **簡介**
2.  **環境需求**
3.  **建立 CR52 e² studio 專案**
4.  **調整啟動流程（Startup & Linker）**
5.  **實作 HelloWorld Firmware**
6.  **編譯與產生 ELF**
7.  **在 Linux 上透過 remoteproc 啟動 CR52**
8.  **驗證 CR52 Firmware 是否正在運行**
9.  **常見問題整理**
10.  **附錄：CR52 啟動流程架構圖**

----------

# 1. 簡介

本文件說明如何在 **Renesas RZ/T2H** 平台上，利用 **e² studio** 建立一個最小化 CR52 firmware（HelloWorld），並由 **Linux remoteproc** 模組在 A55（Linux）側負責載入與啟動此 firmware。

本手冊不涵蓋 RPMsg / OpenAMP 內容，僅專注於：

-   如何建立可由 remoteproc 載入的 CR52 ELF
-   如何正確設定 linker address 與啟動向量   
-   如何確認 CR52 firmware 已成功啟動並執行
    

此內容可作為後續整合 OpenAMP / RPMsg 的基礎。

----------

# 2. 環境需求

### 軟體

-   Renesas **e² studio**
-   Renesas RZ/T2H **FSP（Flexible Software Package）**
-   ARM GCC 工具鏈（e² studio 隨附）
-   Linux kernel（5.10 或 6.x）支援 `renesas,rzt-cr52` remoteproc driver
-   使用者空間工具：`devmem2`
    

### 硬體

-   RZ/T2H Evaluation Board（R9A09G077M44/R9A09G077M48）
-   A55 Linux 系統可正常運行
    

----------

# 3. 建立 CR52 e² studio 專案

1.  **File → New → Renesas C/C++ Project**
2.  選擇：
    -   Category：**RZ**
    -   Device：**R9A09G077M48GBG** 或 RZ/T2H 對應晶片
    -   Project Type：**Baremetal / Standalone**
        
3.  專案建立後，預設會包含：
    -   FSP 啟動碼
    -   標準 CMSIS 結構
    -   預設 linker script
        

後續將調整上述內容，使之符合 remoteproc 需求。

----------

# 4. 調整啟動流程（Startup & Linker）

Linux remoteproc 會根據 Device Tree 中的設定，將 CR52 firmware 的啟動位址設定為：

`renesas,rz-start_address = <0x10061000>;` 

因此必須確保：

1.  CR52 的 **linker.ld** `_start` 與 `.text` 必須放在 `0x10061000`
2.  startup 文件（startup_cr52.S）必須以 ARM mode 寫入 Reset Vector，並跳轉到 `_start`
    

### 4.1 修改 linker script

將 linker script 的 `.text` 起始位址調整如下：
```sh
SECTIONS
{
    .text 0x10061000 :
    {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
    }

    .data :
    {
        *(.data*)
    }

    .bss :
    {
        *(.bss*)
    }
}
```
此設定需與 DTS 匹配。

### 4.2 調整 CR52 啟動碼

使用以下簡化版（須確保為 ARM mode）：
```sh
.syntax unified
.cpu cortex-r52
.fpu neon-fp-armv8
.thumb
.thumb_func

.section .vectors, "ax"
.global _start
.type _start, %function

_start:
    .arm
    LDR sp, =0x10080000   // stack pointer
    BL  main
    B   .
```
----------

# 5. 實作 HelloWorld Firmware

目標程式碼：每隔一段時間，向 A55 可見的 SRAM 寫入遞增的值。

### hal_entry.c
```c
#include  <stdint.h>  #define TEST_ADDR (*(volatile uint32_t *)0x10070000) void  hal_entry(void)
{ uint32_t counter = 0; while (1)
    {
        TEST_ADDR = 0x12340000 + counter;
        counter++; for (volatile  int i = 0; i < 500000; i++);
    }
} 
```
該位址位於 CR52 與 A55 共用區域，可由 Linux 端 `devmem2` 讀取。

----------

# 6. 編譯與產生 ELF

在 e² studio：

1.  **Project → Clean**
2.  **Project → Build Project**
    

成功後會產生：

`Debug/HelloWorld.elf` 

此 ELF 即可由 Linux remoteproc 載入。

----------

# 7. 在 Linux 上透過 remoteproc 啟動 CR52

### 7.1 將 ELF 放入 Linux firmware 目錄
`sudo cp HelloWorld.elf /lib/firmware/` 
### 7.2 設定 remoteproc firmware 名稱
`echo HelloWorld.elf | sudo tee /sys/class/remoteproc/remoteproc0/firmware` 
### 7.3 啟動 CR52
`echo start | sudo tee /sys/class/remoteproc/remoteproc0/state` 
### 7.4 確認啟動訊息

`dmesg | tail -n 20` 

預期顯示：
```sh
remoteproc remoteproc0: Booting fw image HelloWorld.elf, size ...
remoteproc remoteproc0: no resource table found for this firmware
remoteproc remoteproc0: remote processor cr52_0 is now up
```
----------

# 8. 驗證 CR52 Firmware 是否正在運行

使用 `devmem2` 讀取 firmware 持續寫入的記憶體位置：
```sh
sudo devmem2 0x10070000
sudo devmem2 0x10070000
sudo devmem2 0x10070000
```
若數值持續增加，例如：
```sh
0x12341C62
0x12341C77
0x12341C8B
```
則代表：

-   remoteproc 成功啟動 ELF
-   CR52 firmware 正在執行
    

此步驟完成整個 remoteproc HelloWorld 驗證流程。

----------

# 9. 常見問題整理

### (1) `Bad phdr da` / `invalid ELF`
-   ELF 未對齊 Device Tree 中的 `renesas,rz-start_address`
-   Linker script address 設錯
-   Startup code 未跳到 `_start`

### (2) `Boot failed: -22`

-   ELF 使用 Thumb mode 或 vector table 格式不正確

### (3) `Boot failed: -12`

-   resource table 不完整或不需要  
    （HelloWorld 不應該包含 resource table）

### (4) CR52 無動作

-   hal_entry 未執行
-   可能忘記排除 FSP 的 startup.c
-   stack pointer 未設定

----------

# 10. 附錄：CR52 remoteproc 啟動流程圖
```markdown
A55  (Linux) |
   | remoteproc
   |   1. 讀取 HelloWorld.elf
   |   2. 載入至 CR52 的 SRAM/TCM
   |   3. 設定 Start Address = 0x10061000
   |   4. 啟動 CR52 核心
   |
   V CR52  CPU |
   | 執行 _start
   | → main()
   | → hal_entry()
   |    → 持續寫入 0x10070000
   |
   V A55  驗證：  devmem2  0x10070000  →  數值持續增加（CR52  正在跑）```
