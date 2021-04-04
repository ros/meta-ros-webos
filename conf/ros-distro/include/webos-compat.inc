# Copyright (c) 2019-2021 LG Electronics, Inc.
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

# webos-rpi.inc _removes "kernel-modules" from MACHINE_EXTRA_RRECOMMENDS, and _remove happens last, so we can't add it back to
# MACHINE_EXTRA_RRECOMMENDS.
IMAGE_INSTALL_append_rpi = " kernel-modules"

# Move from base_prefix to prefix for ostree to update it.
ros_prefix = "${prefix}/opt/ros/${ROS_DISTRO}"

# python*-pyqt5 is blacklisted, because webOS OSE is missing newer meta-qt5 from warrior with older
# meta-oe thud which has incompatible old sip:
# python-pyqt5 was skipped: Recipe is blacklisted: needs newer sip than 4.19.12 what we have in thud, This version of PyQt5 requires sip 4.19.14 or later. http://caprica.lgsvl.com:8080/Errors/Details/1509139
ROS_WORLD_SKIP_GROUPS += "pyqt5"

# widgets PACKAGECONFIG is explicitly disabled in webOS OSE qtbase:
# meta-webos/recipes-qt/qt5/qtbase_git.bbappend:PACKAGECONFIG_remove = "widgets"
ROS_WORLD_SKIP_GROUPS += "qt5-widgets"

# webOS OSE already sets necessary license flags:
# meta-webos/conf/distro/webos.conf:LICENSE_FLAGS_WHITELIST_append = " commercial_gstreamer1.0-omx"
ROS_WORLD_SKIP_GROUPS_remove = "world-license"
# and removes the restricted faad dependency:
# meta-webos-raspberrypi/recipes-multimedia/gstreamer/gstreamer1.0-plugins-bad_1.14.%.bbappend:PACKAGECONFIG_remove_rpi = "faad"
ROS_WORLD_SKIP_GROUPS_remove = "world-license-faad"

# meta-raspberrypi/recipes-multimedia/omxplayer/omxplayer_git.bb:do_compile
# meta-webos/conf/distro/include/webos-recipe-blacklist.inc:PNBLACKLIST[omxplayer] ?= "ERROR: omxplayer-git-r4 do_compile: Function failed: do_compile: DllAvFormat.h:117:51: error: ::avio_feof has not been declared"
PNBLACKLIST[omxplayer] ?= "ERROR: omxplayer-git-r4 do_compile: Function failed: do_compile: DllAvFormat.h:117:51: error: ::avio_feof has not been declared"
