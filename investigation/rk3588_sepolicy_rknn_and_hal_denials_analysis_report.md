
# Android SEPolicy 問題分析與修正報告


_(Rockchip RK3588 rknn_server + HAL binder denied)_

## 📘 一、背景說明

在 Rockchip RK3588 Android BSP 開發過程中，  
遇到兩類 SELinux 權限拒絕 (AVC denied) 問題：

1.  **rknn_server（NPU Daemon）讀取 default_prop 被 AOSP neverallow 擋住**
    
2.  **HDMI HAL 嘗試透過 binder 呼叫 Camera HAL 時被拒絕**
    

這兩類問題皆導致：

-   kernel log/dmesg 出現 AVC denied
    
-   sepolicy 編譯階段（secilc）直接失敗
    
-   無法繼續 bring-up / 測試功能
    

本文件記錄分析流程、原因與最終採用的修正方法。

----------

# 📍 二、問題 1：rknn_server 讀取 default_prop 被全域 neverallow 擋住

## 🔥 現象 (AVC Log)

```bash
type=1400 audit: avc: denied { read } for comm="listener"  scontext=u:r:rknn_server:s0 tcontext=u:object_r:default_prop:s0 tclass=file
```


__這表示 rknn_server 嘗試讀取系統屬性（ro._ / persist._），但無權限。**

----------

## 🔥 編譯期錯誤 (secilc neverallow violation)

```bash
neverallow check failed: neverallow base_typeattr_223 default_prop  (file (read open ...)) violated by allow rknn_server default_prop  (file (read open));
``` 

----------

# 📘 三、Root Cause（問題根本原因）

## ✅ 1. rknn_server 是 vendor domain

→ 基於 Treble 安全模型，**vendor domain 禁止讀取 default_prop**（system property namespace）。

## ✅ 2. AOSP 定義永不可覆寫的 neverallow

位置於：

-   `system/sepolicy/private/property.te:141`
    
-   `system/sepolicy/public/property.te:273`
    

內容概念如下：
```bash
neverallow { vendor domains } default_prop:file { read write open ... }
```

**任何 allow rule 都會被擋住（無條件 fail）。**

## ✅ 3. rknn_server 是 closed-source

→ 你無法修改它讓它讀 `ro.vendor.rknn.*` 這類合法的 vendor namespace 屬性。

----------

# 📘 四、為何「正規 allow rule」無法解決？

因為 **AOSP neverallow 是硬限制（强制不可繞過）**：

-   不會被 allow rule 覆蓋
    
-   不會被 typeattribute 覆蓋
    
-   不會因 sepolicy layering 而放寬
    

除非 **改 AOSP 的 private sepolicy（不可能）**，否則永遠無法通過。

因此，對閉源 rknn_server：

✅ **你無法使用正規 allow 解決這問題**  
❌ **你無法修改 default_prop 給它讀**  
❌ **你無法 override neverallow**

----------

# 📘 五、可行的解法選項（分析）


| 解法 | 可行？ | 優點 | 缺點 |
|------|--------|--------|--------|
| 修改 rknn_server source，使其讀 vendor namespace（ro.vendor.rknn.*） | ❌（無 source） | 最乾淨、完全符合 AOSP sepolicy | 不可行，binary 為閉源 |
| 編寫 allow rule 給 default_prop | ❌（AOSP neverallow 阻止） | 無 | sepolicy 編譯一定失敗，無法繞過 neverallow |
| 修改 property_contexts：將 ro.rknn.* 映射到 vendor_rknn_prop | ⚠️ 可行但 risky | 不違反 neverallow、仍符合 Treble 規範 | 需知道 daemon 讀的所有 property；若漏掉則仍會 denied |
| 使用 LD_PRELOAD propshim（在 libc 層改寫 property key） | ✅ | 100% 合法、不觸發 neverallow、可過編譯 | 需維護額外 .so（libpropshim） |
| 將 rknn_server 設為 permissive domain | ✅ | 立即可用、最簡單、最快解法 | 不安全、非正式、不適合量產 |



----------

# ✅ 六、最終採用解法：permissive rknn_server（因無需通過 VTS）


> **我們不需要通過 GTS/VTS，且只作為內部功能測試。**

因此選擇：

### ✅ 設定 rknn_server 為 permissive domain

**修改：`rknn_server.te`**

`permissive rknn_server;` 

效果：

-   所有 rknn_server 的 denied 變成 permissive（允許）
    
-   kernel 不再阻斷 NPU 功能
    
-   sepolicy 編譯成功
    
-   不需修改 binary、不需 propshim
    

----------

# 📘 七、風險與建議


| 項目 | 說明 |
|------|------|
| 安全性 | rknn_server domain 內所有 denied 都會被允許（permissive），SELinux 對該 domain 等同失效 |
| 適用場景 | internal BSP / bring-up / demo / non-GMS build |
| 不適用場景 | 量產、商用產品、GMS 認證、需通過 VTS/GTS 的版本 |
| 建議 | 若未來要進入生產或正式商用 → 改用 propshim（LD_PRELOAD key rewrite）以符合正規 sepolicy 要求 |



----------

# 📍 八、問題 2：HDMI HAL → Camera HAL Binder call denied

## 🔥 Log
```bash
avc: denied  { call } for scontext=u:r:hal_hdmi_default:s0
tcontext=u:r:hal_camera_default:s0
tclass=binder
``` 

## ✅ 原因

HAL 之間的 binder 呼叫**預設不允許 cross-HAL 呼叫**，  
必須顯示定義 allow。

## ✅ 正規修正（最小必要權限）

**修改：`hal_hdmi_default.te`**

`allow hal_hdmi_default hal_camera_default:binder call;` 

若後續 log 有 `{ transfer }`：

`allow hal_hdmi_default hal_camera_default:binder { call transfer };` 

這是完全合法的 SELinux allow，  
不會觸發 neverallow。

----------

# ✅ 九、最終整合結果


| 項目 | 採用修正 | 狀態 |
|------|----------|--------|
| rknn_server default_prop 問題 | permissive rknn_server; | ✅ 已解決 |
| HDMI HAL binder call denied | allow rule | ✅ 已解決 |
| sepolicy 編譯 | 成功 | ✅ |
| 系統功能 | rknn OK / HDMI OK |✅ |

