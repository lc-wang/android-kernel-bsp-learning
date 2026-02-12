
# Kernel trace notes — dma_alloc_coherent

---

## 🔍 dma_alloc_coherent() 走哪裡？
```
dma_alloc_coherent()
	└─ dma_alloc_attrs()
		└─ ops->alloc()
			└─ arch specific
```

實際實作依平台而定：

- ARM / ARM64
- x86
- IOMMU / non-IOMMU

---

## 🧠 CPU / Device 看的是什麼？

CPU → cpu_addr (virtual)
Device → dma_addr (DMA address)


兩者是：

- 同一塊實體記憶體
- 不同的 address view

---

## 🔥 為什麼需要 coherent？

沒有 coherent 的世界：

CPU cache ≠ DRAM ≠ Device view


coherent 保證：

- CPU 寫 → device 看到
- device 寫 → CPU 看到
- driver 不用自己 flush cache

---

## 🧩 為什麼 kmalloc 不夠？


kmalloc:

-   cache 行為未定
    
-   需要手動 sync
    
-   不保證 DMA-safe
    

dma_alloc_coherent:

-   為 DMA 設計
    
-   cache 一致
    
-   安全


---

## 🔥 真實 driver 對照

| Subsystem | 用途 |
|----------|------|
| DRM GEM | framebuffer |
| Camera ISP | image buffer |
| netdev | ring buffer |
| remoteproc | shared memory |

---

## 🧠 心智模型

DMA buffer
≠ normal memory


**只要是硬體會直接碰的記憶體，一律用 dma API。**
