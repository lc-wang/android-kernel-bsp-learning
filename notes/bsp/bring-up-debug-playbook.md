# BSP Bring-up Debug（系統化除錯決策）

> 本章定位（Capstone）：
> 
> -   將前面 BSP 章節（clock / pinctrl / interface / firmware / power domain / bootloader）  
>     **收斂成可執行的 debug 決策流程**
>     
> -   不是新知識，而是「遇到問題時，該先查哪裡、不要先改什麼」
>     
> -   目標是縮短 bring-up 與鬼問題 debug 的時間，而不是列 checklist
>     

----------

## 1. 為什麼 BSP Debug 一定要有 Playbook

BSP 問題的困難點，不在於單一技術，而在於：
-   問題橫跨 bootloader / kernel / firmware / userspace  
-   現象往往延遲出現（runtime / suspend / resume）
-   很多錯誤「看起來像 driver bug」
    

沒有 Playbook，debug 常會變成：
-   直覺改 driver
-   嘗試性 patch
-   修到看似正常但其實沒找到根因

👉 **Playbook 的價值在於「決策順序」，而不是技巧本身。**

----------

## 2. BSP 問題的四大類型

在開始 debug 前，先把問題歸類：

### 2.1 完全不起來（Hard Failure）

-   裝置 probe 失敗
-   bus 上完全看不到裝置
    

通常優先懷疑：
-   clock / reset    
-   pinctrl
-   firmware blocking

----------

### 2.2 Probe 成功但不能用（Soft Failure）

-   probe OK 
-   runtime 存取失敗

優先懷疑：
-   pinmux / 電氣
-   firmware 版本或狀態
-   power domain    
----------

### 2.3 偶發性錯誤（Intermittent Failure）

-   有時好、有時壞
-   reboot 後消失

高度指向：
-   power domain    
-   clock 穩定度  
-   bootloader 殘留狀態

----------

### 2.4 Suspend / Resume 後壞掉

-   cold boot 正常
-   resume 後功能失效

優先懷疑：

-   pinctrl sleep state
-   firmware 未重新初始化
-   power domain restore 順序

----------

## 3. Debug 優先順序

當你不知道從哪裡開始時，**永遠照這個順序**：

1.  **Clock / Reset / Power**
2.  **Pin Control / GPIO**
3.  **Bus / Interface（I2C / I2S / SPI）**
4.  **Firmware Loading**
5.  **Driver 行為**
6.  **Userspace / Framework**

👉 越前面的層級沒確認，越不要往後改。

----------

## 4. 常見情境的快速 Debug 路徑

### 4.1 I2C Probe 成功但讀不到資料

建議順序：

1.  pinctrl 是否正確套用
2.  pull-up / 電氣設定
3.  power domain 是否真的 on
4.  firmware 是否在 blocking 狀態

❌ 不要第一時間改 I2C driver

----------

### 4.2 Audio 有聲音但怪怪的

建議順序：

1.  clock rate / PLL 
2.  master / slave 設定
3.  pinctrl sleep / default state
4.  suspend / resume 行為

❌ 不要先怪 codec driver

----------

### 4.3 Resume 後裝置消失

建議順序：

1.  power domain restore
2.  firmware reload
3.  pinctrl sleep → default
----------

### 4.4 Kernel 看起來隨機壞掉

建議順序：

1.  bootloader 是否殘留狀態
2.  DTS 是否同步
3.  reserved memory / firmware

----------

## 5. BSP Debug 最常犯的錯誤

1.  太早改 driver
2.  忽略 firmware
3.  沒看 pinctrl
4.  只看 clock 不看 power domain
5.  不測 suspend / resume
6.  DTS 與 bootloader 不同步
7.  用 reboot 掩蓋問題
8.  不固定測試條件
9.  把偶發當成 race
10.  修到「看起來好」就停

----------

## 6. Bring-up 最小驗證清單（Exit Criteria）

在說「這塊板子 OK」之前，至少確認：

-   cold boot 連續成功  
-   bus 裝置穩定可用
-   firmware 載入可控
-   suspend / resume 重複測試
-   clock / power domain 行為符合預期

👉 這不是測試項目，是 **信心來源**。

----------

## 7. 如何使用這份 Playbook

-   每次 debug，先選擇「問題類型」
-   再依優先順序排除
-   每一步都要能回答「為什麼不是這一層」

當你能做到這件事時：
-   debug 會變快 
-   修正會更少副作用
