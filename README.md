Nighttime-Alert
===============

Nighttime-Alert prevent you from accidentally browsing the Internet late into the night. Set Cron to launch it just before your bedtime and it will give your a warning and time to save your work before forcibly shutting down the computer.

    make
    make install

Should be enough to install it most of the time. On some systems, the rule:

    <allow send_destination="org.freedesktop.systemd1"
           send_interface="org.freedesktop.systemd1.Manager"
           send_member="StartUnit"/>

will need to be added to /etc/dbus-1/system.d/org.freedesktop.systemd1.conf before nighttime-alert can shutdown the system.

Currently, Nighttime-Alert depends on [systemd](http://en.wikipedia.org/wiki/Systemd) to work properly; patches welcome.

[MIT Licensed](LICENSE)
