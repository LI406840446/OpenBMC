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
