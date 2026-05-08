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
教學:
https://hackmd.io/@a0979552111/rJ1ruAr-1e
https://deepwiki.com/openbmc/docs/2.1-development-environment-setup

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



##  詳解

###  Bitbake recipe (fake-power-sensor.bb)
```C
-告訴Yocto怎麼取檔、編譯、安裝、打包、啟動service
它的目的就是把你這個專案變成一個 Yocto package，並且讓它能跟著 image 一起被建置與安裝。

SUMMARY = "Fake power sensor DBus service"
DESCRIPTION = "Exports a fake power sensor on DBus under /xyz/openbmc_project/sensors/power/FakePower0"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit meson pkgconfig systemd
/*  meson：使用 meson 來 configure/build/install（對應 files/meson.build）
    pkgconfig：讓依賴的 pkg-config 查找更順（例如 sdbusplus）
    systemd：讓 recipe 可以很方便宣告/安裝/啟用 systemd unit*/

SRC_URI = "file://meson.build \
           file://src/main.cpp \
           file://xyz.openbmc_project.FakePowerSensor.service"

S = "${UNPACKDIR}"

DEPENDS += "sdbusplus boost" //編譯期依賴：需要 sdbusplus（DBus 封裝）和 boost（asio / system）

SYSTEMD_SERVICE:${PN} = "xyz.openbmc_project.FakePowerSensor.service" //這是 systemd class 的宣告：這個 package 帶了一個 unit

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/xyz.openbmc_project.FakePowerSensor.service \
        ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "${systemd_system_unitdir}/xyz.openbmc_project.FakePowerSensor.service"
/*  因為你後來把「安裝 service 檔案」放在 recipe 做（不再靠 meson）
    do_install 把 .service 放到 ${systemd_system_unitdir}
    FILES 確保打包時把 unit 檔一起包進去*/
```

###  Menson專案 (menson.build)
```C
-告訴編譯系統怎麼把main.cpp編成/usr/bin/fake-power-sensor
它的目的非常單純：把 src/main.cpp 編成一個可執行檔並安裝到 rootfs。
project('fake-power-sensor', 'cpp',
  version: '1.0',
  meson_version: '>=0.60.0',
  default_options: [
    'warning_level=2',
    'werror=false',
    'cpp_std=c++23', //改成 c++23 是為了跟你當下的 sdbusplus header 相容（用到較新的標準庫型別）
  ],
)

sdbusplus_dep = dependency('sdbusplus')
boost_system_dep = dependency('boost', modules: ['system'])

executable( //產出 binary 名稱：fake-power-sensor
  'fake-power-sensor',
  'src/main.cpp',
  dependencies: [sdbusplus_dep, boost_system_dep], //宣告外部依賴，meson 會從 sysroot 裡找對應的 library/include
  install: true, //代表會被安裝到 image（通常是 /usr/bin/fake-power-sensor）
)

```

###  Systemd unit
```
-告訴系統開機後要怎麼把程式跑起來、掛掉後要不要重啟
[Unit]
Description=Fake Power Sensor
After=dbus.service //等 DBus 起來再跑（因為程式會 request DBus name）

[Service]
Type=simple
ExecStart=/usr/bin/fake-power-sensor //啟動程式
Restart=always //如果程式掛掉，2 秒後自動重啟（很適合常駐 sensor daemon）
RestartSec=2

[Install]
WantedBy=multi-user.target //代表它可以被 enable 成為開機服務（開機進 multi-user.target 時會拉起來）
```

###  work space layer
```C
-讓bibake掃得道你現在放在workspace/recipes-*/... 的 .bb / .bbappend

# ### workspace layer auto-generated by devtool ###
BBPATH =. "${LAYERDIR}:"
BBFILES += "${LAYERDIR}/recipes/*/*.bb \      //告訴 bitbake 要掃哪些路徑的 .bb / .bbappend
            ${LAYERDIR}/recipes-*/*/*.bb \
            ${LAYERDIR}/recipes-*/*/*/*.bb \
            ${LAYERDIR}/appends/*.bbappend"
BBFILE_COLLECTIONS += "workspacelayer"
BBFILE_PATTERN_workspacelayer = "^${LAYERDIR}/"
BBFILE_PATTERN_IGNORE_EMPTY_workspacelayer = "1"
BBFILE_PRIORITY_workspacelayer = "99"  //workspace layer 通常 priority 很高，方便你「覆蓋/新增」而不需要去動原本 meta-layer
LAYERSERIES_COMPAT_workspacelayer = "walnascar"
```

###  main.cpp
```c
#include <boost/asio/io_context.hpp> //Boost.Asio 的事件迴圈核心物件 io_context。後面用 timer、DBus connection 都靠它跑 event loop。
#include <boost/asio/steady_timer.hpp> //提供 steady_timer，用來做「每隔一段時間執行一次」的非同步計時器。
#include <chrono> //提供時間型別，例如 std::chrono::seconds。
#include <cmath> //提供 std::sin()，你用它做正弦波浮動。
#include <cstdint> //提供固定寬度整數型別，例如 std::uint64_t。
#include <functional> //提供 std::function，你用它把「排程下一次 timer」的 lambda 存起來遞迴呼叫。
#include <memory> //提供 std::shared_ptr，用於 DBus connection 的生命週期管理。
#include <sdbusplus/asio/connection.hpp> //OpenBMC 常用的 sdbusplus + Asio 整合：DBus connection 會掛在 io_context 上跑。
#include <sdbusplus/asio/object_server.hpp> //OpenBMC 常用的 sdbusplus + Asio 整合：DBus connection 會掛在 io_context 上跑。io_context 內部維護一個 事件佇列（event queue），當有任務、計時器、或 I/O 完成事件加入時，loop 會逐一取出並執行 callback handler。
#include <string> //association tuple 用到。
#include <tuple> //association 的元素是三元組。
#include <vector> //association list 是一個 vector。

namespace //匿名 namespace：讓這些常數只在本檔案可見（類似 static 的效果）。
{
constexpr const char* kBusName = "xyz.openbmc_project.FakePowerSensor"; //這是程式想要在 system bus 上註冊的 well‑known name（service 名稱）。其它程式可透過這個名字呼叫你的服務或找到你的 object。
constexpr const char* kObjPath = "/xyz/openbmc_project/sensors/power/FakePower0"; //這是你要在 D‑Bus 上 export 的物件路徑（object path）。在這個 path 下會 expose 相關介面與 property（例如 Value）。
constexpr const char* kIntfValue = "xyz.openbmc_project.Sensor.Value"; //這個 D‑Bus interface 包含 sensor 的 Value property（bmcweb/Redfish 會讀此值來呈現 sensor reading）。
constexpr const char* kIntfAssoc = "xyz.openbmc_project.Association.Definitions"; //這個介面通常用來建立 sensor 與 inventory 元件（例如某塊板或 chassis）之間的關聯，讓 bmcweb 能把 sensor 出現在正確的 Redfish 路徑下。

constexpr const char* kInventoryAst2600Evb =
    "/xyz/openbmc_project/inventory/system/board/AST2600_EVB"; //這是一個 inventory 的 object path（代表主機板或某個 inventory node）。你把 sensor 的 association 指向這個 path，表示這個 sensor 屬於該板子。

constexpr double kBaseWatts = 120.0; //模擬功耗的中心值（120W）。
constexpr double kAmplitudeWatts = 30.0; //模擬功耗的振幅（±30W），所以值會落在 90~150W 之間擺動。
constexpr std::chrono::seconds kPeriod{1}; //Timer 週期 1 秒。這直接決定「每秒更新」。
} // namespace

int main()
{
    boost::asio::io_context io; //建立 event loop。後面的 timer callback、DBus I/O 都在這個 loop 中執行。

    auto conn = std::make_shared<sdbusplus::asio::connection>(io); //建立 DBus connection，並綁定到 io_context。
    conn->request_name(kBusName); //向 DBus 申請擁有 well-known name：xyz.openbmc_project.FakePowerSensor。成功後別人就能用這個 service name 找到你。

    sdbusplus::asio::object_server server(conn); //建立 object server：用來 export interfaces/properties。
    auto intf = server.add_interface(kObjPath, kIntfValue); //在 object path 上加上 xyz.openbmc_project.Sensor.Value 介面。
    auto assocIntf = server.add_interface(kObjPath, kIntfAssoc); //同一個 object path 再加上 xyz.openbmc_project.Association.Definitions 介面。

    double valueWatts = kBaseWatts;
    intf->register_property("Value", valueWatts,  //在 xyz.openbmc_project.Sensor.Value 上建立 DBus property：Value。readOnly 表示 DBus client 只能讀，不能寫。
                            sdbusplus::asio::PropertyPermission::readOnly);

    using AssociationList =
        std::vector<std::tuple<std::string, std::string, std::string>>; //Associations 的型別是「多筆三元組」。
    const AssociationList associations = { 
        {"chassis", "all_sensors", kInventoryAst2600Evb}}; //表示這個 sensor 與 inventory AST2600_EVB 有關聯。
    assocIntf->register_property("Associations", associations,
                                 sdbusplus::asio::PropertyPermission::readOnly);

    intf->initialize(); //這兩行是把 interface 真正「註冊到 DBus」讓外界看得到。若沒呼叫 initialize，介面不會完整上線。  
    assocIntf->initialize();

    boost::asio::steady_timer timer(io); //建立 timer，也綁定到同一個 event loop。
    std::uint64_t tick = 0; //計數器，用來生成不同時間點的相位（phase）。

    std::function<void()> schedule; //宣告一個函式物件，稍後會指向 lambda（匿名函式 / 輕量函式物件）。
    schedule = [&]() {
        timer.expires_after(kPeriod); //設定 1 秒後到期。
        timer.async_wait([&](const boost::system::error_code& ec) { //到期時執行 callback。
            if (ec) //如果 timer 被取消或出錯，就不繼續。
            {
                return;
            }

            const double phase = static_cast<double>(tick) / 10.0; //計算正弦波相位（單位是 radians）。因為每秒 tick +1，所以 phase 每秒增加 0.1 rad。
            valueWatts = kBaseWatts + (kAmplitudeWatts * std::sin(phase)); //產生上下浮動的功耗值。
            intf->set_property("Value", valueWatts); //把新的值寫回 DBus property Value。

            ++tick;  //下一秒 phase 會不同，值就會不同。
            schedule(); //再次排程下一次 timer → 形成無限循環的每秒更新。
        });
    };

    schedule(); //啟動第一次排程（不呼叫就不會開始更新）。
    io.run(); //開始跑 event loop；程式在這裡阻塞並持續處理 timer/DBus 事件。
    return 0; //正常結束（通常不會走到這裡，除非 io_context 停止）。
}

```





































