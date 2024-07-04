#!/bin/bash -e
#
# Copyright (c) 2024 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

#根据原始的zephyr.bin_orig文件得到哈希文件
#openssl dgst -sha256 zephyr_no_sign.bin

#将签名字符串解码成二进制文件
echo -n "NI7JK7lT5hh6npHBtKFthZubvieAKNbeKFtwJF6Kl0zo0a2XSeeZknzNFZfiywZHzS3vh9IIFW62+KwKjyK2C0XzlmmVQUvzGe5p2dTuCoGMtTwXiLDGPx/rHvvpJ5DvhPnhSyTDBkguLpRBdHfQOShS+mBJM14Sd+uA7YezWzxxV+Mk7ZL2c/gUI+gkY+wzaeiQNr8uz0mpc6LOerfxtRomnvfVuhhoX/n3x7qKGP1BdNuOGpF5RayZnwSXg4PK8cckMUcs/TP2M4lw1cbldq1IBqNrosSo6E/kD/QgHboKjkcuJJ1PzcKkvEBJISdckTHAfpFq4k4DAOtJ3EV31g==" | base64 -d > zephyr_cert.bin

#验证签名数据是否合法
openssl dgst -sha256 -verify ../../../../samples/bt_speaker/app_conf/flip7/public.pem -signature zephyr_cert.bin zephyr_no_sign.bin



