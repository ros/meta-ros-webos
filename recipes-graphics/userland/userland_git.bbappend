# Copyright (c) 2019 LG Electronics, Inc.

# We need mmal for raspicam-node and mmal need vcsm from host_applications/linux/libs/sm/
SRC_URI_remove = "file://0002-Remove-EGL-dependency.patch"
