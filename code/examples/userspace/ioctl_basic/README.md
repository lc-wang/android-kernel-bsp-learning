
# ioctl_basic

userspace ↔ kernel driver ioctl 最小對照範例。

本章節的目標是：

- 讓 ioctl 不再是黑魔法
- 建立 userspace → kernel 的完整 mental model

---

## 🎯 本章的目的

此範例示範：

- userspace 如何呼叫 ioctl()
- ioctl cmd 如何編碼
- data 如何在 user / kernel 間傳遞
- kernel driver 如何接收 ioctl

這個模型適用於：

- Bluetooth HCI ioctl
- DRM ioctl
- V4L2 ioctl
- miscdevice
- 各類 control path driver

---

## 🔗 與 kernel/char_device 的關係

本章 userspace 程式會直接操作：

/dev/mychardev


並對應到：

file_operations.unlocked_ioctl()


---

## 🧠 ioctl 的本質

ioctl = control path


不是資料流（data path），而是：

- 設定
- 查詢
- 命令
- 狀態控制
