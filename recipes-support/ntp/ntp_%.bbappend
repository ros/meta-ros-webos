# Copyright (c) 2021 LG Electronics, Inc.

do_install:prepend() {
    # As recommeneded by https://developers.google.com/time/faq#how_do_i_use_google_public_ntp and
    # https://developers.google.com/time/guides
    sed -i -e  '/time.server.example.com/ s/^.*$/server time.google.com iburst prefer/' ${WORKDIR}/ntp.conf
}
