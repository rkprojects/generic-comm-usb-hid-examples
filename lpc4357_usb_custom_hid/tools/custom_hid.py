# Class wrapper over PyUSB to talk to underlying HID device.
#
# Copyright (C) 2019 Ravikiran Bukkasagara, <contact@ravikiranb.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
# THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# TAB = 4 spaces
#

import usb.core
import usb.util

import threading

_USB_HID_CLASS_CTRL_bmRequestType = 0x21
_USB_HID_CLASS_CTRL_bRequest_SET_REPORT = 0x09
_USB_HID_CLASS_CTRL_wValue_REPORT_TYPE_INPUT = 0x01 << 8
_USB_HID_CLASS_CTRL_wValue_REPORT_TYPE_OUTPUT = 0x02 << 8
_USB_HID_CLASS_CTRL_wValue_REPORT_TYPE_FEATURE = 0x03 << 8

_USB_CLASS_bmRequestType_GET_DESCRIPTOR = 0x81
_USB_CLASS_bRequest_GET_DESCRIPTOR = 6
_USB_CLASS_wValue_GET_HID_REPORT_DESCRIPTOR = (0x22) << 8 # Report Descriptor


_USB_HID_CLASS_REPORT_DESCRIPTOR_TYPE = 0x22
_BLINK_RATE_MAX = 20
_BLINK_RATE_MIN = 1

class CustomHID:
    def __init__(self, vendor_id, product_id):
        self.device = usb.core.find(idVendor=vendor_id, idProduct=product_id)
        self.vid_pid_str = "{0:04X}:{1:04X}".format(vendor_id, product_id)
        
        if self.device is None:
            raise Exception("USB Device ID {0} not found".format(self.vid_pid_str))
        
        # There is only one configuration for this device.
        cfg = self.device.get_active_configuration()
        if cfg is None:
            cfg = self.device[0]
            cfg.set()
    
        # There is only one interface for this device.
        intf = cfg[(0,0)]
        
        self.interface_number = intf.bInterfaceNumber
        
        self.kernel_driver_was_active = self.device.is_kernel_driver_active(self.interface_number)
        if self.kernel_driver_was_active:
            self.device.detach_kernel_driver(self.interface_number)
        
        print("")
        print("USB Device ID {0} found".format(self.vid_pid_str))
        print("Manufacturer: {0}".format(self.device.manufacturer))
        print("Product: {0}".format(self.device.product))
        print("Serial Number: {0}".format(self.device.serial_number))
            
        # Interrupt IN, OUT endpoints.
        self.ep_in = intf[0]
        self.ep_out = intf[1]    
            
        self.close_thread = False
        self.poll_th = threading.Thread(target=self._poll_ep_in)
        self.poll_th.start()
        self.led5_state = 0
        
    def _poll_ep_in(self):
        while self.close_thread == False:
            try:
                self.ep_in.read(1, 1000)
                print("\n\n***\nInterrupt IN Endpoint: SW2 switch pressed.\n***")
            except usb.core.USBError as e:
                if "timed out" in str(e):
                    pass
                else:
                    print(e)
                    print("aborting EP_IN polling")
                    return
            except Exception as e:
                print(e)
                return

    def toggle_led5(self):
        self.led5_state = self.led5_state ^ 1
        self.ep_out.write([self.led5_state])
    
    def set_led4_blink_rate(self, rate_hz):
        self.device.ctrl_transfer(_USB_HID_CLASS_CTRL_bmRequestType,
                            _USB_HID_CLASS_CTRL_bRequest_SET_REPORT,
                            _USB_HID_CLASS_CTRL_wValue_REPORT_TYPE_FEATURE,
                            self.interface_number,
                            bytes([rate_hz]))
    
    def close(self):
        self.close_thread = True
        self.poll_th.join()
        if self.device is None:
            return
            
        usb.util.dispose_resources(self.device)
        if self.kernel_driver_was_active:
            self.device.attach_kernel_driver(self.interface_number)
