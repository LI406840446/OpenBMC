SUMMARY = "Fake power sensor DBus service"
DESCRIPTION = "Exports a fake power sensor on DBus under /xyz/openbmc_project/sensors/power/FakePower0"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit meson pkgconfig systemd

SRC_URI = "file://meson.build \
           file://src/main.cpp \
           file://xyz.openbmc_project.FakePowerSensor.service"

S = "${UNPACKDIR}"

DEPENDS += "sdbusplus boost"

SYSTEMD_SERVICE:${PN} = "xyz.openbmc_project.FakePowerSensor.service"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/xyz.openbmc_project.FakePowerSensor.service \
        ${D}${systemd_system_unitdir}/
}

FILES:${PN} += "${systemd_system_unitdir}/xyz.openbmc_project.FakePowerSensor.service"
