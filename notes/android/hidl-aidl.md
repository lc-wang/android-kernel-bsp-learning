
# Android HAL IPC：HIDL 與 AIDL (Stable) 比較

這份筆記整理 Android HAL 層 IPC 的演進，  
從 **HIDL (HAL Interface Definition Language)** 到 **AIDL (stable)** 的轉換，  
說明 HAL 層如何透過 Binder 機制與 Framework 溝通。

---

## 1. 背景與設計目標

Android HAL（Hardware Abstraction Layer）負責連接 **Framework ↔ 驅動層**，  
其 IPC 架構經歷以下演進：

| 時期 | 架構 | 主要特點 |
| --- | --- | --- |
| Android 7 以前 | **Legacy HAL** (直接呼叫 `.so`) | 無版本控制、無 IPC 隔離、需重編系統 |
| Android 8 ~ 11 | **HIDL** | 透過 Binder IPC 封裝 HAL，支援多版本與 vendor 分離 |
| Android 12+ | **AIDL (stable)** | 統一 IPC 機制，支援穩定版本與跨分區更新 (Treble) |

💡 **設計目標**
- Framework 與 Vendor HAL 隔離（Treble 架構）  
- 使用 Binder 實現跨進程通訊  
- 提供介面版本管理與向後相容性  
- 確保 vendor 不需與 framework 同步更新即可運作  

---

## 2. HIDL 架構與運作原理

### 架構圖
```cscc
Framework (Java/C++)
↓
libhidl & hwservicemanager
↓
Binder Driver (/dev/hwbinder)
↓
Vendor HAL Daemon (C++)
```

| 元件 | 角色 | 功能 |
| --- | --- | --- |
| `libhidl` | IPC 封裝層 | 封裝 HIDL 介面與 binder transaction |
| `hwservicemanager` | HAL Service 註冊中心 | 類似 ServiceManager，用於 HAL |
| `/dev/hwbinder` | 驅動節點 | HIDL 專用 Binder 驅動 |
| HAL Daemon | Server 端 | 實作 HAL 邏輯，接受 framework 呼叫 |

---

### 範例：定義 HIDL 介面
```hidl
IExample.hal
package vendor.example.hardware.example@1.0;

interface IExample {
    oneway void helloWorld(string name);
};
```


編譯後產生：

-   `IExample.h` / `IExample.cpp`    
-   `IExampleServer.cpp`   
-   `IExampleClient.cpp`
執行流程：
1.  HAL server 註冊到 `hwservicemanager`。 
2.  Framework client 呼叫 `getService()` 取得介面。
3.  Binder 透過 `/dev/hwbinder` 傳遞交易。



## 3. AIDL (Stable) 架構與特點

AIDL 在 Android 12 後正式支援 **stable interface**，  
取代 HIDL 成為統一的 HAL IPC 解決方案。

### 架構圖
```scss
Framework (Java/C++)
   ↓
libbinder (AIDL Stable)
   ↓
Binder Driver (/dev/vndbinder)
   ↓
Vendor HAL Daemon (C++)
```

| 元件 | 角色 | 功能 |
| --- | --- | --- |
| **libbinder_ndk** | NDK 層 AIDL API | 讓 C/C++ HAL 使用 Binder IPC。 |
| **/dev/vndbinder** | 驅動節點 | Vendor 專用 Binder 通道，用於 AIDL HAL 通訊。 |
| **vndservicemanager** | Vendor 層註冊中心 | 管理所有 AIDL HAL 服務的註冊與查詢。 |
| **.aidl interface** | 介面定義 | 定義穩定的 HAL API 與版本，支援跨分區更新。 |



範例：AIDL Stable 介面
```aidl
// IExample.aidl
package vendor.example.hardware.example;

@VintfStability
interface IExample {
    void helloWorld(String name);
}
```

**編譯輸出**  
使用 `aidl_interface` in `Android.bp`：
``` bp
aidl_interface {
    name: "vendor.example.hardware.example",
    vendor_available: true,
    srcs: ["IExample.aidl"],
    stability: "vintf",
}
```
產出檔案：
-   `IExample.h` / `IExample.cpp`    
-   `IExampleService.cpp` 
-   `IExampleClient.cpp`

💡 **補充說明：**
-   `libbinder_ndk` 位於 `frameworks/native/libs/binder/ndk/`，  
    是 HAL 層最常用的 AIDL NDK 封裝 API。
-   `/dev/vndbinder` 是 Vendor 專用通道，與 `/dev/hwbinder`、`/dev/binder` 相對應。
-   `vndservicemanager` 在 vendor 分區啟動，負責管理 HAL IPC。
-   `.aidl` 檔案必須標註 `@VintfStability` 以確保版本穩定性。

## 4. HIDL → AIDL 差異比較

| 項目 | HIDL | AIDL (Stable) |
| --- | --- | --- |
| **定義語言** | `.hal` | `.aidl` |
| **IPC 通道** | `/dev/hwbinder` | `/dev/vndbinder` |
| **Service Manager** | `hwservicemanager` | `vndservicemanager` |
| **使用層級** | HAL (C++) | HAL / Framework (C++, Java) |
| **編譯系統** | `hidl-gen` + `Android.bp` | `aidl_interface` + Soong |
| **版本控制** | `@1.0 / @1.1` | `@VintfStability` |
| **跨分區支援** | 部分 | 完整（system/vendor 分離） |
| **向後相容性** | 複雜（需維護多版本 interface） | 透過穩定 AIDL 自動維持版本相容性 |

💡 **補充說明**

-   HIDL 採「介面多版本」策略，例如 `@1.0`, `@1.1`，每次新增都需維護多套檔案。
-   AIDL (Stable) 改以 **VINTF 穩定性模型** 控制版本，讓 framework/vendor 可獨立升級。
-   Android 12 起，Google 已正式建議 **所有新 HAL 改用 AIDL-stable**。

## 5. Interface 定義與整合

### Android.bp 範例

```bp
aidl_interface {
    name: "android.hardware.light",
    srcs: ["ILights.aidl"],
    stability: "vintf",
    backend: {
        cpp: {
            enabled: true,
        },
        ndk: {
            enabled: true,
        },
    },
}
```
### 啟動與註冊流程
1.  HAL Daemon 啟動後呼叫 `IExample::addService()`。   
2.  `vndservicemanager` 登錄 HAL。
3.  Framework 使用 `IExample::getService()` 取得連線。

## 6. Debug 與驗證方法

| 工具 / 節點 | 功能說明 |
| --- | --- |
| `lshal` | 顯示所有 HIDL / AIDL HAL 服務與版本。 |
| `ps -A \| grep hwservicemanager` | 確認 HAL 管理進程是否啟動。 |
| `service list` | 檢查 Framework Binder 服務註冊狀態。 |
| `/dev/hwbinder`, `/dev/vndbinder` | 驗證 HAL IPC 通道是否存在。 |
| `strace -p <pid>` | 追蹤 HAL Daemon 的 Binder ioctl 活動。 |
| `vndservicemanager --list` | 列出目前 Vendor 層 AIDL 服務。 |

💡 **補充說明**

-   `lshal` 是最直觀的 HAL 層檢查工具，可同時列出 HIDL 與 AIDL HAL。
-   `hwservicemanager` 與 `vndservicemanager` 是分層的 service manager（system/vendor 分離）。
-   若要查看 AIDL HAL 是否運作，可用：
    `adb shell vndservicemanager --list` 
-   若要查看 binder 通道是否建立：
    `adb shell ls -l /dev/*binder*`


## 7. 常見問題與最佳實踐

| 問題 | 可能原因 | 修正建議 |
| --- | --- | --- |
| **HAL 無法啟動** | `.rc` 啟動腳本錯誤或權限不足 | 檢查 `init.vendor.rc` 與 SELinux policy。 |
| **HIDL 服務找不到** | 未註冊到 `hwservicemanager` | 確認 server 呼叫 `registerAsService()`。 |
| **AIDL service 無法查詢** | vendor/system binder 通道未對應 | 驗證是否使用正確的 `/dev/vndbinder`。 |
| **HAL crash** | 介面版本不相容或 interface 定義錯誤 | 對齊 Framework 對應的 interface 版本。 |
| **轉換至 AIDL-stable 失敗** | Soong 檔未設定 `stability: "vintf"` | 補上設定並重新生成介面。 |

💡 **補充說明**

-   `registerAsService()` 是所有 HIDL HAL 的註冊入口。 
-   AIDL-stable HAL 若設定錯誤，`vndservicemanager --list` 中不會出現對應服務。
-   可使用以下指令檢查啟動狀態：
``` bash
adb shell dmesg | grep binder
adb shell vndservicemanager --list
```
-   若 AIDL HAL 轉換後 crash，可比較 `.aidl` 與原 `.hal` 的參數型別差異。


## 8. 學習建議
1.  使用 `lshal` 比對 HIDL 與 AIDL HAL 的差異。
2.  嘗試撰寫一個最小 HAL 專案（AIDL 版）並測試 IPC。
3.  閱讀範例：`hardware/interfaces/` 與 `aidl/` 目錄。
4.  在 `vndservicemanager` 啟動後觀察 `/dev/vndbinder` 活動。
5.  分析從 Framework 呼叫 HAL 的 binder transaction 流程。

📘 **延伸閱讀**

-   `hardware/interfaces/`（HIDL 原始碼）
-   `aidl/`（AIDL stable 原始碼）
-   Google: Convert HALs from HIDL to AIDL
-   `system/tools/aidl`

