
# DAI Link 與 Machine Driver 深度解析

> ASoC 聲卡如何被「組裝」出來

----------

# 1️⃣ 先建立整體視角

在 SoC 音訊世界中：
```
CPU I2S controller  (platform driver)
Codec chip          (I2C driver)
Board wiring        (machine driver)
```
ASoC 的任務就是：

> 把這三個 driver 組裝成一張 sound card

這個組裝機制的核心是：

`snd_soc_dai_link` 

----------

# 2️⃣ DAI 是什麼？

DAI = Digital Audio Interface

代表「數位音訊接口」。

例如：

-   I2S
    
-   TDM
    
-   PCM
    
-   PDM
    

----------

## DAI 在 Kernel 中
```
struct snd_soc_dai {
    const char *name;
    struct snd_soc_dai_ops *ops;
    ...
};
```
每個 driver 可以註冊一個或多個 DAI。

例如：
```
rockchip_i2s.c   → 註冊 CPU DAI
wm8960.c         → 註冊 Codec DAI
```
----------

# 3️⃣ snd_soc_dai_link

這是 ASoC 的「連線定義」。
```
struct snd_soc_dai_link {
    const char *name;
    const char *stream_name;

    const char *cpu_dai_name;
    const char *codec_dai_name;

    const char *platform_name;
    const char *codec_name;

    unsigned int dai_fmt;
};
```
----------

## 它做了什麼？

它描述：

`CPU DAI  <------>  Codec DAI` 

包含：

-   誰是 master
    
-   clock format
    
-   I2S / left-justified
    
-   bit clock polarity
    

----------

# 4️⃣ Machine Driver 是什麼？

Machine driver 是：

> 板級 glue layer

📁 常見位置：
```
sound/soc/rockchip/
sound/soc/fsl/
sound/soc/renesas/
```
----------

## Machine driver 負責：

-   定義 dai_link
    
-   註冊 snd_soc_card
    
-   設定 routing
    
-   設定 clock
    
-   定義 DAPM widgets
    

----------

# 5️⃣ 實際註冊流程

當系統 boot 時：
```
CPU DAI driver probe
Codec driver probe
Machine driver probe
```
最關鍵是 machine driver。

----------

## Machine driver 範例
```
static  struct  snd_soc_dai_link  my_dai_link = {
    .name = "I2S-Codec",
    .stream_name = "Playback",
    .cpu_dai_name = "rockchip-i2s",
    .codec_dai_name = "wm8960-hifi",
    .codec_name = "wm8960.1-001a",
    .platform_name = "rockchip-i2s",
    .dai_fmt = SND_SOC_DAIFMT_I2S |
               SND_SOC_DAIFMT_NB_NF |
               SND_SOC_DAIFMT_CBS_CFS,
};
```
----------

## 註冊 card
```
static  struct  snd_soc_card  my_card = {
    .name = "MySoundCard",
    .owner = THIS_MODULE,
    .dai_link = &my_dai_link,
    .num_links = 1,
};
```
最後：

`snd_soc_register_card(&my_card);` 

----------

# 6️⃣ Probe call flow

當 machine driver 呼叫：

`snd_soc_register_card()` 

內部會：
```
snd_soc_bind_card()
   ↓ soc_bind_dai_link()
   ↓
找到 CPU DAI
找到 Codec DAI
建立 snd_soc_pcm_runtime
```
----------

# 7️⃣ snd_soc_pcm_runtime 是什麼？
```
struct snd_soc_pcm_runtime {
    struct snd_soc_dai *cpu_dai;
    struct snd_soc_dai *codec_dai;
    struct snd_pcm *pcm;
};
```
這是 runtime 連線物件。

播放時：
```
aplay
  ↓
ALSA Core
  ↓
snd_soc_pcm_ops
  ↓
轉呼叫 cpu_dai->ops
  ↓
codec_dai->ops
```
----------

# 8️⃣ DTS 如何影響 Machine Driver

在 modern kernel，

很多 machine driver 不再硬寫 dai_link，

而是：

> 透過 device tree 描述

例如：
```
sound {
    compatible = "simple-audio-card";

    simple-audio-card,cpu {
        sound-dai = <&i2s0>;
    };

    simple-audio-card,codec {
        sound-dai = <&wm8960>;
    };
};
```
simple-audio-card driver 會：

-   parse DT
    
-   建立 dai_link
    
-   註冊 card
    

----------

# 9️⃣ ASoC 真正運作流程

播放時完整流程：
```
aplay
  ↓ snd_pcm_open()
  ↓ snd_soc_pcm_open()
  ↓
cpu_dai->ops->startup()
codec_dai->ops->startup()
  ↓ hw_params()
  ↓ trigger(START)
  ↓
CPU I2S controller 啟動 DMA
  ↓
codec 接收 bit clock
  ↓
DAC 輸出聲音
```
----------

# 🔟 BSP Debug 時你真正要檢查什麼？

如果沒有聲音：

你要檢查：
```
1. CPU DAI driver 有 probe 嗎？
2. Codec driver 有 probe 嗎？
3. Machine driver 有 bind 成功嗎？
4. snd_soc_card 有建立嗎？
5. dai_link 格式對嗎？
6. clock tree 正確嗎？
```
不是只看 PCM。

----------

# 1️⃣1️⃣ 常見錯誤案例

### ❌ cpu_dai_name 不匹配

dmesg：

`ASoC:  no  DAI  found` 

----------

### ❌ codec_name 錯

`ASoC: CODEC not registered` 

----------

### ❌ dai_fmt 不對

聲音是雜音或完全沒聲音。
