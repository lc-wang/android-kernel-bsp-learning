
# mmap_driver_example

最小 mmap 驅動範例：userspace `mmap()` ↔ kernel driver `.mmap()`。

本章的目標是建立「zero-copy 對照模型」：

- userspace 透過 mmap 取得一段 memory mapping
- kernel driver 提供 `.mmap`，把一塊 kernel page 映射到 userspace
- userspace 直接讀寫映射區，kernel 也能看到同一塊 memory 內容

---

## 🎯 本章的目的

理解：

- `.mmap` file operation 是什麼
- `vm_area_struct (vma)` 的基本概念
- `remap_pfn_range()` 做了什麼
- 為什麼 mmap 常用在 framebuffer / camera buffer / ring buffer

---

## 🧠 快速心智模型

read/write
= copy

mmap
= map（把同一塊頁面映射到 userspace）


---

## 🧩 Kernel 原始碼對照
```
mm/mmap.c
mm/memory.c
fs/read_write.c
drivers/char/misc.c
include/linux/mm.h
```

---

## ✅ Build / Run

### 1) 編 kernel module
```bash
cd kernel
make
sudo insmod mmap_dev.ko
dmesg | tail
ls -l /dev/mymmap
```
### 2) 編 userspace 測試程式
```
cd ../userspace
make
./mmap_user
```
### 3) 看 kernel log
```
dmesg | tail -n 80
```
⚠ 注意
本章為了「最小化」，只映射 1 page（4KB）。


真正大型 buffer（DRM GEM / camera / CMA）會用：

-   dma_alloc_coherent / CMA / shmem
    
-   vm_ops + fault
    
-   甚至 IOMMU / cache sync


---

