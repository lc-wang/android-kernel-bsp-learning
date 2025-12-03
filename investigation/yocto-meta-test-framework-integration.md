
# 📄 **Yocto devtool × bitbake-layers × test-framework 整合技術報告**


# 1. 背景說明

本份報告記錄如何使用：
-   **bitbake-layers**
-   **devtool**
-   **custom meta-layer**
-   **image recipe**

----------

# 2. Yocto Layer 架構整合流程總覽

整合 test-framework 的完整流程如下：

1.  **建立 meta-test-framework layer**（bitbake-layers）
2.  **使用 devtool add 從 GitHub 建立 recipe**
3.  **手動補上 do_install 與啟動器**
4.  **將 recipe 移至 meta-layer**
5.  **建立自訂 image 配方 core-image-testfw**
6.  **bitbake core-image-testfw**
7.  **target 運行 testfw 驗證**
    

整體架構：
```sh
meta-test-framework/
├── classes/
│   └── testfw-image.bbclass
├── recipes-test-framework/
│   └── test-framework/
│       ├── test-framework_git.bb
├── recipes-core/
│   └── images/
│       └── core-image-testfw.bb 
```
----------

# 3. bitbake-layers 建立 meta layer

建立獨立 layer：
```sh
cd build-imx95-smarc-wayland
bitbake-layers create-layer ../meta-test-framework 
```
加入 Yocto build：
```sh
bitbake-layers add-layer ../meta-test-framework
```
檢查是否成功加入：
```sh
bitbake-layers  show-layers
```
----------

# 4. devtool 使用流程與注意事項

## 4.1 devtool add 步驟
```sh
devtool add test-framework https://github.com/lc-wang/test-framework.git --version main
``` 

產生的內容會被放在 workspace：
```sh
workspace/
├── recipes/test-framework/test-framework_git.bb
├── sources/test-framework/
```
----------

## 4.2 devtool 常見問題：branch=main 仍找 master

### ✔ 症狀
```sh
Unable to resolve 'master' in upstream git repository` 
```
### ✔ 根本原因
```sh
Yocto 的 git fetcher 在部分版本會強制 fallback 至 master。
```
### ✔ 解法

👉 在 GitHub 建立一個 master branch。

----------

## 4.3 devtool workspace 殘留問題

若執行 devtool add 後想重建 recipe：

### 錯誤症狀：
```sh
recipe test-framework is already in your workspace
```
### 解法 1（推薦）：清空 workspace
```sh
rm -rf workspace 
```
### 解法 2：reset 單一 recipe
```sh
devtool reset test-framework
```
----------

## 4.4 devtool add 後 recipe 缺少 do_install

devtool 不會自動產生 do_install：
```sh
Package  'test-framework' has no installation candidate
```
需手動補上（見後續完整 recipe）。

----------

# 5. test-framework_git.bb — 最終可用版
```sh
meta-test-framework/recipes-test-framework/test-framework/test-framework_git.bb
```
```sh
DESCRIPTION = "Automated Test Framework for Linux BSP / PCBA / System Test"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835b7f5f8e2d301a25de4f6c51e4f7a"

SRC_URI = "git://github.com/lc-wang/test-framework.git;protocol=https;branch=main"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

RDEPENDS:${PN} += "coreutils jq dialog util-linux"

do_install() {
    install -d ${D}/opt/test-framework

    cp -r ${S}/configs     ${D}/opt/test-framework/
    cp -r ${S}/scripts     ${D}/opt/test-framework/
    cp -r ${S}/tests       ${D}/opt/test-framework/
    cp -r ${S}/resources   ${D}/opt/test-framework/ || true

    install -d ${D}${bindir}
    cat << 'EOF' > ${D}${bindir}/testfw
#!/bin/sh
cd /opt/test-framework/scripts/core
exec ./menu.sh "$@"
EOF

    chmod 0755 ${D}${bindir}/testfw
}

FILES:${PN} += "/opt/test-framework"
FILES:${PN} += "${bindir}/testfw"
```
----------

# 6. 自訂 image recipe：core-image-testfw
```sh
meta-test-framework/recipes-core/images/core-image-testfw.bb
```
```sh
DESCRIPTION = "Image with test-framework"
LICENSE = "MIT"

require recipes-graphics/images/core-image-weston.bb
inherit testfw-image

IMAGE_INSTALL:append = " test-framework"
```
----------

# 7. testfw 可執行啟動器

路徑：
```sh
/usr/bin/testfw
```
內容：
```sh
#!/bin/sh
cd /opt/test-framework/scripts/core
exec ./menu.sh "$@"
```
這修正了 test-framework 找不到 libs 的問題：
```sh
core/libs/logging_utils.sh:  No  such  file  or  directory
```
----------


# 8. 重要錯誤排查紀錄

| 問題 | 錯誤訊息 | 根因 | 解法 |
|------|-----------|--------|-------|
| Git branch 問題 | Unable to resolve 'master' | devtool fallback | GitHub 建立 master |
| devtool 重複 add | recipe already in workspace | workspace 未清 | `rm -rf workspace` / `devtool reset` |
| create-image 不存在 | invalid choice: create-image | BSP 過舊 | 手動建立 image |
| do_install 缺失 | No installation candidate | devtool 不產生 do_install | 手動撰寫 |
| libs not found | logging_utils.sh: No such file | launcher 沒切工作目錄 | `cd /opt/.../core` |
| QA fail | requires /bin/bash | 啟動器使用 bash | 改成 `/bin/sh` |

