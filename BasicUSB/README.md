# Basic USB Driver #
The Basic USB Driver is a KMDF USB driver that has all but the minimum functionality stripped out. It is written to support the OSR USB FX2 Learning Kit Device:

[https://store.osr.com/product/osr-usb-fx2-learning-kit-v2/](https://store.osr.com/product/osr-usb-fx2-learning-kit-v2/)

## Building the Sample
The provided solution builds with Visual Studio 2015 and the Windows 10 1607 Driver Kit.

## Installation ##
The driver installs using the following command:

    devcon.exe update basicusb.inf USB\Vid_0547&Pid_1002

The devcon utility can be found in the Tools subdirectory of the Windows Driver Kit installation