
# GPIO Debug Playbook


# 🎯 GPIO 問題的本質

所有 GPIO 問題，本質只會是以下其中之一：

1️⃣ pin 沒 mux 成 gpio  
2️⃣ polarity 錯誤（active-low）  
3️⃣ 被 hog / driver 佔用  
4️⃣ open-drain / pull-up 設錯  
5️⃣ regulator 沒 enable  
6️⃣ interrupt domain 設錯

----------

# 🧭 總體 Debug 流程
```
Step 1  → 確認 DT 正確  
Step 2  → 確認 pinctrl mux  
Step 3  → 確認 gpio controller 註冊  
Step 4  → 確認 line 是否被佔用  
Step 5  → 用 libgpiod 手動拉  
Step 6  → 用示波器驗證  
Step 7  → trace driver 行為
```

----------

# 🔎 Case 1：Reset 拉不起來

## Step 1️⃣ 檢查 Device Tree
```
dtc -I fs /sys/firmware/devicetree/base
```
確認：
```
reset-gpios = <&gpio3 5 GPIO_ACTIVE_LOW>;
```
是否存在。

----------

## Step 2️⃣ 檢查是否 active-low 搞錯

用：
```
gpioinfo gpiochipX
```
查看：
```
active-low
```
然後測試：
```
gpioset gpiochipX 5=1  
gpioset gpiochipX 5=0
```
⚠ 如果 active-low：

1 = physical low

----------

## Step 3️⃣ 檢查 pinctrl
```
ls /sys/kernel/debug/pinctrl/
```
查看該 pin：

-   是否 function = gpio
    
-   是否被其他 driver 佔用
    

如果 pin 還在：
```
i2c mode / pwm mode / dsi mode
```
那 GPIO 一定無效。

----------

## Step 4️⃣ 檢查 hog
```
cat /sys/kernel/debug/gpio
```
如果看到：
```
gpio-XX (panel-enable) hogged
```
代表：

-   user space 不能 request
    
-   driver 也不能 request
    

----------

## Step 5️⃣ 示波器驗證

不要相信軟體。

實際量：

-   是否真的變電平
    
-   是否 open drain
    
-   是否 drive strength 太低
    

----------

# 🔎 Case 2：WiFi Power 拉不起來

常見情況：

-   regulator 沒 enable
    
-   power sequence 錯
    
-   mmc driver 先 claim GPIO
    

----------

## 檢查 regulator
```
cat /sys/kernel/debug/regulator/regulator_summary
```
如果：
```
wifi_vdd disabled
```
那 GPIO 拉高也沒用。

----------

# 🔎 Case 3：gpiomon 沒事件

## Step 1️⃣ 確認 DT IRQ
```
interrupt-parent = <&gpio3>;  
interrupts = <5 IRQ_TYPE_LEVEL_LOW>;
```
----------

## Step 2️⃣ 檢查 controller 是否 interrupt-controller
```
gpio-controller;  
interrupt-controller;  
#interrupt-cells = <2>;
```
----------

## Step 3️⃣ 確認 /proc/interrupts
```
cat /proc/interrupts
```
看是否有對應 GPIO IRQ。

----------

## Step 4️⃣ 確認 trigger type

很多問題出在：
```
LEVEL_LOW vs EDGE_FALLING
```
設定錯誤 → 永遠不觸發。

----------

# 🔎 Case 4：GPIO 設了但硬體不動

可能原因：

### 1️⃣ pin 還在 alternate function

最常見。

### 2️⃣ open drain 沒 pull-up

如果：
```
GPIO_OPEN_DRAIN
```
但板子沒外部 pull-up，

高電平永遠上不去。

----------

### 3️⃣ drive strength 太弱

某些 SoC 預設：
```
2mA drive
```
推不動外部電路。

----------

# 🧠 進階 Debug：Trace Kernel

----------

## 查看 gpiod request

加 dynamic debug：
```
echo  'file drivers/gpio/* +p' > /sys/kernel/debug/dynamic_debug/control
```
查看：
```
gpiod_request  
gpiod_direction_output
```
----------

## ftrace
```
echo  function > /sys/kernel/debug/tracing/current_tracer  
echo gpiod_set_value > set_ftrace_filter
```
可以看到誰在操作 GPIO。

----------

# 🔎 Case 5：Driver probe 失敗

如果：
```
reset  =  devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
```
失敗：

可能原因：

-   reset-gpios 名字錯
    
-   #gpio-cells 錯
    
-   phandle 錯
    
-   GPIO controller 尚未 probe
    

----------

# 🧰 標準 GPIO Bring-up Checklist


| 項目 | 檢查重點 |  
|-----------------|--------------------------------------------|  
| DT 定義 | reset-gpios 等 GPIO phandle 設定正確 |  
| flags | polarity（GPIO_ACTIVE_LOW / HIGH）正確 |  
| #gpio-cells | 與 GPIO controller binding 定義一致 |  
| pinctrl | pinmux 已切換為 GPIO 功能 |  
| hog | 無其他 hog 佔用或衝突 |  
| regulator | 相關電源已 enable |  
| interrupt | interrupt domain 與 parent 設定正確 |  
| drive strength | 驅動強度足夠符合硬體需求 |

----------

# 總結

GPIO 問題 80% 不是 GPIO。

而是：

-   pinctrl
    
-   regulator
    
-   power sequence
    
-   clock enable
    
-   reset timing
    

----------

# 🧩 GPIO + Regulator + Reset Sequence 模型

標準 reset 流程應該是：
```
enable regulator  
 ↓  
delay 10ms  
 ↓  
assert reset  
 ↓  
delay  
 ↓  
deassert reset
```
如果順序錯：

→ device 永遠不起來。
