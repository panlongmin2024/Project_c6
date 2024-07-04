#!/usr/bin/env python
#
# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

import os
import sys
import requests
import json 



def main(argv):
    url = 'http://192.168.40.101:8001/sign-firmware-development'
    # url = 'http://tms-be7f007b:8001/sign-firmware-development'
    # url = 'http://tms-3ead2:8001/sign-firmware-development'


    headers = {
        "Content-type": "application/json"
    }
    data = {
        "hashOfFirmwareImage": "88aac46aeb3c0931f91665d5948e031943d63f8d4129bc9eef52c2c1b192c917",
        "productModelName": "JBL Test Speaker"
    }
    response = requests.post(url, data=json.dumps(data), headers=headers)
    print(response.text)
    print("test finished")
    
"""
def main(argv):
    #url = 'http://tms-be7f007b:8001/sign-hardware-id'
    url = 'http://tms-3ead2:8001/sign-hardware-id'
    data =   {
        "hashOfHardwareId1": "1234sdfsa56789abcdef",
        "hardwareId1": "HWID12345",
        "productModelName": "JBL Bar 300"
    }
    response = requests.post(url, data=data)
    print("test RE:")
    print(response.text)
    print("test finished")
"""



if __name__ == "__main__":
    main(sys.argv)
