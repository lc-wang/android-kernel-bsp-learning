

# Android Graphics & Display System（SurfaceFlinger / HWComposer / BufferQueue）

本章解構 Android 的整體顯示架構：  
App → BufferQueue → SurfaceFlinger → HWComposer (HWC) → DRM/KMS → Panel。

---

## 1. 顯示系統總覽架構
```yaml
App (OpenGL/Canvas)
↓
Surface / SurfaceControl
↓
BufferQueue
Producer (App) → Consumer (SurfaceFlinger)
↓
SurfaceFlinger 合成 (OpenGL ES / RenderEngine)
↓
Hardware Composer HAL (HWC)
↓
DRM / KMS / Framebuffer
↓
Display Panel
```

Android 7.0 之後強調：
- SF（SurfaceFlinger）為「中央合成器」
- HWC（HAL）負責硬體 overlay、vsync、composition hint
- Producer/Consumer 模型使用 BufferQueue

---

## 2. BufferQueue（核心機制：生產者 / 消費者模型）

BufferQueue 是所有 Android 顯示資料流動的基礎。

### 結構
| 角色 | 功能 |
| --- | --- |
| BufferQueueProducer | App / Camera / MediaCodec 出 buffer |
| BufferQueueConsumer | SurfaceFlinger 接收 buffer |

### buffer 狀態流轉
```yaml
dequeueBuffer
write to GPU
queueBuffer
SurfaceFlinger acquireBuffer
SurfaceFlinger releaseBuffer
```

### 重要特性
- 內建 triple-buffering（避免延遲與撕裂）
- 透過 Binder IPC 傳遞 buffer metadata（非畫面本身）
- buffer 本體經由 Gralloc HAL 分配

---

## 3. SurfaceFlinger（SF：全系統合成器）

SurfaceFlinger 是 Android 顯示系統的「心臟」。

### SF 主要工作
| 功能 | 說明 |
| --- | --- |
| Layer 管理 | 每個畫面都是一個 Layer |
| 合成（Composition） | Mixed GPU + HWC |
| 處理 VSYNC | frame 時序來源 |
| 與 HWC 溝通 | 決定 overlay 或 GPU 合成 |
| 最終輸出到 framebuffer | 透過 HWC API |

---

## 4. HWComposer HAL（HWC）

HWC 是 HAL 層負責把 SF 的 Layer 實際輸出到硬體。

版本：
- **HWC1**：舊、複雜、狀態多  
- **HWC2**（Android 7+）：非同步、簡潔、callback 架構  

### HWC 的主要任務

| 任務 | 說明 |
| --- | --- |
| VSYNC | 產生 vsync event 給 SF |
| Composition | Overlay / GPU / Client 合成策略 |
| 輸出 framebuffer | 寫入 DRM/KMS 或 display controller |
| Present | 送出最終 frame |

你的 DRM driver（像 pixpaper）在這層是 **HWC 的 backend**。

---

## 5. VSYNC（影格時序）

VSYNC 控制整個顯示管線節奏。

來源：
- HWC → `/system/lib/hw/hwcomposer.*.so`
- 由硬體（display controller）產生 vsync interrupt

流程：
```yaml
VSYNC interrupt
↓
HWC callback setVsyncEnabled()
↓
SurfaceFlinger onVsync()
↓
Layer 更新 / composition / present
```

---

## 6. 合成方式（Composition Types）

| 類型 | 說明 |
| --- | --- |
| **Client Composition** | GPU 合成（OpenGL / Vulkan / RenderEngine） |
| **Device Composition** | HWC overlay，省電、效能高 |
| **Solid Color** | 純色 layer |
| **Cursor** | 游標 layer |

HWC 會回報：
```yaml
HWC2_COMPOSITION_DEVICE
HWC2_COMPOSITION_CLIENT
```

---

## 7. RenderEngine（OpenGL / Skia GPU backend）

SurfaceFlinger 在進行 GPU 合成時使用 RenderEngine。

Composition 流程：
```yaml
Layer buffer → (GL) RenderEngine → GPU composite → output buffer
```

Android 12+ 也可以用：
- Vulkan backend
- Skia GPU pipeline

---

## 8. DRM / KMS（HWC backend → Linux display）

HWC 最後會把合成好的 buffer 交給 DRM/KMS：
```yaml
HWC present()
↓
drmModeAtomicCommit()
↓
Plane / CRTC / Encoder / Connector
```

如果畫面顯示異常，debug 路徑一般如下：
```yaml
SurfaceFlinger → HWC → DRM → Panel
```

---

## 9. Debug 工具

| 工具 | 用途 |
| --- | --- |
| `dumpsys SurfaceFlinger` | 顯示 layer、fps、vsync 狀態 |
| `dumpsys SurfaceFlinger --latency` | frame latency 分析 |
| `dumpsys SurfaceFlinger --dispsync` | vsync 資訊 |
| `dumpsys window` | Window region、layer stack 來源 |
| `adb shell dumpsys gfxinfo` | App GPU profile |
| `/sys/kernel/debug/dri/0/state` | DRM/KMS 狀態（plane/crtc/connector） |
| systrace(composition) | 確認 SF / HWC composition 時序 |
| `atrace` graphics, hwcomposer | 追蹤顯示 pipeline |

---

## 10. 常見問題與排查

| 問題 | 可能原因 | 解決方式 |
| --- | --- | --- |
| 黑屏 | HWC present 失敗 / DRM mode set 錯誤 | 檢查 HWC log、DRM atomic commit |
| 閃屏 | double-buffer 錯誤 / fence 不一致 | 檢查 acquire/releaseFence |
| 撕裂 | vsync 錯誤或未同步 | 確保 HWC vsync 訊號正確 |
| 延遲高 | GPU composition 過多 | 檢查 layers 是否能 overlay |
| 某 layer 不更新 | buffer 未 queue / consumer 卡住 | 用 `dumpsys SurfaceFlinger` 查看 layer timeline |

---
## 11. Graphics threads 與 scheduler / cgroup / uclamp 的實際關係

### 11.1 Graphics pipeline 中的關鍵 threads

在一個典型互動畫面路徑中，真正影響 frame deadline 的 threads 包含：

| Thread | 所屬 process | 角色 |
|---|---|---|
| App UI thread | App process | 接收 input、觸發 draw |
| RenderThread | App process (HWUI) | GPU draw commands |
| SurfaceFlinger main thread | surfaceflinger | layer 收集與合成調度 |
| HWC callback thread | surfaceflinger / hwc | vsync / present |

這些 threads **不是對等的**，但它們通常：
- 位於 **top-app / foreground cgroup**
- 共享相似的 uclamp policy

---

### 11.2 cgroup 與 uclamp 對 graphics threads 的影響

以「前景互動 App」為例：

- App UI thread / RenderThread
- SurfaceFlinger thread

通常會被 framework 放入：

```text
/sys/fs/cgroup/top-app/
```

並套用：

-   較高的 `uclamp.min`
-   允許使用 big core
    

效果是：

-   thread 剛 wakeup 時，即使 util_avg 很低
-   scheduler 仍會選擇高效能 CPU
-   確保 frame 能在 vsync deadline 前完成
    

👉 這是 Android 能避免「首幀慢、動畫卡」的關鍵。

----------

### 11.3 為什麼 graphics pipeline 正確，畫面仍然會卡

常見情境：

-   BufferQueue 正常流動
-   HWC composition 正確
-   DRM atomic commit 成功
    

但仍有 jank。

**根本原因往往是 scheduler 層級問題**：

| 問題描述                               | 實際影響                                   |
|----------------------------------------|--------------------------------------------|
| Graphics thread 被放入 background cgroup | Scheduler 偏好小 core，導致效能不足         |
| uclamp.min 設定過低                    | Wakeup latency 增加，互動延遲明顯           |
| Thermal throttle 啟動                  | CPU 頻率受限，uclamp 設定無法有效發揮       |


這類問題 單看 graphics log 是看不出來的。


### 11.4 實戰 Debug：Graphics jank 從哪裡查

#### 1️⃣ 確認 thread 所屬 cgroup
```sh
ps -e -o pid,tid,comm,cgroup | grep surfaceflinger
```
#### 2️⃣ 檢查 uclamp 設定
```sh
cat /sys/fs/cgroup/top-app/uclamp.min cat /sys/fs/cgroup/top-app/uclamp.max
```
#### 3️⃣ 對照 scheduler trace 與 vsync

-   `atrace sched gfx hwcomposer`   
-   比對：
    -   thread wakeup
    -   實際 run time
    -   是否錯過 vsync
        
----------

### 11.5 BSP / vendor 常見踩雷點（graphics 專屬）

1.  **vendor kernel scheduler patch 與 uclamp 衝突** 
2.  **SurfaceFlinger thread priority 被改動**
3.  **thermal policy 過度保守，big core 無法拉頻**

---
## 12. 小結

Android 顯示管線是：
1. **App 渲染 → BufferQueue**
2. **SurfaceFlinger 收集 layer**
3. **HWC 決策 overlay / GPU composition**
4. **RenderEngine GPU 合成（必要時）**
5. **DRM/KMS 最終輸出到 Panel**
