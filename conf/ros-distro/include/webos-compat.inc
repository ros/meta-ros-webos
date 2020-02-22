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

# widgets PACKAGECONFIG is explicitly disabled in webOS OSE qtbase:
# meta-webos/recipes-qt/qt5/qtbase_git.bbappend:PACKAGECONFIG_remove = "widgets"
ROS_WORLD_SKIP_GROUPS += "qt5-widgets"

# webOS OSE already sets necessary license flags:
# meta-webos/conf/distro/webos.conf:LICENSE_FLAGS_WHITELIST_append = " commercial_gstreamer1.0-omx"
# and removes the restricted faad dependency:
# meta-webos-raspberrypi/recipes-multimedia/gstreamer/gstreamer1.0-plugins-bad_1.14.%.bbappend:PACKAGECONFIG_remove_rpi = "faad"
ROS_WORLD_SKIP_GROUPS_remove = "world-license"

PNBLACKLIST[nagios-core] ?= "${@bb.utils.contains('BBFILE_COLLECTIONS', 'webserver', '', 'Depends on apache2 from meta-webserver which is not included', d)}"
PNBLACKLIST[nagios-plugins] ?= "${@bb.utils.contains('BBFILE_COLLECTIONS', 'webserver', '', 'Depends on nagios-core which depends on apache2 from meta-webserver which is not included', d)}"
PNBLACKLIST[nagios-nrpe] ?= "${@bb.utils.contains('BBFILE_COLLECTIONS', 'webserver', '', 'Depends on nagios-core which depends on apache2 from meta-webserver which is not included', d)}"
PNBLACKLIST[nagios-nsca] ?= "${@bb.utils.contains('BBFILE_COLLECTIONS', 'webserver', '', 'Rdepends on nagios-base provided by nagios-core which depends on apache2 from meta-webserver which is not included', d)}"
PNBLACKLIST[ipxe] ?= "Doesn't match the COMPATIBLE_HOST restriction from syslinux: syslinux was skipped: incompatible with host arm-webos-linux-gnueabi (not in COMPATIBLE_HOST)"

# meta-openembedded/meta-multimedia/recipes-multimedia/gerbera/gerbera_git.bb:do_compile
PNBLACKLIST[gerbera] ?= "ERROR: gerbera-1.0+gitAUTOINC+4435b165d1-r0 do_compile: Function failed: do_compile: 8.2.0/ld: error: /jenkins/mjansa/build-ros/webos-dashing-thud/tmp-glibc/work/qemux86-webos-linux/gerbera/1.0+gitAUTOINC+4435b165d1-r0/recipe-sysroot-native/usr/lib/libnsl.so: incompatible target"
# meta-openembedded/meta-networking/recipes-support/ntop/ntop_5.0.1.bb:do_compile
PNBLACKLIST[ntop] ?= "ERROR: ntop-5.0.1-r0 do_compile: Function failed: do_compile: netflowPlugin.c:2144:1: error: no return statement in function returning non-void [-Werror=return-type]"
# meta-openembedded/meta-python/recipes-devtools/python/python3-twofish_0.3.0.bb:do_compile
PNBLACKLIST[python3-twofish] ?= "ERROR: python3-twofish-0.3.0-r0 do_compile: Function failed: do_compile: twofish.c:45:1: error: control reaches end of non-void function [-Werror=return-type]"
# meta-updater/recipes-test/big-update/big-update_2.0.bb:do_populate_lic
PNBLACKLIST[big-update] ?= "ERROR: big-update-2.0-r0 do_populate_lic: QA Issue: big-update: Recipe file fetches files and does not have license file information (LIC_FILES_CHKSUM) [license-checksum]"
# meta-virtualization/recipes-containers/cri-o/cri-o_git.bb:do_compile
PNBLACKLIST[cri-o] ?= "ERROR: cri-o-1.12.0+git774a29ecf6855f2dff266dc2aa2fe81d7d964465-r0 do_compile: Function failed: do_compile: | Makefile:112: recipe for target conmon/config.h failed"
# meta-virtualization/recipes-containers/docker/docker-ce_git.bb:do_package_qa
PNBLACKLIST[docker-ce] ?= "ERROR: docker-ce-18.09.0-ce+git6e632f7fc395d15bce46f426086e91c01598cf59-r0 do_package_qa: QA Issue: docker-ce-contrib rdepends on bash, but it isnt a build dependency, missing bash in DEPENDS or PACKAGECONFIG? [build-deps]"
# meta-virtualization/recipes-containers/lxc/lxc_3.1.0.bb:do_package_qa
PNBLACKLIST[lxc] ?= "ERROR: lxc-3.1.0-r0 do_package_qa: QA Issue: lxc-ptest rdepends on bash, but it isnt a build dependency, missing bash in DEPENDS or PACKAGECONFIG? [build-deps]"
# meta-virtualization/recipes-containers/singularity/singularity_git.bb:do_package_qa
PNBLACKLIST[singularity] ?= "ERROR: singularity-2.3.1+gitAUTOINC+e214d4ebf0-r0 do_package_qa: QA Issue: singularity rdepends on bash, but it isnt a build dependency, missing bash in DEPENDS or PACKAGECONFIG? [build-deps]"
# meta-virtualization/recipes-core/kata-containers/kata-agent_git.bb:do_package_qa
PNBLACKLIST[kata-agent] ?= "ERROR: kata-agent-git-r0 do_package_qa: QA Issue: ELF binary /jenkins/mjansa/build-ros/webos-dashing-thud/tmp-glibc/work/qemux86-webos-linux/kata-agent/git-r0/packages-split/kata-agent/usr/bin/kata-agent has relocations in .text [textrel]"
# meta-virtualization/recipes-core/kata-containers/kata-runtime_git.bb:do_compile
PNBLACKLIST[kata-runtime] ?= "ERROR: kata-runtime-git-r0 do_compile: oe_runmake failed: make: *** No rule to make target arch/386-options.mk.  Stop."
# meta-virtualization/recipes-core/kata-containers/kata-shim_git.bb:do_package_qa
PNBLACKLIST[kata-shim] ?= "ERROR: kata-shim-git-r0 do_package_qa: QA Issue: ELF binary /jenkins/mjansa/build-ros/webos-dashing-thud/tmp-glibc/work/qemux86-webos-linux/kata-shim/git-r0/packages-split/kata-shim/usr/libexec/kata-containers/kata-shim has relocations in .text [textrel]"
# meta-virtualization/recipes-core/runv/runv_git.bb:do_compile
PNBLACKLIST[runv] ?= "ERROR: runv-0.4.0+gitb360a686abc6c6e896382990ef1b93ef07c7a677-r0 do_compile: Function failed: do_compile: src/import/vendor/src/github.com/hyperhq/runv/hypervisor/qemu/qemu.go:45:28: undefined: QEMU_SYSTEM_EXE"
# meta-virtualization/recipes-devtools/python/python-m2crypto_0.22.3.bb:do_configure
PNBLACKLIST[python-m2crypto] ?= "ERROR: python-m2crypto-0.22.3-r0 do_configure: Function failed: do_configure: Error: CPP #error __WORDSIZE is not defined. Use the -cpperraswarn option to continue swig processing."
# meta-virtualization/recipes-extended/dev86/dev86_0.16.20.bb:do_fetch
PNBLACKLIST[dev86-native] ?= "ERROR: dev86-native-0.16.20-r0 do_fetch: Fetcher failure: Connecting to v3.sk (v3.sk)|68.183.70.209|:80... connected. HTTP request sent, awaiting response... 503 Service Unavailable"
PNBLACKLIST[dev86] ?= "ERROR: dev86-native-0.16.20-r0 do_fetch: Fetcher failure: Connecting to v3.sk (v3.sk)|68.183.70.209|:80... connected. HTTP request sent, awaiting response... 503 Service Unavailable"
# meta-virtualization/recipes-extended/ipxe/ipxe_git.bb:do_compile
PNBLACKLIST[ipxe] ?= "ERROR: ipxe-gitrAUTOINC+64b4452bca-r0 do_compile: Function failed: do_compile: Makefile.housekeeping:111: GNU gold is unsuitable for building iPXE"
# meta-virtualization/recipes-extended/kvmtool/kvmtool.bb:do_compile
PNBLACKLIST[kvmtool] ?= "ERROR: kvmtool-4.14.0+git0e1882a49f81cb15d328ef83a78849c0ea26eecc-r0 do_compile: Function failed: do_compile: virtio/net.c:336:47: error: argument to sizeof in strncpy call is the same expression as the source; did you mean to use the size of the destination? [-Werror=sizeof-pointer-memaccess]"
# meta-virtualization/recipes-extended/libvirt/libvirt_4.7.0.bb:do_package_qa
PNBLACKLIST[libvirt] ?= "ERROR: libvirt-4.7.0-r0 do_package_qa: QA Issue: libvirt-ptest rdepends on bash, but it isn't a build dependency, missing bash in DEPENDS or PACKAGECONFIG? [build-deps]"
# meta-virtualization/recipes-extended/seabios/seabios_1.11.2.bb:do_compile
PNBLACKLIST[seabios] ?= "ERROR: seabios-1.11.2-r0 do_compile: Function failed: do_compile: i586-webos-linux-ld: error: out/rom16.strip.o: unsupported ELF file type 2"
# meta-virtualization/recipes-extended/vgabios/biossums_0.7a.bb:do_package_qa
PNBLACKLIST[biossums] ?= "ERROR: biossums-0.7a-r0 do_package_qa: QA Issue: No GNU_HASH in the ELF binary /jenkins/mjansa/build-ros/webos-dashing-thud/tmp-glibc/work/qemux86-webos-linux/biossums/0.7a-r0/packages-split/biossums/usr/bin/biossums, didnt pass LDFLAGS? [ldflags]"
# meta-virtualization/recipes-networking/cni/cni_git.bb:do_compile
PNBLACKLIST[cni] ?= "ERROR: cni-0.6.0+gitcc562d1b44b3d00b8c341250d49a8a0823a1c085-r0 do_compile: Function failed: do_compile: | ../../../src/github.com/containernetworking/plugins/vendor/github.com/safchain/ethtool/ethtool.go:205:35: undefined: syscall.SYS_SOCKET"
# meta-virtualization/recipes-networking/openvswitch/openvswitch_git.bb:do_package_qa
PNBLACKLIST[openvswitch] ?= "ERROR: openvswitch-2.10.0+5563e309b80bbea9bff538e71ecfd7e5e538bab9-r0 do_package_qa: QA Issue: openvswitch rdepends on bash, but it isnt a build dependency, missing bash in DEPENDS or PACKAGECONFIG? [build-deps]"
# meta-webosose/meta-webos-backports/meta-webos-backports-3.0/recipes-multimedia/gstreamer/gst-validate_1.16.0.bb:do_configure
PNBLACKLIST[gst-validate] ?= "ERROR: gst-validate-1.16.0-r0 do_configure: Function failed: do_configure: configure: Requested gstreamer-1.0 >= 1.16.0 but version of GStreamer is 1.14.4"
# meta-webosose/meta-webos-backports/meta-webos-backports-3.0/recipes-multimedia/gstreamer/gstreamer1.0-rtsp-server_1.16.0.bb:do_configure
PNBLACKLIST[gstreamer1.0-rtsp-server] ?= "ERROR: gstreamer1.0-rtsp-server-1.16.0-r0 do_configure: Function failed: do_configure: | configure: Requested gstreamer-1.0 >= 1.16.0 but version of GStreamer is 1.14.4"
# meta-webosose/meta-webos-backports/meta-webos-backports-3.0/recipes-multimedia/gstreamer/gstreamer1.0-vaapi_1.16.0.bb:do_configure
PNBLACKLIST[gstreamer1.0-vaapi] ?= "ERROR: gstreamer1.0-vaapi-1.16.0-r0 do_configure: Function failed: do_configure: configure: Requested gstreamer-1.0 >= 1.16.0 but version of GStreamer is 1.14.4"

PNBLACKLIST[libvmi] ?= "Depends on blacklisted libvirt"
PNBLACKLIST[openlmi-tools] ?= "Depends on blacklisted python-m2crypto"
PNBLACKLIST[gstd] ?= "Depends on blacklisted gstreamer1.0-rtsp-server"
PNBLACKLIST[vgabios] ?= "Depends on blacklisted dev86-native"
PNBLACKLIST[xen] ?= "Depends on blacklisted dev86-native"
PNBLACKLIST[python-pywbem] ?= "Depends on blacklisted python-m2crypto"
PNBLACKLIST[kubernetes] ?= "Depends on blacklisted cni"
PNBLACKLIST[python3-pyjks] ?= "Depends on blacklisted python3-twofish"

# These are reproducible only with rpi MACHINEs, most likely caused by usrmerge DISTRO_FEATURE enabled with sota from meta-updater
# meta-openembedded/meta-filesystems/recipes-utils/xfsdump/xfsdump_3.1.8.bb:do_configure
PNBLACKLIST[xfsdump] ?= "ERROR: xfsdump-3.1.8-r0 do_configure: Function failed: do_configure: | FATAL ERROR: could not find a current XFS handle library."
# meta-openembedded/meta-filesystems/recipes-utils/xfsprogs/xfsprogs_4.18.0.bb:do_package
PNBLACKLIST[xfsprogs] ?= "ERROR: xfsprogs-4.18.0-r0 do_package: QA Issue: xfsprogs: Files/directories were installed but not shipped in any package:"
# meta-openembedded/meta-multimedia/recipes-connectivity/rygel/rygel_0.34.0.bb:do_install
PNBLACKLIST[rygel] ?= "ERROR: rygel-0.34.0-r0 do_install: Function failed: do_install: rmdir: failed to remove tmp-glibc/work/raspberrypi4-webos-linux-gnueabi/rygel/0.34.0-r0/image/usr/lib/systemd: Directory not empty "
# meta-openembedded/meta-networking/recipes-connectivity/ufw/ufw_0.33.bb:do_package_qa
PNBLACKLIST[ufw] ?= "ERROR: ufw-0.33-r0 do_package_qa: QA Issue: ufw package is not obeying usrmerge distro feature. /lib should be relocated to /usr. [usrmerge]"
# meta-openembedded/meta-networking/recipes-filter/arno-iptables-firewall/arno-iptables-firewall_2.0.1g.bb:do_install
PNBLACKLIST[arno-iptables-firewall] ?= "ERROR: arno-iptables-firewall-2.0.1g-r0 do_install: Function failed: do_install: install: cannot stat tmp-glibc/work/raspberrypi4-webos-linux-gnueabi/arno-iptables-firewall/2.0.1g-r0/arno-iptables-firewall_2.0.1g//usr/lib/systemd/system/arno-iptables-firewall.service: No such file or directory"
# meta-openembedded/meta-networking/recipes-kernel/wireguard/wireguard-module_0.0.20171221.bb:do_package
PNBLACKLIST[wireguard-module] ?= "ERROR: wireguard-module-0.0.20171221-r0 do_package: QA Issue: wireguard-module: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-networking/recipes-protocols/babeld/babeld_1.8.3.bb:do_package
PNBLACKLIST[babeld] ?= "ERROR: babeld-1.8.3-r0 do_package: QA Issue: babeld: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-networking/recipes-support/cifs/cifs-utils_git.bb:do_package
PNBLACKLIST[cifs-utils] ?= "ERROR: cifs-utils-6.7-r0 do_package: QA Issue: cifs-utils: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-networking/recipes-support/linux-atm/linux-atm_2.5.2.bb:do_package_qa
PNBLACKLIST[linux-atm] ?= "ERROR: linux-atm-2.5.2-r0 do_package_qa: QA Issue: linux-atm package is not obeying usrmerge distro feature. /lib should be relocated to /usr. [usrmerge]"
# meta-openembedded/meta-oe/recipes-connectivity/lirc/lirc_0.9.4d.bb:do_package
PNBLACKLIST[lirc] ?= "ERROR: lirc-0.9.4d-r0 do_package: SYSTEMD_SERVICE_lirc value lircmd.service does not exist"
# meta-openembedded/meta-oe/recipes-connectivity/phonet-utils/phonet-utils_git.bb:do_package_qa
PNBLACKLIST[phonet-utils] ?= "ERROR: phonet-utils-0.0.0+gitrAUTOINC+4acfa720fd-r2 do_package_qa: QA Issue: phonet-utils package is not obeying usrmerge distro feature. /lib should be relocated to /usr. [usrmerge]"
# meta-openembedded/meta-oe/recipes-extended/boinc/boinc-client_7.12.bb:do_package
PNBLACKLIST[boinc-client] ?= "ERROR: boinc-client-7.12-r0 do_package: SYSTEMD_SERVICE_boinc-client value boinc-client.service does not exist"
# meta-openembedded/meta-oe/recipes-extended/fluentbit/fluentbit_0.12.1.bb:do_package
PNBLACKLIST[fluentbit] ?= "ERROR: fluentbit-0.12.1-r0 do_package: SYSTEMD_SERVICE_fluentbit value td-agent-bit.service does not exist"
# meta-openembedded/meta-oe/recipes-extended/scsirastools/scsirastools_1.6.6.bb:do_package
PNBLACKLIST[scsirastools] ?= "ERROR: scsirastools-1.6.6-r0 do_package: QA Issue: scsirastools: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-oe/recipes-extended/uml-utilities/uml-utilities_20040406.bb:do_package
PNBLACKLIST[uml-utilities] ?= "ERROR: uml-utilities-20040406-r1 do_package: QA Issue: uml-utilities: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-oe/recipes-graphics/dietsplash/dietsplash_git.bb:do_package
PNBLACKLIST[dietsplash] ?= "ERROR: dietsplash-0.3-r1 do_package: QA Issue: dietsplash: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-oe/recipes-security/tomoyo-tools/tomoyo-tools_2.5.0.bb:do_package
PNBLACKLIST[tomoyo-tools] ?= "ERROR: tomoyo-tools-2.5.0-r0 do_package: QA Issue: tomoyo-tools: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-oe/recipes-shells/tcsh/tcsh_6.20.00.bb:do_install
PNBLACKLIST[tcsh] ?= "ERROR: tcsh-6.20.00-r0 do_install: Function failed: do_install: ln: failed to create symbolic link tmp-glibc/work/raspberrypi4-webos-linux-gnueabi/tcsh/6.20.00-r0/image/usr/bin/tcsh: File exists"
# meta-openembedded/meta-oe/recipes-support/gradm/gradm_3.1.bb:do_package
PNBLACKLIST[gradm] ?= "ERROR: gradm-3.1-r0 do_package: QA Issue: gradm: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-oe/recipes-support/libsmi/libsmi_0.5.0.bb:do_package_qa
PNBLACKLIST[libsmi] ?= "ERROR: libsmi-0.5.0-r0 do_package_qa: QA Issue: /usr/bin/smistrip contained in package libsmi requires /usr/bin/sh, but no providers found in RDEPENDS_libsmi? [file-rdeps]"
# meta-openembedded/meta-oe/recipes-support/usb-modeswitch/usb-modeswitch_2.5.0.bb:do_package
PNBLACKLIST[usb-modeswitch] ?= "ERROR: usb-modeswitch-2.5.0-r0 do_package: QA Issue: usb-modeswitch: Files/directories were installed but not shipped in any package"
# meta-openembedded/meta-python/recipes-devtools/python/python-pyrex_0.9.9.bb:do_package
PNBLACKLIST[python-pyrex] ?= "ERROR: python-pyrex-0.9.9-r4 do_package: QA Issue: python-pyrex: Files/directories were installed but not shipped in any package"
# meta-raspberrypi/recipes-multimedia/omxplayer/omxplayer_git.bb:do_compile
PNBLACKLIST[omxplayer] ?= "ERROR: omxplayer-git-r4 do_compile: Function failed: do_compile: DllAvFormat.h:117:51: error: ::avio_feof has not been declared"
# meta-virtualization/recipes-containers/cgroup-lite/cgroup-lite_1.15.bb:do_package
PNBLACKLIST[cgroup-lite] ?= "ERROR: cgroup-lite-1.15-r0 do_package: QA Issue: cgroup-lite: Files/directories were installed but not shipped in any package"
# meta-virtualization/recipes-containers/lxcfs/lxcfs_3.0.1.bb:do_package
PNBLACKLIST[lxcfs] ?= "ERROR: lxcfs-3.0.1-r0 do_package: SYSTEMD_SERVICE_lxcfs value lxcfs.service does not exist"
# meta-virtualization/recipes-extended/diod/diod_1.0.24.bb:do_package
PNBLACKLIST[diod] ?= "ERROR: diod-1.0.24+gitAUTOINC+0ea3fe3d82-r0 do_package: QA Issue: diod: Files/directories were installed but not shipped in any package"
# meta-webosose/meta-webos/recipes-upstreamable/bootchart2/bootchart2_git.bb:do_package
PNBLACKLIST[bootchart2] ?= "ERROR: bootchart2-0.14.8+gitAUTOINC+42509aa0c9-r10 do_package: SYSTEMD_SERVICE_bootchart2 value bootchart2.service does not exist"
# openembedded-core/meta/recipes-connectivity/avahi/avahi-ui_0.7.bb:do_install
PNBLACKLIST[avahi-ui] ?= "ERROR: avahi-ui-0.7-r0 do_install: Function failed: do_install: rm: cannot remove tmp-glibc/work/raspberrypi4-webos-linux-gnueabi/avahi-ui/0.7-r0/image/usr/lib/libavahi-c*: No such file or directory"
# openembedded-core/meta/recipes-devtools/systemd-bootchart/systemd-bootchart_233.bb:do_package
PNBLACKLIST[systemd-bootchart] ?= "ERROR: systemd-bootchart-233+gitAUTOINC+fe1c5e41e6-r0 do_package: SYSTEMD_SERVICE_systemd-bootchart value systemd-bootchart.service does not exist"

PNBLACKLIST[wireguard-tools] ?= "Depends on blacklisted wireguard-module"
PNBLACKLIST[python-snimpy] ?= "Depends on blacklisted libsmi"
PNBLACKLIST[lcdproc] ?= "Depends on blacklisted lirc"
PNBLACKLIST[usb-modeswitch-data] ?= "Depends on blacklisted usb-modeswitch"

# Few more failing in warrior:
# meta-virtualization/recipes-networking/netns/netns_git.bb:do_compile_ptest_base
PNBLACKLIST[netns] ?= "ERROR: netns-0.5.3-r0 do_compile_ptest_base: Function failed: do_compile_ptest_base: | bridge.go:10:2: cannot find package "github.com/genuinetools/netns/netutils""
# meta-openembedded/meta-multimedia/recipes-multimedia/dvb-apps/dvb-apps_1.1.1.bb:do_package_qa
PNBLACKLIST[dvb-apps] ?= "ERROR: dvb-apps-1.1.1-r0 do_package_qa: QA Issue: dvb-apps package is not obeying usrmerge distro feature. /lib should be relocated to /usr. [usrmerge]"
# meta-updater/recipes-support/slcand-start/slcand-start.bb:do_populate_lic
PNBLACKLIST[slcand-start] ?= "ERROR: slcand-start-1.0-r0 do_populate_lic: QA Issue: slcand-start: The LIC_FILES_CHKSUM does not match for file:///jenkins/mjansa/build-ros/webos-eloquent-warrior/openembedded-core/LICENSE;md5=4d92cd373abda3937c2bc47fbc49d690"

PNBLACKLIST[tvheadend] ?= "Depends on blacklisted dvb-apps"

# Few more in zeus, but we can blacklist them in advance in thud branch, to keep it simple
PNBLACKLIST[remmina] ?= "Depends on blacklisted avahi-ui"
PNBLACKLIST[packagegroup-meta-multimedia] ?= "Depends on blacklisted gstd, gerbera, rygel"
