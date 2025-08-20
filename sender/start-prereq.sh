#!/bin/bash

apt -y install python3-dbus python3-bluez
apt -y install pipewire pipewire-audio-client-libraries wireplumber libspa-0.2-bluetooth pipewire-audio-client-libraries
apt -y install libpipewire-0.3-dev pkg-config

systemctl --user --now enable pipewire pipewire-pulse wireplumber

sudo systemctl restart bluetooth.service
systemctl --user restart pipewire.service
systemctl --user restart wireplumber.service

wpctl status