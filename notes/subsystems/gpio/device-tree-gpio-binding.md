
# Device Tree GPIO Binding 深入解析

----------

# 1️⃣ 為什麼 GPIO 一定跟 Device Tree 綁在一起？

在現代 ARM SoC（RK3588 / RZ/V2H / i.MX）上：

> **GPIO 幾乎都透過 Device Tree 描述**

原因：

-   GPIO controller 由 DT 宣告
    
-   每個 peripheral 透過 phandle 取得 GPIO
    
-   flags 定義 active-level / open-drain / bias
    

沒有正確 DT：

👉 driver 根本拿不到 GPIO descriptor

----------

# 2️⃣ GPIO Controller 在 DT 中的樣子

範例（Rockchip 類型）：
```
gpio0: gpio@fec20000 {  
 compatible = "rockchip,gpio-bank";  
 reg = <0x0 0xfec20000 0x0 0x100>;  
 gpio-controller;  
 #gpio-cells = <2>;  
 interrupt-controller;  
 #interrupt-cells = <2>;  
};
```
關鍵屬性：


| 屬性 | 意義說明 |  
|------------------------|-----------------------------------------------|  
| gpio-controller | 宣告此節點為 GPIO controller |  
| #gpio-cells | 定義 GPIO phandle 所需參數數量 |  
| interrupt-controller | 宣告此 GPIO controller 可提供 IRQ 功能 |

----------

# 3️⃣ GPIO Binding 基本格式

標準格式：
```
gpios = <&gpioX line flags>;
```
例如：
```
reset-gpios = <&gpio3 5 GPIO_ACTIVE_LOW>;
```
含義：


| 參數 | 說明 |  
|------------------|------------------------------------|  
| &gpio3 | GPIO controller 的 phandle |  
| 5 | GPIO line offset（在該 controller 中的編號） |  
| GPIO_ACTIVE_LOW | GPIO active flag（低電位為 active） |

----------

# 4️⃣ flags 解釋
DT 中第三個參數叫做 **flags**：

reset-gpios = <&gpio3 5 GPIO_ACTIVE_LOW>;

實際上 flags 是 bitmask，定義在：

include/dt-bindings/gpio/gpio.h

----------

## 4.1 常見 Flags
```
#define GPIO_ACTIVE_HIGH      0  
#define GPIO_ACTIVE_LOW       1  
#define GPIO_OPEN_DRAIN       2  
#define GPIO_OPEN_SOURCE      4  
#define GPIO_PULL_UP          8  
#define GPIO_PULL_DOWN        16
```
⚠ 注意：不同 kernel 版本 bit 定義可能不同，但概念相同。

----------

## 4.2 GPIO_ACTIVE_LOW 是什麼？

這個 flag **只影響邏輯語意，不改變硬體模式**。

意思是：
```
logical 1 → physical 0  
logical 0 → physical 1
```
Kernel 內部會在：
```
gpiod_set_value()
```
做邏輯反轉。

----------

### 🔎 常見錯誤

很多人以為：
```
GPIO_ACTIVE_LOW = open drain
```
錯。

Active-low 只是邏輯反轉，不會改變：

-   驅動能力
    
-   電氣模式
    
-   是否為開漏
    

----------

## 4.3 GPIO_OPEN_DRAIN 是什麼？

open drain 表示：

只能拉低  
高電平由外部 pull-up 提供

硬體模式：
```
output-low  → drive low  
output-high → high-Z
```
這通常用於：

-   I2C
    
-   reset
    
-   power-good
    
-   shared line
    

----------

## 4.4 flags vs pinctrl 的差異

很多 SoC：

-   open drain 必須由 pinctrl 設定
    
-   pull-up/down 由 pinctrl 設定
    

DT flags 只是「宣告」，  
真正是否生效要看：

gpio controller driver 是否支援
----------

# 5️⃣ #gpio-cells 是什麼？


## 5.1 基本概念

在 controller 中定義：
```
#gpio-cells = <2>;
```
表示：

> phandle 後面要接 2 個參數

格式為：
```
<&controller param1 param2>
```
----------

## 5.2 常見情況

### Case A：2 cells
```
#gpio-cells = <2>;
```
代表：
```
<&gpioX offset flags>
```
這是最常見格式。

----------

### Case B：3 cells

某些 SoC 會：
```
#gpio-cells = <3>;
```
格式變成：
```
<&controller bank offset flags>
```
例如：
```
<&gpio 2 5 GPIO_ACTIVE_LOW>
```
----------

## 5.3 為什麼不能假設固定格式？

因為：

-   不同 SoC 設計不同
    
-   有些 controller 有多 bank
    
-   有些 controller 需要特殊參數
    

Kernel 解析時：
```
of_parse_phandle_with_args()
```
會依照：
```
#gpio-cells
```
解析數量。

----------

## 5.4 如果 #gpio-cells 錯了會發生什麼？

-   driver probe 失敗
    
-   gpiod_get() 失敗
    
-   無法解析 phandle
    
-   甚至 silent error

----------

# 6️⃣ GPIO Hog 機制

DT 可直接 claim GPIO：
```
enable-hog {  
 gpio-hog;  
 gpios = <5 GPIO_ACTIVE_HIGH>;  
 output-high;  
 line-name = "panel-enable";  
};
```
效果：

-   開機自動設定
    
-   user space 無法 request
    
-   driver 也無法取得
    

常用於：

-   電源 rail
    
-   背光 enable
    
-   reset default 狀態
    

----------

# 7️⃣ reset-gpios / enable-gpios 命名規則

標準 naming：
```
xxx-gpios
```
driver 會：
```
devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
```
自動對應：
```
reset-gpios
```
👉 這是 descriptor model 的關鍵。

----------

# 8️⃣ interrupt-gpios 與 interrupt-controller


## 8.1 GPIO 同時是 interrupt source

當某 pin：

-   既是 GPIO
    
-   又可觸發 IRQ
    

DT 需要描述：

1️⃣ 該 controller 是 interrupt-controller  
2️⃣ 該裝置的 interrupt 來源

----------

## 8.2 Controller 宣告
```
gpio3: gpio@xxxx {  
 gpio-controller;  
 interrupt-controller;  
 #interrupt-cells = <2>;  
};
```
代表：

> 此 GPIO controller 同時是 IRQ controller

----------

## 8.3 裝置使用 interrupt-parent

標準寫法：
```
interrupt-parent = <&gpio3>;  
interrupts = <5 IRQ_TYPE_LEVEL_LOW>;
```
格式由：
```
#interrupt-cells
```
決定。

通常是：
```
<offset trigger-type>
```
----------

## 8.4 interrupt-gpios 是什麼？

某些 binding 允許簡寫：
```
interrupt-gpios = <&gpio3 5 GPIO_ACTIVE_LOW>;
```
這種寫法：

-   driver 內部會轉成 gpiod + IRQ
    
-   不是通用標準
    
-   依 binding 定義而定
    

⚠ 不等於 interrupt-parent

----------

## 8.5 完整 IRQ flow
```
Hardware edge  
 ↓  
GPIO controller irq handler  
 ↓  
irq_domain_translate()  
 ↓  
request_irq()  
 ↓  
driver ISR
```
如果：

-   沒設 interrupt-controller
    
-   #interrupt-cells 錯誤
    
-   IRQ_TYPE 錯誤
    

👉 gpiomon 永遠不會觸發
    

----------

# 9️⃣ 與 pinctrl 的關係


GPIO = 控制電平  
pinctrl = 控制 pin 功能 + 電氣特性

----------

## 9.1 Pin mux 問題

如果 pin 沒被 mux 成 gpio：

-   設定方向無效
    
-   set_value 無效
    
-   gpiod 正常但硬體不動
    

例如：

該 pin 仍在 I2C mode

----------

## 9.2 pinctrl 設定範例
```
panel_pins: panel-pins {  
 pins = "GPIO3_B5";  
 function = "gpio";  
 bias-pull-up;  
};
```
裝置：
```
pinctrl-names = "default";  
pinctrl-0 = <&panel_pins>;
```
----------

## 9.3 為什麼 GPIO flags 不等於 pinctrl？

flags 是「邏輯層」  
pinctrl 是「電氣層」

例如：


| 功能 | flags 支援 | pinctrl 支援 |  
|-----------------|------------|--------------|  
| active-low | ✅ | ❌ |  
| open-drain | 部分支援 | 多數由 pinctrl 設定 |  
| pull-up | ❌ | ✅ |  
| drive strength | ❌ | ✅ |

----------

## 9.4 錯誤案例

### 情境：

reset 拉不起來

實際原因：

-   pinctrl 沒 mux
    
-   pin 被 bridge driver 佔用
    
-   bias 設錯
    
-   drive strength 太低
    

----------

## 9.5 Debug pinctrl

查看：
```
/sys/kernel/debug/pinctrl/
```
可看：

-   mux state
    
-   owner
    
-   function

----------

# 🔟 Open Drain + Pull-up

正確寫法：
```
reset-gpios = <&gpio3 5 GPIO_ACTIVE_LOW>;
```
pinctrl：
```
bias-pull-up;  
drive-open-drain;
```
或：
```
GPIO_OPEN_DRAIN
```
⚠ 很多人誤以為：
```
GPIO_ACTIVE_LOW = open drain
```
完全錯誤。

----------

# 1️⃣1️⃣ 多 GPIO 定義

支援 multi-line：
```
data-gpios = <&gpio1 3 GPIO_ACTIVE_HIGH>,  
 <&gpio1 4 GPIO_ACTIVE_HIGH>;
```
driver 可：
```
devm_gpiod_get_array();
```
----------



# 1️⃣2️⃣Debug 指令

Dump DT：
```
dtc -I fs /sys/firmware/devicetree/base
```
或：
```
cat /proc/device-tree/xxx
```
查 GPIO：
```
cat /sys/kernel/debug/gpio
```
----------

# 1️⃣3️⃣ 常見錯誤觀念


| 錯誤觀念 | 正確理解 |  
|------------------------------------|-----------------------------------------------|  
| GPIO_ACTIVE_LOW 會自動拉低 | 只是邏輯反轉，不會改變實際電位 |  
| GPIO hog 只是預設值 | hog 會在 boot 時 claim 並鎖住 GPIO |  
| 不需要 pinctrl | GPIO 使用前必須先正確 pinmux 到 GPIO 模式 |
