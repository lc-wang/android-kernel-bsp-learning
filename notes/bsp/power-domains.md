
# BSP Power Domains（電源域與電源隔離整合實務）

> 本章定位：
> 
> -   站在 **BSP / SoC Bring-up Engineer** 視角，理解 power domain 在現代 SoC 中的角色與風險
>     
> -   說清楚為什麼「probe 成功、偶發消失、resume 後不動」常常是 power domain 問題
>     
> -   能實際用於 debug：runtime PM 異常、裝置隨機失效、低功耗後功能消失
>     

----------

## 1. 為什麼 Power Domain 是 BSP 的進階地雷區

在現代 SoC 中：

-   裝置不再只是「有 clock 就能動」
-   很多 IP 被包在 **獨立的 power domain** 內
-   power domain 的 on/off 可能完全獨立於 clock

👉 **power domain 問題通常不是立即爆炸，而是隨機或延遲出現。**

----------

## 2. Power Domain 的基本概念

### 2.1 什麼是 Power Domain

Power domain 是：

-   一組可以被獨立上電 / 斷電的硬體區塊

----------

### 2.2 Power Domain 與 Clock 的差異

-   Clock：控制時序
-   Power domain：控制是否供電 

可能狀況：
-   clock enable，但 domain 沒電
-   domain 有電，但 clock 沒開
    
👉 **兩者必須同時正確，裝置才會正常。**

----------

## 3. Linux Power Domain Framework 的角色

Linux 提供：

-   generic power domain (genpd)
    

它負責：
-   管理 domain on/off
-   與 runtime PM / system PM 整合
    
BSP 的責任是：
-   正確描述 domain 關係
-   確保 domain 拓撲正確

----------

## 4. Device Tree 中的 Power Domain 描述

### 4.1 power-domains 屬性

裝置節點常包含：

```dts
power-domains = <&pd_gpu>;
```

這代表：
-   裝置依賴該 power domain
    
----------

### 4.2 Domain 依賴關係錯誤的後果

若：
-   domain 漏描述  
-   domain 關係錯誤
    
結果可能是：
-   probe 成功
-   runtime 使用時才失敗

----------

## 5. Power Domain 與 Runtime PM 的關係

### 5.1 runtime PM 的假設

runtime PM 假設：
-   domain 可被安全 on/off   

若 domain 設定錯誤：
-   runtime suspend 會切錯電  

----------

### 5.2 常見錯誤行為

-   裝置閒置後再使用失敗    
-   第一次 OK，第二次壞掉

👉 **這類問題非常典型地指向 power domain。**

----------

## 6. Suspend / Resume 與 Power Domain

在 system suspend：
-   power domain 會被大量關閉

resume 時：
-   domain 必須依正確順序重新啟動
    
若順序錯誤：
-   裝置 resume 後無反應
    
----------

## 7. Power Domain Debug Toolbox

### 7.1 查看 power domain 狀態

```bash
ls /sys/kernel/debug/pm_genpd/
```

```bash
cat /sys/kernel/debug/pm_genpd/summary
```

觀察：

-   domain 是否被開啟
-   使用者（consumer）是誰

----------

### 7.2 驗證 runtime PM 行為

```bash
cat /sys/devices/.../power/runtime_status
```

狀態：
-   active
-   suspended

----------

### 7.3 suspend / resume 驗證

```bash
echo mem > /sys/power/state
```

resume 後確認 domain 是否恢復。

----------

## 8. 常見誤判與 Debug

| 現象           | 常見誤判      | 真正原因              |
|----------------|---------------|-----------------------|
| 裝置隨機失效   | Driver race   | Power domain 問題     |
| Runtime PM 壞掉| PM core 問題  | Domain 描述錯誤       |
| Resume 後消失  | Clock 問題    | Domain 未正確恢復     |

