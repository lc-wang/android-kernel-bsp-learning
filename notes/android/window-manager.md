
# Android Window Manager（WMS）系統解析

> 本章定位：
> 
> -   站在 **Android System / BSP Engineer** 視角，理解 WindowManagerService（WMS）在整個系統中的角色
>     
> -   說清楚 **WMS 與 ActivityManagerService（AMS）為何高度耦合，但職責不同**
>     
> -   能實際用於 debug：點不到、視窗異常、前後景判斷錯誤、input / 顯示問題
>     

----------

## 1. 為什麼 Android 需要 WindowManager

在 Android 系統中：

-   **AMS 決定誰重要**（process / task importance）
-   **WMS 決定誰可見**（window / task visibility）
    

這兩者不是重疊，而是**分工**。

關鍵前提是：

> **系統不可能根據「邏輯狀態」判斷 UX，只能根據「實際顯示結果」。**

而「實際顯示結果」的唯一權威來源，就是 WMS。

----------

## 2. WMS 在 system_server 中的位置

WMS 運行於：

```text
system_server
 └─ WindowManagerService
```

它的責任不是畫畫面，而是：

-   管理 window 的建立、銷毀與層級
-   維護「畫面上發生了什麼」的事實
-   對其他 subsystem（AMS / Input）提供可見性資訊
    

真正的繪製與合成：

-   由 App / RenderThread
-   由 SurfaceFlinger 負責
    

👉 **WMS 是「顯示事實管理者」，不是渲染者。**

----------

## 3. Task：WMS 與 AMS 的共同抽象

理解 WMS，必須從 **Task** 開始。

### 3.1 為什麼不能只用 Window

單一 window 無法表示：

-   一個 App 的完整使用流程
-   back stack
-   分割畫面 / PIP

因此 Android 引入 **Task** 作為上層抽象。

----------

### 3.2 WMS 中的 Task 與 WindowContainer

在 WMS 中：

-   Task 是一個 **WindowContainer**
-   內部包含多個 window / surface
-   Task 本身有：
    -   visibility
    -   z-order
    -   focus 狀態
        
簡化結構如下：

```text
Display
 └─ Task
     ├─ Window A
     ├─ Window B
     └─ Window C
```
----------

### 3.3 與 AMS 的對應關係

這裡是最關鍵的一點：

> **WMS 與 AMS 操作的是同一個 Task，只是關心的面向不同。**

| 元件 | 關心的重點                               |
|------|------------------------------------------|
| WMS  | Task 是否可見、是否位於前景               |
| AMS  | Task 對應的 Process 是否重要              |

WMS 提供「可見性事實」，  
AMS 根據這些事實做出系統決策。

----------

## 4. Window 可見性如何影響系統決策

### 4.1 Visibility 是系統級訊號

在 Android 中：

-   Window 是否可見 
-   比 Activity lifecycle 更直接反映 UX
    

例如：

-   Activity technically resumed 
-   但 window 被遮住
    

👉 對使用者來說，這仍然是「不可見」。

----------

### 4.2 實際決策鏈

```text
Window visibility 改變
    ↓
WMS 更新 Task 狀態
    ↓
WMS 通知 AMS
    ↓
AMS 重新計算 Task / Process importance
```

這也是為什麼：

-   split-screen
-   overlay window
-   PIP
    

都會影響 App 是否被視為前景。

----------

## 5. WMS 與 Input 系統的關係

### 5.1 為什麼 Input 一定要經過 WMS

InputDispatcher 在派送事件前，必須知道：

> 「這個座標落在哪個 window？」

而只有 WMS 知道：

-   window 的實際位置
-   z-order
-   哪個 window 在最上層

----------

### 5.2 Input routing 流程

```text
Input event
   ↓
InputDispatcher
   ↓
詢問 WMS：window / task / focus
   ↓
事件派送到對應 process
```

如果 WMS 狀態錯誤，

-   input 可能送錯 App
-   或被判定為 timeout（Input ANR）
    

----------

## 6. WMS 與 Graphics / SurfaceFlinger

### 6.1 WMS 不負責繪製

WMS 的責任是：

-   建立 window 與 surface 的關係
-   決定 layer 的層級
    
實際合成由 SurfaceFlinger 完成。

----------

### 6.2 Task 與 Layer 的關係

SurfaceFlinger 合成的是：
-   來自不同 Task 的 layer

WMS 提供：
-   layer hierarchy
-   visibility
-   z-order
    
👉 **Graphics pipeline 依賴 WMS 提供正確的畫面結構。**

----------

## 7. 常見 WMS 問題與 Debug 方向

### 7.1 常見問題類型

| 現象             | 可能原因                         |
|------------------|----------------------------------|
| 點不到畫面       | Window Z-order 錯誤               |
| App 被當成背景   | Visibility 判斷錯誤               |
| Input ANR        | Window state 更新延遲             |

----------

### 7.2 Debug 工具

```bash
dumpsys window
dumpsys window windows
dumpsys window displays
```

觀察重點：

-   Task / window 可見性 
-   focus
-   window 層級
