
# module_basic

Linux kernel module 最小生命週期範例。

此目錄提供一個**最小可運作的 kernel module**，用途並非教學，
而是作為日後 trace Linux kernel driver 時的「對照心智模型」。

---

## 🎯 本範例的目的

此範例用來理解：

- module_init() / module_exit()
- __init / __exit section 行為
- module metadata（MODULE_LICENSE 等）
- kernel log（pr_info）
- insmod / rmmod 的實際執行流程

這一套流程適用於：

- platform driver
- SPI / I2C driver
- DRM driver
- network driver
- staging driver

---

## 🧩 Kernel 原始碼對照位置

相關核心程式碼位於：
```
include/linux/module.h
kernel/module/main.c
kernel/module/kmod.c
kernel/init/main.c
```

模組載入時的關鍵函式：
```
finit_module()
└─ load_module()
├─ layout_and_allocate()
├─ copy_module_from_user()
├─ resolve_symbols()
├─ do_init_module()
│ └─ do_one_initcall()
└─ module_enable_ro()
```

---

## 🔄 Module 載入流程（insmod）
```
userspace
└─ insmod hello_module.ko
└─ finit_module()

kernel
└─ load_module()
└─ do_init_module()
└─ do_one_initcall()
└─ hello_init()
```

---

## 🔁 Module 卸載流程（rmmod）
```
rmmod hello_module
└─ delete_module()
└─ exit_module()
└─ hello_exit()
```

---

## 🧠 為什麼這很重要？

在 trace kernel driver 時，你一定會看到：
```
do_one_initcall()
```

例如：

- platform_driver_register()
- spi_register_driver()
- i2c_register_driver()
- drm_dev_register()
- pci_register_driver()

**所有 driver init 都會經過同一條路徑**。

---

## ⚠ 常見誤解釐清

### ❓ 為什麼這個範例沒有 probe()？

因為：

- module ≠ device
- module_init() 只代表「driver 存在」
- probe() 代表「device 出現」

probe() 只有在下列條件同時成立時才會呼叫：

driver_register()

-   device_register()
    
-   bus match
