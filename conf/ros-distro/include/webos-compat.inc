# Copyright (c) 2019 LG Electronics, Inc.
#
# Place the fixups necessary to make meta-webosose compatible with meta-ros layer version 3 here.

# Give ROS billing in DISTRO_NAME.
ROS_OE_DISTRO_NAME_PREPEND ?= "${ROS_OE_DISTRO_NAME} with "
DISTRO_NAME_prepend = "${ROS_OE_DISTRO_NAME_PREPEND}"

# Append the webOS OSE build number to WEBOS_DISTRO_VERSION.
ROS_WEBOS_DISTRO_VERSION_APPEND ??= ""
WEBOS_DISTRO_VERSION_TYPE_append = "${ROS_WEBOS_DISTRO_VERSION_APPEND}"

# Append the ROS metadata version to DISTRO_VERSION.
ROS_OE_DISTRO_VERSION_APPEND ?= "${@'+' + d.getVar('ROS_DISTRO_METADATA_VERSION', True)}"
DISTRO_VERSION_append = "${ROS_OE_DISTRO_VERSION_APPEND}"


# These will be "<unknown>" if TOPDIR isn't a Git repo => drop them from the display of the build configuration if this is the
# case.
BUILDCFG_VARS_remove = "${@oe.utils.conditional('WEBOS_DISTRO_TOPDIR_REVISION', '<unknown>', 'WEBOS_DISTRO_TOPDIR_REVISION', '', d)}"
BUILDCFG_VARS_remove = "${@oe.utils.conditional('WEBOS_DISTRO_TOPDIR_DESCRIBE', '<unknown>', 'WEBOS_DISTRO_TOPDIR_DESCRIBE', '', d)}"

# XXX Perhaps move this into a .bbappend that is only parsed with meta-networking (the layer with the recipe for ntp) is present.
do_install_prepend_pn-ntp() {
    # As recommeneded by https://developers.google.com/time/faq#how_do_i_use_google_public_ntp and
    # https://developers.google.com/time/guides
    sed -i -e  '/time.server.example.com/ s/^.*$/server time.google.com iburst prefer/' ${WORKDIR}/ntp.conf
}


# Allow per-device configuration from a USB flash drive. The webos-device-config service is run once when a USB flash drive is
# mounted upon WEBOS_DEVICE_CONFIG_MOUNTPOINT. It runs webos-device-config-invoke.sh, which, if it finds a
# WEBOS_DEVICE_CONFIG_MOUNTPOINT/DISTRO-device-config/WEBOS_DEVICE_CONFIG_VERSION/rc.local, runs it and then copies it to
# WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL, where upon subsequent boots, it will be run by /etc/rc.local . These settings can be
# overridden in conf/local.conf . Set WEBOS_DEVICE_CONFIG_MOUNTPOINT_pn-webos-initscripts to "" to disable the feature; set
# WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL_pn-webos-initscripts to "" to prevent the copying.
# XXX Eventually move into a .bbappend!!
WEBOS_DEVICE_CONFIG_MOUNTPOINT_pn-webos-initscripts = "/tmp/usb/sda/sda1"
WEBOS_DEVICE_CONFIG_VERSION_pn-webos-initscripts = "v1"
WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL_pn-webos-initscripts = "${webos_localstatedir}/webos-device-config/rc.local"

do_configure_prepend_pn-webos-initscripts() {
    if [ -n "${WEBOS_DEVICE_CONFIG_MOUNTPOINT}" ]; then
         # NB. The CMake variable WEBOS_TARGET_DISTRO is set to DISTRO by webos_cmake.bbclass .
        local distro_dir=${DISTRO}
        local systemd_system_scripts_dir=${base_libdir}/systemd/system/scripts

        if [ ! -d ${S}/files/systemd/services/$distro_dir ]; then
            bberror "do_configure_prepend_pn-webos-initscripts in ${FILE} doesn't know where to put webos-device-config.service"
        fi

        local not_firstboot_sentinel_dir=$(dirname ${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL})
        cat <<! > ${S}/files/systemd/services/$distro_dir/webos-device-config.service
# Copyright (c) 2019 LG Electronics, Inc.
#
# Invoke webos-device-config-invoke.sh, which handles the webOS per-device configuration rc.local on the USB flash drive
# automounted onto ${WEBOS_DEVICE_CONFIG_MOUNTPOINT} .

[Unit]
Description=webos - "%n"
# Launch after webos-ibd.target so that networking is up.
Requires=webos-ibd.target
After=webos-ibd.target
# XXX ConditionFirstBoot=true <- doesn't work!
# Temporary scheme until ConditionFirstBoot works. Doing it this way is marginally less secure (but only marginally, since local
# access is required to remove the sentinel file).
ConditionPathIsDirectory=!$not_firstboot_sentinel_dir

[Service]
Type=oneshot
ExecStart=$systemd_system_scripts_dir/webos-device-config-invoke.sh ${WEBOS_DEVICE_CONFIG_MOUNTPOINT}/${DISTRO}-device-config ${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL}
# This has the fortunate side-effects of preventing a re-insertion from invoking it again and allowing rc-local.service to detect
# that is has been run:
RemainAfterExit=yes

# NB. .wants symlink "<sysconfdir>/systemd/system/<usb_mount_wants>/webos-device-config.service" installed by ${PN}.
!

        cat <<\! > ${S}/files/systemd/scripts/$distro_dir/webos-device-config-invoke.sh.in
#! /bin/sh
# Copyright (c) 2019 LG Electronics, Inc.
#
# webos-device-config-invoke.sh WEBOS-DEVICE-CONFIG-DIR-ON-USB [COPY-TO-PATH]

# Sanity check.
[ $# -eq 0 ] && exit 1

if [ -d $1 ]; then
    if [ -d $1/v1 ]; then
        rc_local=$1/v1/rc.local
        # This rc.local expects an argument just like /etc/rc.local does, but also recognizes "firstboot" as well as "start". If
        # it returns failure, don't copy it but return success to the unit file so that the sentinel is created.
        $rc_local firstboot || exit 0
        if [ $# -ge 2 ]; then
            copy_to_path=$2
            mkdir -p $(dirname $copy_to_path)
            cp -fp $rc_local $copy_to_path
            timestamp=$(date -u +%Y%m%d%H%M%S)
            echo >> $copy_to_path \
                "# Copied from $rc_local on $timestamp UTC (might not be accurate if the sync to network time has yet to occur)."
            unset timestamp
            unset copy_to_path
        fi
        unset rc_local

        exit 0
    # elif [ -d $1/v2 ]; then ...
    fi
fi

# Return failure if a script wasn't executed because, say, the USB flash drive that's present isn't a device config one.
exit 1
!
        # XXX Second -e only needed for older webos-initscripts layout.
        sed -i -e "/webos_configure_source_files(systemd_distro_in_scripts\$/ s@\$@ files/systemd/scripts/$distro_dir/webos-device-config-invoke.sh@" \
               -e "/set(systemd_units\$/ s@\$@ files/systemd/services/$distro_dir/webos-device-config.service@" \
            ${S}/CMakeLists.txt
    fi  # [ -n "${WEBOS_DEVICE_CONFIG_MOUNTPOINT}" ]


    # Remove AMENT_PREFIX_PATH from the default environment of webOS OSE. We add it and the others to the environment via
    # ros_setup.sh .
    sed -i -e '/ROS2 settings/ d' -e '/AMENT_PREFIX_PATH/ d' ${S}/files/systemd/environments/30-webos-global.conf.in
}
# ^^^^^ END do_configure_prepend_pn-webos-initscripts()

do_install_append_pn-webos-initscripts() {
    if [ -n "${WEBOS_DEVICE_CONFIG_COPIED_RC_LOCAL}" ]; then
        if [ ! -e ${D}${sysconfdir}/rc.local ]; then

            mkdir -p ${D}${sysconfdir}
            cat <<\! > ${D}${sysconfdir}/rc.local
#! /bin/sh
# Copyright (c) 2019 LG Electronics, Inc.
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
# Copyright (c) 2019 LG Electronics, Inc.

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
# ^^^^^ END do_install_append_pn-webos-initscripts()


# We need to revert changes from
#
# commit 49720e6d680d0041850c00ce6dc859d557825595
# Author: Alexander Kanavin <alex.kanavin@gmail.com>
# Date:   Thu Apr 11 18:00:37 2019 +0200
# Subject: python3: add a tr-tr locale for test_locale ptest
#
# because as described in:
# http://lists.openembedded.org/pipermail/openembedded-commits/2017-November/215520.html
# http://lists.openembedded.org/pipermail/openembedded-core/2018-October/156694.html
# it doesn't work when GLIBC_GENERATE_LOCALES are restricted like they are in webos builds:
# meta-webos/conf/distro/include/webos-toolchain.inc:GLIBC_GENERATE_LOCALES = "en_US.UTF-8"
RDEPENDS_python3-ptest_remove_libc-glibc = " \
    locale-base-tr-tr.iso-8859-9 \
"

# webos-rpi.inc _removes "kernel-modules" from MACHINE_EXTRA_RRECOMMENDS, and _remove happens last, so we can't add it back to
# MACHINE_EXTRA_RRECOMMENDS.
IMAGE_INSTALL_append_rpi = " kernel-modules"

# otherwise e.g. vboxguest needed by qemux86 webos-images isn't enabled and do_rootfs fails
PREFERRED_VERSION_linux-yocto = "4.18%"

# Move from base_prefix to prefix for ostree to update it.
ros_prefix = "${prefix}/opt/ros/${ROS_DISTRO}"

# python*-pyqt5 is blacklisted, because webOS OSE is missing newer meta-qt5 from warrior with older
# meta-oe thud which has incompatible old sip:
# python-pyqt5 was skipped: Recipe is blacklisted: needs newer sip than 4.19.12 what we have in thud, This version of PyQt5 requires sip 4.19.14 or later. http://caprica.lgsvl.com:8080/Errors/Details/1509139
ROS_WORLD_SKIP_GROUPS += "pyqt5"

# mongodb is blacklisted in webOS OSE:
# mongodb was skipped: Recipe is blacklisted: src/mongo/util/net/ssl_manager.cpp:557:70: error: invalid conversion from 'size_t*' {aka 'unsigned int*'} to 'long unsigned int*' [-fpermissive] http://caprica.lgsvl.com:8080/Errors/Details/1509767
ROS_WORLD_SKIP_GROUPS += "mongodb"
