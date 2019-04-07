# Generic Communication with USB HID Class - Examples

Tutorial page: <https://ravikiranb.com/tutorials/generic-comm-usb-hid/>  

Examples are tested on Ubuntu 16.04 LTS.


## Software Requirements

* [NXP MCUXpresso IDE](https://www.nxp.com/support/developer-resources/software-development-tools/mcuxpresso-software-and-tools/mcuxpresso-integrated-development-environment-ide:MCUXpresso-IDE)
* Python 3.5
* [PyUSB](https://github.com/pyusb/pyusb/blob/master/docs/tutorial.rst)


## Hardware Requirements

* [NGX LPC4357 Xplorer++ Development Board](https://ngxkart.com/products/lpc4357-xplorer)
* Any ARM Cortex CMSIS DAP compatible debugger to flash and debug the firmware.

## Get Source Code

$ git clone https://github.com/rkprojects/generic-comm-usb-hid-examples.git

## Compile Source Code

* In MCUXpresso switch workspace to cloned directory *generic-comm-usb-hid-examples*.
* Add all projects inside directory *generic-comm-usb-hid-examples* to workspace.
* Build project *lpc4357_usb_custom_hid*.
* *master* branch contains LED control example.
* *system_power_control* branch contains host sleep mode example.

## LED Example

* Checkout *master* branch, compile and flash the firmware and connect USB1 to host. 
* $ cd generic-comm-usb-hid-examples/lpc4357_usb_custom_hid/tools
* $ sudo python3 hid_host_test.py
* Follow on screen instructions.
* You can add the usb device in udev rules to avoid running script as sudo.
* Pressing user switch SW2 on board will cause read interrupt in test tool.

## System Power Control Example

* Checkout *system_power_control* branch, compile and flash the firmware and connect USB1 to host. 
* Cross check with *lsusb* whether device got detected.
* Pressing user switch SW2 will cause computer to enter sleep mode.
* If you are trying it on Windows (not tested) ensure Sleep mode is enabled.

## License

Some of the firmware source code files are under MIT license, others are under NXP's LPCOpen License. 
Source code header lists the corresponding copyright notice.  

Python host test application source code files are under MIT license.