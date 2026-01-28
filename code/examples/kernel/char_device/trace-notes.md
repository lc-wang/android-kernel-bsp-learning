
# Kernel trace notes — char_device

---

## 🧩 /dev 節點從哪來？
```
device_create()
└─ drivers/base/core.c
└─ device_add()
└─ uevent
└─ udev 建立 /dev/mychardev
```

---

## 🔍 open() trace
```
open("/dev/mychardev")
└─ sys_openat
└─ do_sys_open
└─ do_dentry_open
└─ chrdev_open
└─ file->f_op = my_fops
└─ my_open()
```

---

## 📖 read() trace
```
read()
└─ vfs_read()
└─ file->f_op->read()
└─ my_read()
```

---

## ✍ write() trace
```
write()
└─ vfs_write()
└─ file->f_op->write()
└─ my_write()
```

---

## 🔧 ioctl() trace
```
ioctl()
└─ do_vfs_ioctl()
└─ file->f_op->unlocked_ioctl()
└─ my_ioctl()
```

---

## 🧠 最重要心智模型
```
userspace
↓
syscall
↓
VFS
↓
file_operations
↓
driver
```

---

## 🚫 常見錯誤觀念

❌ /dev 是 driver  
❌ open() 直接進 driver  

✅ 實際是：

inode → struct file → f_op

driver 只是 callback 集合。

---

## 🔥 為什麼這一章超重要？

因為：

- Bluetooth HCI → ioctl
- Wi-Fi cfg80211 → ioctl
- DRM → ioctl
- V4L2 → ioctl
- media → ioctl

**90% kernel driver 的 userspace 入口都在這裡。**
