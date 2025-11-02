# Linux Power Management Overview 
這份筆記整理 Linux kernel 電源管理 (Power Management, PM) 架構，  
涵蓋系統層與裝置層的 suspend/resume、runtime PM、以及 clock/regulator 等相關機制。

---
## 1. 電源管理的目的
- **節能與效能平衡**：在不影響功能的前提下降低功耗。  
- **動態控制資源**：根據狀態啟用或關閉裝置、時鐘、電源域。  
- **常見目標**：
  - 減少 idle 功耗
  - 延長電池續航
  - 降低熱量與功耗峰值

--- 

## 2. 電源管理架構層級

| 層級 | 元件 / 框架 | 功能說明 |
| --- | --- | --- |
| **User Space** | systemd, Android PowerHAL, userspace tools | 發出 suspend/resume 或 runtime power 請求 |
| **Kernel Power Management Core** | PM core, Runtime PM, PM QoS, Wakeup Source | 負責核心層電源管理控制與狀態協調 |
| **Device / Subsystem Layer** | Device drivers, Subsystems (I2C, SPI, PCI, USB) | 個別裝置的 suspend/resume、runtime 控制 |
| **SoC Resource Layer** | Clock, Regulator, Power Domain (genpd) | 控制實體電源、電壓、時鐘、電源域開關 |
| **Hardware** | SoC, peripheral blocks | 實際進入低功耗或斷電狀態 |

 --- 
 ## 3. 系統層 Suspend/Resume
系統進入休眠或喚醒時，kernel 會通知所有 device driver 執行對應 callback。

- **主要狀態**
  - `PM_SUSPEND_TO_IDLE` (light sleep)
  - `PM_SUSPEND_MEM` (deep sleep)
  - `PM_SUSPEND_DISK` (hibernate)

- **驅動 callback**
```c
  static const struct dev_pm_ops my_pm_ops = {
      .suspend = my_suspend,
      .resume  = my_resume,
      .freeze = my_freeze,
      .thaw    = my_thaw,
  };

  static struct platform_driver my_driver = {
      .probe = my_probe,
      .remove = my_remove,
      .driver = {
          .name = "my_device",
          .pm = &my_pm_ops,
      },
  };
```
-   **流程概念**
    
    1.  user space 呼叫 `echo mem > /sys/power/state`
        
    2.  kernel 發送 suspend event 給每個 driver
        
    3.  driver 停止 DMA / IRQ、儲存狀態
        
    4.  SoC 進入低功耗模式
        
    5.  resume 時按相反順序恢復
        

----------

## 4. Runtime PM (動態電源管理)

-   **用途**：在裝置閒置時關閉電源或 clock，而不影響整體系統運作。
    
-   由 subsystem 或 driver 自主決定何時 idle、何時 active。
    
-   **API 範例**
```c
pm_runtime_enable(dev); 
// 當裝置需要使用時 
pm_runtime_get_sync(dev); 
// 當閒置時 
pm_runtime_put_sync(dev);
pm_runtime_disable(dev);
```
    
-   **常見搭配**
    
    -   regulator_disable()
        
    -   clk_disable_unprepare()
        
    -   disable_irq()
        
    -   關閉 peripheral power domain
        

----------

## 5. Clock Framework

-   每個驅動使用的 clock 都應透過 **common clock framework (CCF)** 管理。
    
-   **主要 API**
```c
struct  clk *clk = devm_clk_get(dev, "core");
clk_prepare_enable(clk);
clk_disable_unprepare(clk);
``` 
    
-   **目的**
    
    -   統一管理 SoC 各模組的 clock source。
        
    -   避免多 driver 同時操作 clock register。
        

----------

## 6. Regulator Framework

-   管理裝置電源供應 (power rail)。
    
-   **主要 API**
```c
struct  regulator *vcc = devm_regulator_get(dev, "vcc");
regulator_enable(vcc);
regulator_disable(vcc);
regulator_get_voltage(vcc);
```
    
-   可與 Device Tree 結合：
```dts
my_device@0 {
	vcc-supply = <&reg_3v3>;
};
```
    

----------

## 7. Power Domain (genpd)

-   **Generic PM Domain (genpd)**：讓 driver 能在不同電源域之間共享/控制電源狀態。
    
-   **常見於 SoC**：例如 GPU, VPU, ISP, Display domain。
    
-   **驅動整合方式**
    
    -   在 DTS 宣告：
```dts
my_device@0 {
	power-domains = <&pd_vpu>;
};
```     
-   核心會自動呼叫 `pm_genpd` 介面控制該域。
        

----------

## 8. Wakeup Source

-   某些裝置 (例如 GPIO、RTC、USB) 需要在系統休眠時喚醒系統。
    
-   使用 `device_init_wakeup()` 啟用：
    
    `device_init_wakeup(dev, true);` 
    
-   驅動可在中斷 handler 中呼叫：
    
    `pm_wakeup_event(dev, 0);` 
    

----------

## 9. PM QoS (Power Management Quality of Service)

-   提供介面讓 driver 或 user space 要求特定功耗/延遲條件。
    
-   **用途**
    
    -   限制 CPU latency (避免睡太深)
        
    -   設定最低頻率
        
    -   防止 suspend
        
-   **介面**
```bash
cat /dev/cpu_dma_latency
echo 0 > /dev/cpu_dma_latency
```
    

----------


## 10. Debug 工具
| 工具 / 節點 | 用途 |
| --- | --- |
| `/sys/power/state` | 控制系統 suspend 模式 (`freeze`, `mem`, `disk`) |
| `/sys/kernel/debug/pm_debug` | 顯示 PM 事件與裝置電源狀態 |
| `/sys/kernel/debug/clk/` | 檢查時鐘啟用狀態與使用者 |
| `/sys/kernel/debug/regulator/` | 顯示電壓供應器狀態與目前電壓值 |
| `/sys/kernel/debug/wakeup_sources` | 列出所有 wakeup source 及統計資訊 |
| `powertop` | 分析功耗與喚醒事件頻率 |
| `trace-cmd record -e power:*` | 追蹤 suspend/resume 事件時序 |
| `dmesg | grep PM:` | 查看 suspend/resume 相關 kernel log |
----------


## 11. 常見問題與調試思路

| 問題 | 可能原因 | 檢查與建議 |
| --- | --- | --- |
| 系統無法進入 suspend | 某些裝置未進入 idle 狀態 | 檢查 `/sys/kernel/debug/pm_debug`、確認 driver suspend callback 是否正確實作 |
| 無法 resume | wakeup source 設定錯誤或缺失 | 查看 `/sys/kernel/debug/wakeup_sources`，確認正確來源是否可喚醒 |
| suspend 時 kernel panic | IRQ 未 disable 或 DMA 未停止 | 檢查驅動 suspend 流程中是否正確關閉中斷與 DMA |
| runtime PM 無效 | driver 未呼叫 `pm_runtime_put()` 導致參考計數不歸零 | 驗證 idle 條件與 runtime PM 使能邏輯 |
| 裝置 resume 後異常 | 未重新初始化 clock/regulator | 驅動 resume 階段補上重新啟用動作 |
----------

## 12. 學習建議

1.  用 `echo mem > /sys/power/state` 測試 suspend/resume，觀察 dmesg。
2.  嘗試在 driver 中加上 `.suspend` / `.resume` callback 並打印 log。
3.  觀察 `debugfs/clk` 與 `debugfs/regulator` 狀態，理解裝置依賴關係。
4.  分析真實驅動 (如 MMC、I2C、WiFi) 的 power management flow。
5.  了解 Android 上的 PowerHAL、wakelock 與 kernel wakeup source 的對應關係。
----------

📘 **延伸閱讀**

-   `Documentation/power/runtime_pm.rst`  
-   `Documentation/power/devices.rst`
-   `Documentation/power/genpd.rst` 
-   `Documentation/power/regulator/overview.rst`
-   Android: `wakelock`, `suspend blocker` 機制
