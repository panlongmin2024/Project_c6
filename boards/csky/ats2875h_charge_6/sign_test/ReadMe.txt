zephyr固件签名的生成流程:

1.如果当前目录存在zephyr_no_sign.bin(原始的zephyr文件，可在_firmware/bin_no_sign/zephyr_no_sign.bin得到)，则sdk打包脚本会将该zephyr_no_sign.bin覆盖原有生成的zephyr.bin,
而原有的zephyr.bin会重命名到_firmware/bin_no_sign/zephyr_no_sign.bin

2.如果当前目录存在zephyr_cert.bin(该文件需要将zephyr_no_sign.bin发给签名厂商，由厂商签名后转换的来，转换脚本为convert_zephyr_signature.sh)， 则会将该zephyr_cert.bin拷贝到firmware/bin目录下。如果不存在zephyr_cert.bin, 则打包脚本会尝试与服务器通信将zephyr_no_sign.bin进行数字签名，形成zephyr_cert.bin

3.如果存在zephyr_no_sign.bin和zephyr_cert.bin, 打包脚本会把sign目录下的这两个文件覆盖原有的文件，并生成新的带签名的zephyr.bin
4.如果后续厂商签名自动化流程ok，则不需要在sign目录下使用zephyr.bin和zephyr_cert.bin了

ota固件签名的生成流程:
1.必须先完成zephyr.bin的签名过程才可以进行ota固件的签名

2.如果当前目录存在ota_no_sign.bin(原始的ota文件，可在_firmware/bin_no_sign/ota_no_sign.bin得到)，则sdk打包脚本会将该ota_no_sign.bin覆盖原有生成的ota.bin,
而原有的ota.bin会重命名到_firmware/bin_no_sign/ota_no_sign.bin

3.如果当前目录存在ota_cert.bin(该文件需要将ota_no_sign.bin发给签名厂商，由厂商签名后转换的来，转换脚本为convert_ota_signature.sh)， 则会将该ota_cert.bin拷贝到firmware/目录下。如果不存在ota_cert.bin, 则打包脚本会尝试与服务器通信将ota_no_sign.bin进行数字签名，形成ota_cert.bin

4.如果存在ota_no_sign.bin和ota_cert.bin, 打包脚本会把sign目录下的这两个文件覆盖原有的文件，并生成新的带签名的ota.bin

5.如果后续厂商签名自动化流程ok，则不需要在sign目录下使用zephyr.bin和zephyr_cert.bin了