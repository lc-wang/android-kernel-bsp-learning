
# platform_bus

Linux platform bus / platform driver 最小實作範例。

platform bus 是 Linux 中 **SoC 裝置驅動的核心機制**，
幾乎所有非 PCI / USB 裝置都會透過 platform bus 來綁定 driver。

---

## 🎯 本章的目的

本章用來建立以下關鍵觀念：

- platform bus 是什麼
- platform_device 與 platform_driver 的關係
- device tree 如何對應到 platform_device
- probe() 是在什麼條件下被呼叫

---

## 🧠 platform bus 的定位

platform bus
= SoC 上「不是自動枚舉」的裝置


例如：

- UART
- SPI controller
- I2C controller
- DRM display
- GPIO controller
- watchdog
- remoteproc

---

## 🧩 Kernel 原始碼對照
```
drivers/base/platform.c
drivers/base/bus.c
drivers/of/platform.c
drivers/of/base.c
```

---

## 🔄 總體流程（Device Tree → probe）
```
Device Tree (.dts)
└─ of_platform_populate()
└─ platform_device_register()
└─ device_add()
└─ bus_add_device()
└─ platform_bus.match()
└─ driver.probe()
```

---

## 🧠 最重要的觀念

沒有 platform_device
就不會有 probe()


DTS 本身 **不會直接呼叫 driver**，
它只會生成 platform_device。

