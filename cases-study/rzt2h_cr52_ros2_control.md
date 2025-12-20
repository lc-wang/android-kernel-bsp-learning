
# 📘 **RZ/T2H CR52 RemoteProc × ROS2 控制整合 — 技術文件**

本文記錄 RZT2H (Ubuntu 24.04) 上透過 **ROS2 ** 控制 CR52 remoteproc 的完整流程，並整理實作與 debug 過程，方便未來維護與其他人參考。

----------

# 目錄

1.  系統環境
2.  安裝 ROS2 Jazzy
3.  建立 ROS2 Workspace
4.  `rzt2h_remoteproc` 套件建立流程
5.  CR52 RemoteProc Service Node 程式
6.  Build 與執行  
7.  Host PC 使用 rqt 操作 CR52  
8.  多機 ROS2 通訊設定（Network Discovery）
9.  CR52 沒有啟動的原因分析（權限問題） 
10.  最終採用的解法（setuid root）
11.  建議的進階改善方向
    

----------

# 1. 系統環境

RZT2H SBC 上運行：
```yaml
Ubuntu 24.04 LTS (Noble) 
```
確認：
```yaml
cat /etc/os-release 
```
Host PC 也運行 ROS2，用 rqt 操作。

----------

# 2. 安裝 ROS2
參考官方文件即可
https://docs.ros.org/en/rolling/Installation/Ubuntu-Install-Debs.html
##  2.1安裝 colcon
### ✔ 安裝 colcon 基本環境：
```bash
sudo apt install -y python3-colcon-common-extensions
```
這會安裝：

-   `colcon`
-   `colcon build`
-   常用的 Python extensions
-   amment build 整套工具鏈
    
### ✔ 確認 colcon 是否正常：
```bash
colcon --version
```
正常輸出類似：
```bash
colcon version 0.16.x
```
如果這行出現，表示 ROS2 workspace 就可以開始編譯。

----------

# 3. 建立 ROS2 Workspace
```yaml
mkdir -p ~/ros2_ws/src cd ~/ros2_ws 
```
----------

# 4. `rzt2h_remoteproc` 套件建立流程

建立 package：
```yaml
cd ~/ros2_ws/src
ros2 pkg create rzt2h_remoteproc --build-type ament_python
```
建立 scripts 與 module：
```yaml
rzt2h_remoteproc/
  package.xml
  setup.py
  rzt2h_remoteproc/
      __init__.py
      cr52_remoteproc_service.py   ← CR52 控制程式
  scripts/
```
----------

## ✔ 修正後的 setup.py（包含 `glob` 與正確 entry point）
```python
from setuptools import setup
import os
from glob import glob

package_name = 'rzt2h_remoteproc'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/scripts', glob('scripts/*.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='user',
    maintainer_email='user@example.com',
    description='RZT2H CR52 remoteproc controller',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'cr52_remoteproc_service = rzt2h_remoteproc.cr52_remoteproc_service:main',
        ],
    },
)
```
----------

# 5. CR52 RemoteProc Service Node 程式

使用 ROS2 Service（std_srvs/Trigger）控制：
-   `/cr52/start`
-   `/cr52/stop`
----------

# 6. Build 與執行

編譯：
```bash
cd ~/ros2_ws
colcon build --symlink-install
source install/setup.bash
```
執行 Node：
```bash
source  /opt/ros/rolling/setup.bash
ros2 run rzt2h_remoteproc cr52_remoteproc_service
```
成功訊息：
```bash
[INFO]  [cr52_remoteproc]: CR52 RemoteProc ROS2 Service Node started.
```
----------

# 7. Host PC 使用 rqt 操作 CR52

在 Host PC：
```yaml
rqt
```
開啟：
```yaml
Plugins → Services → Service Caller 
```
可以看到：
-   `/cr52/start`
-   `/cr52/stop`
   
按下 Call 即可控制 CR52。

----------

# 8. 多機 ROS2 通訊設定（Network Discovery）

兩台機器需要相同：

### 1. ROS_DOMAIN_ID
```bash
export ROS_DOMAIN_ID=55
echo "export ROS_DOMAIN_ID=55" >> ~/.bashrc
```
### 2. 防火牆關閉

`sudo ufw disable` 

### 3. multicast 測試

Host：
```bash
ros2 multicast receive
```
RZT2H：
```bash
ros2 multicast send
```
能收到才算完全打通 discovery。

----------

# 9. CR52 沒有啟動的原因分析（權限問題）

原始觀察：

-   rqt 按下 `/cr52/start`
-   RZT2H 上 remoteproc 沒反應
-   `/sys/class/remoteproc/.../firmware` 沒有變更
    
檢查後發現：

`ubuntu user 沒有權限寫入 sysfs` 

原因：  
RemoteProc sysfs 需要 root 權限才能寫入。

ROS2 Node 以一般使用者執行 → echo 寫入失敗 → service 看起來「沒作用」。

----------

# 10. 最終採用的解法（setuid root）

你選擇了最簡單直接可用的方法：
```bash
sudo chmod +s ~/ros2_ws/install/rzt2h_remoteproc/lib/rzt2h_remoteproc/cr52_remoteproc_service
```
效果：

-   ROS2 執行該程式時 → 以 root 身份執行    
-   可以成功寫入：
    
```bash
/sys/class/remoteproc/remoteproc0/firmware
/sys/class/remoteproc/remoteproc0/state
```
驗證：
```bash
cat /sys/class/remoteproc/remoteproc0/firmware
cat /sys/class/remoteproc/remoteproc0/state
```
成功看到：
```bash
gcc_rzt2h_cr52_0_rpmsg_linux_baremetal_demo.elf
running
```
----------


### 11. 建議的進階改善方向

| 項目 | 說明 |
|------|------|
| 🟦 使用 udev rule | 讓 sysfs 變成 0666，node 不需 root |
| 🟩 增加 /cr52/status service | 讓 rqt 顯示 LifeCycle |
| 🟥 寫一個 rqt plugin | 做成 Start/Stop CR52 GUI 按鈕 |
| 🟨 加上 RPMsg → ROS2 bridge | 將 CR52 firmware 資料轉成 ROS topics |
