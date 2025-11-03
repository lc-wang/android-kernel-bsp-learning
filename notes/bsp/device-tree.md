
# Device Tree Overview 
這份筆記整理 Linux Device Tree（裝置樹）架構與使用方式。  
目標是理解 DTS 的語法結構、kernel 匹配機制、overlay 與 debug 方法。

--- 

## 1. Device Tree 是什麼？
- **Device Tree (DT)** 是一種描述硬體配置的資料結構。  
- 主要設計目標：讓 kernel 不需重新編譯即可支援不同硬體平台。  
- 由 **ARM / PowerPC** 平台發展，現已廣泛用於 SoC 系統。

| 檔案類型 | 說明 |
| --- | --- |
| `.dts` | Device Tree Source – 主要描述檔案 |
| `.dtsi` | Device Tree Include – 共用設定 (類似 C header) |
| `.dtb` | Device Tree Blob – 編譯後二進位，供 kernel 使用 |
| `.overlay` | Device Tree Overlay – 動態覆蓋、修改部分節點 |
---
 ## 2. DTS 與 Kernel 的關係`` 
## 2. DTS 與 Kernel 的關係

Board.dts  
  ↓ （DTC 編譯）  
Board.dtb  
  ↓  
U-Boot / Bootloader  
  ↓  
傳遞給 Kernel  


- Kernel 開機時由 bootloader 傳入 `.dtb`。  
- 驅動透過 `of_match_table` 依照 `compatible` 屬性進行匹配。  
- 不同平台可共用同一套驅動，只需不同 `.dts`。

---

## 3. DTS 基本語法

```dts
/ {
    compatible = "rockchip,rk3588", "arm64";
    model = "IEI Board B675";

    memory@0 {
        device_type = "memory";
        reg = <0x0  0x80000000  0x0  0x40000000>; // 1GB };

    soc {
        compatible = "simple-bus"; #address-cells = <2>; #size-cells = <2>;
        ranges;

        uart0: serial@ff180000 {
            compatible = "rockchip,rk3568-uart", "snps,dw-apb-uart";
            reg = <0x0  0xff180000  0x0  0x100>;
            interrupts = <GIC_SPI 80 IRQ_TYPE_LEVEL_HIGH>;
            status = "okay";
        };
    };
};
``` 

| 元素 | 說明 |
| --- | --- |
| `/` | 根節點 |
| `node@addr` | 節點名稱，`@addr` 對應 register base |
| `compatible` | 驅動匹配依據 |
| `reg` | 寄存器區間 (address + size) |
| `interrupts` | 中斷設定 |
| `status` | `"okay"` / `"disabled"` |
| `label:` | 可被 phandle 參考的標籤 |

----------

## 4. phandle 與引用

-   **phandle** 是 DT 裡的「指標」，用來參照其他節點。
    
```dts
led_controller: gpio@1000 {
    compatible = "mychip,gpio";
    reg = <0x1000 0x100>;
    #gpio-cells = <2>;
};

led@0 {
    compatible = "mychip,led";
    gpios = <&led_controller 5 GPIO_ACTIVE_HIGH>;
};
```

> `&led_controller` → 指向該節點 phandle，驅動解析時可直接存取。

----------

## 5. 驅動匹配機制

驅動與 Device Tree 的連結依靠 `compatible` 屬性。

### 驅動範例

```c
static  const  struct  of_device_id  myled_of_match[] = {
    { .compatible = "mychip,led" },
    {},
};
MODULE_DEVICE_TABLE(of, myled_of_match); static  struct  platform_driver  myled_driver = {
    .probe = myled_probe,
    .remove = myled_remove,
    .driver = {
        .name = "myled",
        .of_match_table = myled_of_match,
    },
};
module_platform_driver(myled_driver);
```

當 kernel 掃描到符合的節點，會自動呼叫對應的 `probe()`。

----------

## 6. include / overlay / alias

### (1) include

-   共用設定放在 `.dtsi`，供多個板子引用。
```dts    
#include "rk3588.dtsi"
#include "rk3588-b675.dtsi"
```    
    

### (2) overlay

-   用於動態修改部分節點（例如外接裝置）。
```dts
/dts-v1/;
    /plugin/;
    
    &uart0 {
        status = "okay";
    };
    
    &i2c2 {
        eeprom@50 {
            compatible = "atmel,24c02";
            reg = <0x50>;
        };
    };
```

### (3) alias

-   提供簡短名稱，常見於 `/aliases` 節點：
```dts
aliases {
    serial0 = &uart0;
    ethernet0 = &gmac1;
};
``` 
    

----------

## 7. 常見屬性 (Properties)


| 屬性 | 說明 |
| --- | --- |
| `compatible` | 裝置類型與驅動匹配依據 |
| `reg` | 寄存器區間 (addr + size) |
| `interrupts` | 中斷編號與型態 |
| `clocks` | 參照 clock controller |
| `resets` | 參照 reset controller |
| `pinctrl-names` / `pinctrl-0` | 管腳配置 |
| `power-domains` | 指定裝置所屬電源域 |
| `vcc-supply` | 參照電源管理節點 |
| `status` | `"okay"`, `"disabled"`, `"reserved"` |
----------


## 8. 驗證與調試

| 工具 / 指令 | 用途說明 |
| --- | --- |
| `dtc -I dts -O dtb -o xxx.dtb xxx.dts` | 編譯 DTS 為 DTB（Device Tree Blob）。 |
| `dtc -I dtb -O dts -o xxx.dts xxx.dtb` | 反編譯 DTB 回可讀的 DTS 格式。 |
| `/proc/device-tree/` | 觀察 Kernel 執行中實際載入的 Device Tree 節點。 |
| `cat /sys/firmware/devicetree/base/compatible` | 驗證當前系統的根節點 `compatible` 屬性。 |
| `fdtdump xxx.dtb` | 解析 DTB 結構並輸出詳細節點資訊。 |
| `of_unittest.c` | Kernel 內建的 Device Tree 單元測試程式，用於驗證核心解析邏輯。 |
| `ftrace` (`trace_event=of_*`) | 追蹤 Device Tree 解析與節點建立過程。 |
----------


## 9. 常見錯誤與排查

| 問題 | 可能原因 | 修正建議 |
| --- | --- | --- |
| 驅動 `probe()` 未觸發 | `compatible` 字串不匹配 | 確認驅動的 `of_match_table` 與 DTS 文字完全一致（大小寫須相同）。 |
| DTS 編譯失敗 | `#include` 檔路徑錯誤或語法錯誤 | 檢查 `arch/<arch>/boot/dts/Makefile` 及引用路徑。 |
| 無法找到 GPIO / Clock | `phandle` label 名稱錯誤 | 確保 label 與 `&` 引用名稱一致，例如 `&gpio0`。 |
| overlay 無效 | Kernel 未啟用 `CONFIG_OF_OVERLAY` | 啟用該選項並重新編譯 Kernel。 |
| 無法解析中斷 | `interrupts-extended` 或 parent 定義錯誤 | 檢查 GIC node、interrupt parent 是否設正確。 |
| 節點被忽略 | `status = "disabled"` | 將節點狀態改為 `"okay"` 重新編譯並載入。 |
| 反編譯後節點缺失 | DTB 被壓縮或簽章保護 | 檢查 boot 流程是否載入了正確的 DTB。 |
📘 **小技巧：**
```bash
#驗證 DTS 結構與 binding 格式
make dt_binding_check

#反編譯目前正在使用的 DTB
dtc -I dtb -O dts -o running.dts /sys/firmware/fdt
```
----------

## 10. 學習建議

1.  嘗試撰寫一個最小範例：LED + GPIO controller。
2.  修改 `status` 為 `"disabled"`，觀察驅動 probe 是否改變。  
3.  用 `dtc -I dtb -O dts` 反編譯現有 DTB 分析結構。
4.  熟悉 phandle 與 cross-reference 的使用。
5.  了解 binding 文件格式與 YAML 驗證 (`make dt_binding_check`)。

----------

📘 **延伸閱讀**

-   `Documentation/devicetree/usage-model.rst`
-   `Documentation/devicetree/bindings/`
-   Device Tree Specification 0.3
-   Kernel source: `drivers/of/`, `scripts/dtc/`
