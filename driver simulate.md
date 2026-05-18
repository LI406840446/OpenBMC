##  在 OpenBMC，「driver」到底是什麼？它扮演哪一段？
```diff
可以把整條鏈拆成 4 層：

-A. 硬體/匯流排層 (I2C bus)
BMC SoC 裡的 I2C controller（這個本身就有 driver）
板子上掛的 sensor IC（溫度、電壓、電流、風扇、power monitor…）

-B. Kernel driver 層（你說的 driver，多半是這個）
例如：LM75 溫度、INA2xx 電流電壓、TMP75、NCT、PMBus 類
作用：
透過 I2C 去讀 register
在 Linux 內核把數值標準化地暴露出來
常見出口是 sysfs：/sys/bus/i2c/devices/... 或 /sys/class/hwmon/hwmonX/...

-C. Userspace sensor daemon / service 層（OpenBMC 的 sensors）
OpenBMC 並不是 kernel 一讀到就自動上 DBus
通常需要 userspace 去讀 sysfs/hwmon/iio，然後建立 DBus 物件
常見元件（依平台不同）：
phosphor-hwmon（讀 hwmon sysfs → DBus）
dbus-sensors（一套 sensor daemon，可透過設定檔描述要讀哪個 sysfs/哪個 IIO）
entity-manager（描述 FRU、連接、sensor mapping；有些平台用它產生 sensor config）

-D. DBus / Redfish 層
DBus：xyz.openbmc_project.Sensor.Value
Redfish：BMC Web/Redfish 通常再把 DBus 映射出去
```


```
