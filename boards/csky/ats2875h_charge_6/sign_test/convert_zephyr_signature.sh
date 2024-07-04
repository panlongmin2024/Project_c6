#!/bin/bash -e
#
# Copyright (c) 2024 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

#根据原始的zephyr.bin_orig文件得到哈希文件
#openssl dgst -sha256  -binary -out hash.bin zephyr.bin

#将签名字符串解码成二进制文件
echo -n "Am0vKSOdbznDaPUZ3BWyJgwX4i3BvPEo5h7WqbalQBEM87S4GYpHbaZLzjEpF2wQEEJvEKIxLDEN0E7RyA3CJ3KGTbRwSDJIAGWfGk/soyfei4vPwFnPy64zbjHOuzI70CzO9VizkhlaGnZTMxjUd4Cu4PkrgJAT/7m+VjyTQzYMXPlUf/tqzBiJNHg1YGIuMFhkzmpSOjW96GEc7Hk0jYM9WOem6ztgyS4RarcSmSka0F9PeCTKo4iqrsmu6pVQitUGW4ZgDLd3g8QkignM58S1n8K8K2J+kCCOxZ/QpKy4gAsjzd5iiv/iEi3riaDvFM1clsGraldGx08OsyA2FA==" | base64 -d > zephyr_cert.bin

#验证签名数据是否合法
openssl dgst -sha256 -verify ../../../../samples/bt_speaker/app_conf/charge6/public.pem -signature zephyr_cert.bin zephyr_no_sign.bin



