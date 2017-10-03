# KMDF Nothing Driver #
The Nothing Driver is OSR's classic "do nothing" example. It is a Root Enumerated device driver that supports both read and write operations with the provided test application.

## Building the Sample
The provided solution builds with Visual Studio 2015 and the Windows 10 1607 Driver Kit.

## Installation ##
The driver installs using the following command:

    devcon.exe install nothing_kmdf.inf root\Nothing

The devcon utility can be found in the Tools subdirectory of the Windows Driver Kit installation