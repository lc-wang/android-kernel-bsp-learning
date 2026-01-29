
# Kernel trace notes — ioctl_basic

---

## 🧑‍💻 userspace 呼叫點
```
ioctl(fd, cmd, arg)
glibc：
syscall(SYS_ioctl, fd, cmd, arg)
```

---

## 🧠 kernel syscall 入口
```
SYSCALL_DEFINE3(ioctl)
```

位置：
```
fs/ioctl.c
```

---

## 🔍 呼叫流程
```
ioctl()
└─ sys_ioctl()
└─ do_vfs_ioctl()
└─ file->f_op->unlocked_ioctl()
└─ my_ioctl()
```

---

## 🔑 ioctl cmd 結構
```
| dir | size | magic | nr |
```

由 `_IO*()` 巨集編碼。

---

## 🔄 user / kernel 資料流

### _IOW
```
user data
└─ copy_from_user()
```

### _IOR
```
kernel data
└─ copy_to_user()
```

---

## 🧠 與 read/write 的差異

| API | 用途 |
|----|----|
| read/write | 資料流 |
| ioctl | 控制 / 命令 |

---

## 🚫 常見誤解

❌ ioctl 是慢的  
❌ ioctl 不能傳結構  

✅ 事實是：

- ioctl 是同步 syscall
- 90% driver 都在用
- DRM / V4L2 / netlink 都大量使用

