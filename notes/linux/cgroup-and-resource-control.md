
# Linux cgroup & Resource Control（資源控制的核心模型）

> 本章定位：
> 
> -   從 **Linux 原生視角**理解 cgroup 為什麼存在、解決什麼問題，以及如何影響實際執行行為
----------

## 1. 為什麼 Linux 需要 cgroup

在沒有 cgroup 之前，Linux 的資源管理主要依賴：

-   scheduler（CPU）
-   nice / priority
-   rlimit
    

這些機制有一個共同問題：

-   **以 process 為單位**
-   缺乏「群組」的資源上限與隔離

cgroup 的核心目標是回答：

> 一組工作負載（process / thread）最多能用多少資源？

----------

## 2. cgroup 的基本模型

### 2.1 cgroup 是什麼

cgroup 是：

-   kernel 提供的資源控制層
-   對一群 task 套用資源限制與統計
    

關鍵特性：

-   階層式（hierarchical）
-   可動態移動 task

----------

### 2.2 Controller 的概念

每一種資源對應一個 controller，例如：

-   cpu
-   memory
-   blkio / io 
-   cpuset

controller 決定：

-   能限制什麼
-   能統計什麼

----------

## 3. cgroup v1 與 v2 的根本差異

### 3.1 v1：多層樹、各自為政

特性：

-   每個 controller 一棵樹    
-   task 可同時存在於多個 hierarchy    

缺點：

-   行為難以推理
-   controller 互動複雜

----------

### 3.2 v2：單一 unified hierarchy

v2 的核心理念：

-   所有 controller 共用一棵樹
-   資源分配更一致
    
影響：

-   行為更可預期
-   更適合系統級管理（如 systemd / Android）

----------

## 4. CPU 資源控制

### 4.1 cpu.shares / weight 的意義

CPU controller 的核心不是「保證多少 CPU」，而是：

-   **相對比例**
    
需要理解：

-   shares / weight 影響競爭時的分配
-   空閒時不會限制
   
----------

### 4.2 Throttling 與 latency

當設定：

-   quota / period

可能結果：

-   CPU 被硬性限制
-   latency 顯著增加
    

👉 很多效能問題不是 scheduler bug，而是 cgroup 設定結果。

----------

## 5. Memory cgroup：限制與回收

### 5.1 Memory 壓力的來源

memory cgroup 能限制：

-   anonymous memory
-   page cache
    

但現實中：

-   cache 回收並非即時
-   IO 與 memory 行為交錯
    

----------

### 5.2 OOM 與 memory cgroup

當超過限制：

-   memory cgroup 會觸發 OOM
    
這可能：

-   不影響整個系統 
-   只殺該 group 內的 process
----------

## 6. IO / blkio / io controller

IO controller 解決的是：

-   一組 task 把磁碟吃光

但限制 IO 可能導致：

-   latency 增加    
-   indirect CPU stall
    

👉 IO 問題常表現為「CPU 在等」。

----------

## 7. cgroup 與 scheduler 的交互

### 7.1 group scheduling

scheduler 先在 group 間分配，再在 group 內挑 task。

這代表：

-   priority 只在 group 內比較
-   group weight 決定上限
   
----------

### 7.2 為什麼 priority 不一定有用

如果 task 在：

-   被低權重 group 包住
    
即使 priority 高：

-   也跑不快
    
----------

## 8. Debug cgroup 問題的方法

### 8.1 確認 task 所屬 cgroup
```bash
cat /proc/<pid>/cgroup
```
----------

### 8.2 查看 controller 設定
```bash
cat /sys/fs/cgroup/<path>/cpu.stat

cat /sys/fs/cgroup/<path>/memory.current
```
----------

### 8.3 觀察 throttling
```bash
cat /sys/fs/cgroup/<path>/cpu.stat
```
查看是否出現：

-   throttled_time
    
----------

## 9. 常見誤判與 Debug 心法

| 現象        | 常見誤判          | 真正原因               |
|-------------|-------------------|------------------------|
| CPU 不公平  | Scheduler bug     | Cgroup weight 設定     |
| Latency 高  | Kernel regression | Throttling（thermal / power） |
| OOM         | Memory leak       | Memory cgroup limit    |
