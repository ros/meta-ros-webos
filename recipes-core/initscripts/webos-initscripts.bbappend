# Copyright (c) 2019-2021 LG Electronics, Inc.

# Allow per-device configuration from a USB flash drive. The webos-device-config service is run once when a USB flash drive is
# mounted upon WEBOS_DEVICE_CONFIG_MOUNTPOINT. It runs webos-device-config-invoke.sh, which, if it finds a
# WEBOS_DEVICE_CONFIG_MOUNTPOINT/DISTRO-device-config/WEBOS_DEVICE_CONFIG_VERSION/rc.local, runs it and then copies it to
# WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL, where upon subsequent boots, it will be run by /etc/rc.local . These settings can be
# overridden in conf/local.conf . Set WEBOS_DEVICE_CONFIG_MOUNTPOINT_pn-webos-initscripts to "" to disable the feature; set
# WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL_pn-webos-initscripts to "" to prevent the copying.
# XXX Eventually move into a .bbappend!!
WEBOS_DEVICE_CONFIG_MOUNTPOINT = "/tmp/usb/sda/sda1"
WEBOS_DEVICE_CONFIG_VERSION = "v1"
WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL = "${webos_localstatedir}/webos-device-config/rc.local"

FILESEXTRAPATHS_prepend := "${THISDIR}/${BPN}:"
SRC_URI += "\
    file://webos-device-config-invoke.sh.in \
    file://webos-device-config.service \
"
do_configure_prepend() {
    if [ -n "${WEBOS_DEVICE_CONFIG_MOUNTPOINT}" ]; then
         # NB. The CMake variable WEBOS_TARGET_DISTRO is set to DISTRO by webos_cmake.bbclass .
        local distro_dir=${DISTRO}
        local systemd_system_scripts_dir=${base_libdir}/systemd/system/scripts

        if [ ! -d ${S}/files/systemd/services/$distro_dir ]; then
            bberror "do_configure_prepend in ${FILE} doesn't know where to put webos-device-config.service"
        fi

        local not_firstboot_sentinel_dir=$(dirname ${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL})
        cp ${WORKDIR}/webos-device-config.service ${S}/files/systemd/services/$distro_dir/webos-device-config.service
        cp ${WORKDIR}/webos-device-config-invoke.sh.in ${S}/files/systemd/scripts/$distro_dir/webos-device-config-invoke.sh.in

        # XXX Second -e only needed for older webos-initscripts layout.
        sed -i -e "/webos_configure_source_files(systemd_distro_in_scripts\$/ s@\$@ files/systemd/scripts/$distro_dir/webos-device-config-invoke.sh@" \
               -e "/set(systemd_units\$/ s@\$@ files/systemd/services/$distro_dir/webos-device-config.service@" \
            ${S}/CMakeLists.txt
    fi  # [ -n "${WEBOS_DEVICE_CONFIG_MOUNTPOINT}" ]


    # Remove AMENT_PREFIX_PATH from the default environment of webOS OSE. We add it and the others to the environment via
    # ros_setup.sh .
    sed -i -e '/ROS2 settings/ d' -e '/AMENT_PREFIX_PATH/ d' ${S}/files/systemd/environments/30-webos-global.conf.in
}

do_install_append() {
    if [ -n "${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL}" ]; then
        if [ ! -e ${D}${sysconfdir}/rc.local ]; then

            mkdir -p ${D}${sysconfdir}
            cat <<\! > ${D}${sysconfdir}/rc.local
#! /bin/sh
# Copyright (c) 2019-2021 LG Electronics, Inc.
!
        fi
        cat <<\! >> ${D}${sysconfdir}/rc.local

# If webos-device-config.service is running, then it already ran ${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL} before it was copied.
# ASSERT("$1" == "start")
if [ -x ${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL} ] && ! systemctl -q is-active webos-device-config.service; then
    ${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL} start
else
    true
fi
!
        chmod a+x ${D}${sysconfdir}/rc.local

        mkdir -p ${D}${sysconfdir}/systemd/system/rc-local.service.d
        cat <<! > ${D}${sysconfdir}/systemd/system/rc-local.service.d/30-after-webos-ibd.conf
# Copyright (c) 2019-2021 LG Electronics, Inc.

[Unit]
# /etc/rc.local is meant to be run after networking is up; under webOS, this isn't the case until "webos-ibd.target" is
# reached.
Requires=webos-ibd.target
After=webos-ibd.target
!

        # Equivalent to adding WantedBy=$usb_mount.mount to [Install] section of webos-device-config.service
        local usb_mount_wants=$(echo ${WEBOS_DEVICE_CONFIG_MOUNTPOINT} | sed -e 's@/@-@g' -e 's@^-@@' -e 's/$/.mount.wants/')
        mkdir -p ${D}${sysconfdir}/systemd/system/$usb_mount_wants
        ln -snf ${systemd_system_unitdir}/webos-device-config.service ${D}${sysconfdir}/systemd/system/$usb_mount_wants/webos-device-config.service
    else
        true
    fi  # [ -n "${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL}" ]
}
