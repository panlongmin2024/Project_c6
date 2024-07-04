#!/bin/bash -e
#
# Copyright (c) 2024 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

#根据原始的zephyr.bin_orig文件得到哈希文件
openssl dgst -sha256  -binary -out hash.bin ota_no_sign.bin

#将签名字符串解码成二进制文件
echo -n "RmWwR1Y/87lUxA7b+Bn03g6X8XpCVI5bqHYPEUItN8hK0oAxwPc+rQQYg5/YcT+7pKRErsay/2AEq1BcDYcy2GG3pG04aBqE983qLrT/Wy2fG//lctzduVEUEvGMXQwU/Ne8fX+eBtEVC0KEcH+mAo5Gs0MOePal9lA8SAKvrhr90e1/Pts1xP0xKtGVBMoZ4Lvkrb9oqNFER1SdXpHcbyiwe3fyRibUE7S4Epx7eFfqQ6d0pm9clM3GzhmhBwYeCNbnKBU1QQ8NrSQN28l5EkPSJ0faplwhB6SJmC9imk//RHw8g7T4yWHS9oP98xUQwXieXUGI+jVe2mV/oZ85wg==" | base64 -d > ota_cert.bin

#验证签名数据是否合法
openssl dgst -sha256 -verify ../../../../samples/bt_speaker/app_conf/charge6/public.pem -signature ota_cert.bin ota_no_sign.bin

