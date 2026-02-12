
# dma_alloc_coherent

Linux kernel 中「裝置 DMA buffer」的最小實作範例。

本章用來建立一個**非常重要、而且不能搞錯的觀念**：

> **不是所有記憶體都能給硬體用。**

---

## 🎯 本章的目的

理解以下問題：

- 為什麼硬體需要 DMA memory
- dma_alloc_coherent() 和 kmalloc() 的差異
- CPU 與 device 如何看到「同一塊記憶體」
- cache coherency 是什麼

---

## 🧠 一句話結論

dma_alloc_coherent
= 給硬體用的記憶體


它保證：

- CPU 虛擬位址可存取
- 裝置 DMA 位址可存取
- cache 一致性（coherent）

---

## 🧩 Kernel 原始碼對照
```
kernel/dma/mapping.c
drivers/base/dma-mapping.c
include/linux/dma-mapping.h
```

---

## 🚫 常見錯誤

❌ 用 kmalloc buffer 直接給硬體  
❌ 用 vmalloc buffer 做 DMA  
❌ 自己處理 cache flush
