`meta-ros-webos` is the OpenEmbedded layer for interfacing between `meta-ros`
and webOS OSE.

As of Milestone 17 (2022-06-05), this branch can be used to add the releases
current in mid-November 2021 of ROS 2 **foxy** and **galactic** and ROS 1
**melodic** and **noetic** to webOS OSE 2.14.0 (which is built with the
**dunfell** OpenEmbedded release series).

The mid-November 2021 release (and earlier releases) of ROS 2 **rolling** can no
longer be built because the commits referenced by its recipes have been removed
rom the `[release/rolling/*]`branches of the release repositories.

A "best effort" has been made to keep the EOL-ed ROS 2 **dashing** and
**eloquent** distros buildable, but some of the packages that are used to create
the `webos-image-world` image do not build. However, the `webos-image-core`
image does successfully build.

Instructions for using `meta-ros` are
[here](https://github.com/ros/meta-ros/wiki/OpenEmbedded-Build-Instructions).

## Status

With the exception of the commits added to complete Milestone 17, this repository
has not been maintained since December 2021. Its future maintenance was
discussed in the
[OSRF TSC January 2022 meeting post](https://discourse.ros.org/t/os-2-tsc-meeting-january-20th-2022/23986/2).
