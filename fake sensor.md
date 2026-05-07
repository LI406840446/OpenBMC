## 構建虛擬AST2600管理晶片，在裡面安裝openbmc後透過QEMU模擬開機  

```diff
---前置準備---
如果沒有Linux主機的話，用Windows要先下載WSL模擬Linux系統
照這個流程安裝ubuntu或是你想要的OS-> https://ivonblog.com/posts/setup-wsl2-on-windows11/

#日後啟動WSL的方法：
wsl -d Ubuntu

#WSL關機:
wsl --shutdown
#如果沒反應，用此指令強制中止WSL服務  
taskkill /F /im wslservice.exe  
```
<img width="628" height="132" alt="image" src="https://github.com/user-attachments/assets/157d50ca-a419-4801-9508-7bd51a02871f" />

##  1.建立目錄
```
mkdir -p AST2600_qemu
```
##  2.安裝 Docker
```
git clone https://github.com/macchen-yu/ast2600.git 
sudo sh ./ast2600/docker_install.sh
```
##  3.clone OpenBMC 原始碼
```
git clone https://github.com/AspeedTech-BMC/openbmc.git
```
##  4.進入專案目錄並設定環境
```
cd asbmc_v9.5/openbmc/
. setup ast2600-default as26_build
```
##  5.編譯 OpenBMC 映像檔（需要較長時間）
```
bitbake obmc-phosphor-image

**如果記憶體不足，可以嘗試調整 <build 目錄>/conf/local.conf 中的 BB_NUMBER_THREADS 參數，以控制構建鏡像時的記憶體使用量。
nano conf/local.conf
打開後，使用鍵盤方向鍵捲到檔案的最底部，加上這兩行（假設我們限制它同時最多只能跑 4 個任務）：

BB_NUMBER_THREADS = "4"
PARALLEL_MAKE = "-j 4"
加好之後，按 Ctrl + O 然後按 Enter 存檔，接著按 Ctrl + X 離開。
再次執行編譯
```
##  6.安裝 QEMU
```
sudo apt install qemu-system qemu-kvm libvirt-clients libvirt-daemon-system bridge-utils virt-manager -y
```
##  7.進入編譯目錄
```
cd ~/asbmc_v9.5/openbmc/as26_build
```
##  8.查詢可用機器
```
qemu-system-arm -machine help
```
##  9.啟動 QEMU 模擬 AST2600
```
qemu-system-arm -M ast2600-evb -m 1024 -serial stdio -display none -drive
file="$(pwd)/tmp/deploy/images/ast2600-default/obmc-phosphor-image-ast2600-default-20260507030550.static.mtd",
format=raw,if=mtd -net nic -net user,hostfwd=tcp::2222-:22,hostfwd=tcp::8443-:443

**注意: obmc-phosphor-image-ast2600-default-<時間戳>.static.mtd 的名稱每次都不同，需依照實際生成的檔案名稱修改。
```
##  10.登入 OpenBMC
```
Web UI：https://127.0.0.1:8443/
用戶名: root
密碼: 0penBmc
```
<img width="696" height="301" alt="image" src="https://github.com/user-attachments/assets/a05fc40e-7c82-48e1-a7b9-3d7ac3fa7bd3" />

##  ---怎麼載code,修改,編譯,build image---
```diff
教學:Development Environment Setup | openbmc/docs | DeepWiki

web是預設的code
cd ~/my_openbmc_project/ast2600/openbmc/as26_build/workspace/sources/webui-vue
code . （如果用 VS Code）

載code
cd ricky_li2@tpea90144706:~/my_openbmc_project/ast2600/openbmc/as26_build/workspace/sources$
devtool modify phosphor-ipmi-host

改code
devtool deploy-target webui-vue

加一個新的fake sensor data
!1.main.cpp修改
workspace/recipes-phosphor/sensors/fake-power-sensor/files/src/main.cpp
!2.建立menson.build
workspace/recipes-phosphor/sensors/fake-power-sensor/files/meson.build
!3.建立fake-power-sensor.bb
workspace/recipes-phosphor/sensors/fake-power-sensor/fake-power-sensor.bb
!4.建立
workspace/recipes-phosphor/sensors/fake-power-sensor/files/xyz.openbmc_project.FakePowerSensor.service
!5.修改layer.conf
workspace layer 新增fake-power-sensor 的 recipe + C++ DBus service + systemd unit
!6.build套件
bitbake fake-power-sensor
*要在/home/ricky_li2/my_openbmc_project/ast2600/openbmc/as26_build/下，因為conf/在這
!7.重 build 整個 image（你要更新到 QEMU/實機一定要這個）
bitbake obmc-phosphor-image
!8.看產出的 image 檔案（給 QEMU 用）
ls -l tmp/deploy/images/ast2600-default/ | grep obmc-phosphor-image | head
!9.下指令查看
-service
systemctl status xyz.openbmc_project.FakePowerSensor.service
-dbus
busctl introspect xyz.openbmc_project.FakePowerSensor /xyz/openbmc_project/sensors/power/FakePower0
busctl get-property xyz.openbmc_project.FakePowerSensor /xyz/openbmc_project/sensors/power/FakePower0 xyz.openbmc_project.Sensor.Value Value
-redfish
[看 Chassis] 開瀏覽器或用 host 的 curl：
https://127.0.0.1:8443/redfish/v1/Chassis
[看 Sensors collection]
https://127.0.0.1:8443/redfish/v1/Chassis/AST2600_EVB/Sensors
[展開一次看完整，方便 Ctrl+F 搜 FakePower]
https://127.0.0.1:8443/redfish/v1/Chassis/AST2600_EVB/Sensors?$expand=.

```
<img width="778" height="731" alt="image" src="https://github.com/user-attachments/assets/c3a4e07b-20fb-4894-8886-cbbe049c8176" />
<img width="1046" height="672" alt="image" src="https://github.com/user-attachments/assets/e146778e-3943-42ae-8e5c-621e0d0a793f" />







































