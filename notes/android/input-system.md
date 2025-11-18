
# Android Input System 解析（InputReader / InputDispatcher / IMS / WMS）

本章說明 Android 的輸入系統架構，包括觸控、鍵盤、滑鼠等所有輸入事件的流程。  
流程包含 Kernel → EventHub → InputReader → InputDispatcher → InputChannel → App。

理解此流程對於分析觸控延遲、輸入異常、手勢行為及 WMS 問題非常重要。

---

## 1. 整體架構總覽
```yaml
Linux Kernel (evdev)
↓
EventHub
↓
InputReader (分類 / 正規化)
↓
InputDispatcher (決定目標視窗)
↓
InputChannel / Looper
↓
App (View / Activity)
```


---

## 2. Kernel → EventHub

Linux input 子系統透過 `/dev/input/eventX` 裝置提供事件。  
Android 的 EventHub 用來輪詢所有 input 裝置。

### EventHub 主要任務
| 任務 | 說明 |
| --- | --- |
| 掃描 /dev/input/ | 搜尋可用的輸入裝置 |
| open input fd | 打開 eventX 裝置 |
| epoll 監聽事件 | 非阻塞監聽所有輸入事件 |
| 傳遞事件給 InputReader | 包含 type, code, value |

程式位置：
```yaml
frameworks/native/services/inputflinger/EventHub.cpp
```
---

## 3. InputReader（事件分類與正規化）

InputReader 是 InputFlinger 的 “前半段”。  
它負責**解析 raw event** 並轉換成標準化的 Android Motion / Key 事件。

### 任務
| 任務 | 說明 |
| --- | --- |
| 裝置分類 | Touchscreen, Keypad, Mouse |
| Multi-touch 合併 | 追蹤多手指 pointer ID |
| Gesture 分析 | down、move、up、hover |
| State machine | 保留觸控狀態 |
| 產生 CookedEvent | 分析後的標準化事件 |

### 多點觸控流程
```c
EV_ABS (ABS_MT_POSITION_X/Y)
EV_KEY (BTN_TOUCH)
EV_SYN (SYN_REPORT)
```
對應到：
```c
AMOTION_EVENT_ACTION_DOWN
AMOTION_EVENT_ACTION_MOVE
AMOTION_EVENT_ACTION_UP
```

程式路徑：
```yaml
frameworks/native/services/inputflinger/InputReader.cpp
```
---

## 4. InputDispatcher（決定目標視窗）

InputDispatcher 會將 InputReader 產生的 CookedEvent  
送到適當的視窗（Window）。

### Dispatcher 主要任務

| 任務 | 說明 |
| --- | --- |
| Window focus | 決定哪個 window 要接收事件 |
| ANR timeout 檢查 | App 無回應時觸發 ANR |
| Pointer → Window routing | 依顯示座標或 region 分配事件 |
| Dispatch 給 App | IPC 透過 InputChannel |

### 事件分派邏輯
```yaml
觸控位置 → 查詢 WMS 中的 window region
↓
找到 top-most window
↓
送事件給該 Window 的 InputChannel
```
---

## 5. InputManagerService（IMS）

Java 層的 InputManagerService 是整個 input 系統的管理者。

流程：
```java
SystemServer.startOtherServices()
→ new InputManagerService()
→ nativeInit()
→ 啟動 InputReaderThread / InputDispatcherThread
```
位置：
```yaml
frameworks/base/services/core/java/com/android/server/input/InputManagerService.java
```
### IMS 功能
| 功能 | 說明 |
| --- | --- |
| 管理所有 input 裝置 | 透過 JNI 呼叫 native InputFlinger |
| 變更 pointer speed、gesture 設定 | 設定傳遞到底層 |
| 與 WMS 協作 | Window 接收事件前都需向 WMS 詢問 focus |
| ANR 監測 | 事件未處理時通知 AMS |

---

## 6. InputChannel（App 接收事件）

InputDispatcher 透過 socketpair 形式的 **InputChannel**  
將事件送到 App 的主執行緒。

App 端：
```shell
ActivityThread → ViewRootImpl → InputEventReceiver → View
```

### InputChannel 橋接 IPC
```shell
InputDispatcher → InputChannel.write()
App → InputChannel.read() via Looper
```
這是 native Binder 之外的另一種 IPC 機制。

---

## 7. WMS 與 Input 系統整合

WMS（WindowManagerService）決定：
- 哪個 Window 取得焦點
- 該 Window 是否可接收輸入事件
- 是否需要攔截（如 lock screen、IME、dialog）

事件流程整合如下：
```yaml
InputDispatcher
↓ (查詢視窗)
WMS.findTouchedWindow()
↓
目標 InputChannel
↓
App View 系統
```
WMS 扮演 routing 的決策者。

---

## 8. ANR（輸入無回應）

InputDispatcher 會檢查 App 是否在時間內處理事件。

| 類型 | Timeout |
| --- | --- |
| Key event | 5 秒 |
| Touch event | 5 秒 |
| App switch | 10 秒 |

當超時：
```yaml
InputDispatcher → reportANR()
→ AMS → 處理 ANR 對話框
```
---

## 9. Debug 工具與方法

| 工具 / 節點 | 說明 |
| --- | --- |
| `getevent` | 查看 raw input event |
| `dumpsys input` | 顯示所有 input 裝置、reader、dispatcher 狀態 |
| `dumpsys window` | 查看 window region 與焦點 |
| `/dev/input/eventX` | raw Linux input 裝置 |
| systrace "input" tag | 追蹤 input latency |
| `adb shell input` | 模擬點擊、滑動、key 事件 |

常見除錯方向：
- 多點觸控混亂 → 驗證 ABS_MT slot 與 tracking ID  
- App 無回應輸入 → Dispatcher wait queue 堵塞  
- 點擊到錯視窗 → WMS region 判斷錯誤  
- 觸控延遲 → event batching、display pipeline 延遲  

---

## 10. 常見問題與排查

| 問題 | 可能原因 | 修正方式 |
| --- | --- | --- |
| app 收不到觸控事件 | window 沒有 focus | 檢查 WMS / ViewRootImpl |
| 多點觸控亂跳 | driver 上報 ABS_MT 順序錯誤 | 驗證 input driver |
| 觸控延遲 | dispatcher thread block | 用 `dumpsys input` 查看 queue |
| event 掉事件 | InputReader 過載 | 調低 event 時序或排查 drop log |
| ANR | App 主執行緒卡住 | 分析 main thread Looper stack |

---

📘 **延伸閱讀**
- frameworks/native/services/inputflinger  
- frameworks/base/services/core/java/com/android/server/input  
- input 子系統：`Documentation/input/`  
