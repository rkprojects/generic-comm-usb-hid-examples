# USB HID Host Test Application
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
 
import textwrap
import threading

from custom_hid import CustomHID 

hid = None
try:
    VID = 0x1209
    PID = 0x0001
    
    print (textwrap.dedent("""
    This VID:PID {0:04X}:{1:04X} is for **in-house testing only**.
    Do Not Use it outside your testing environment.
    """.format(VID, PID)))
    
    hid = CustomHID(vendor_id=VID, product_id=PID)
    
    prompt = textwrap.dedent("""
        Choices:
        1) Toggle LED 5
        2) Set LED 4 Blink Rate. 
        \tEnter "2 Rate" (without quotes)
        \twhere Rate is in number of blinks per second.
        \tExample "2 4" LED 4 will blink four times per second.
        q) Quit
        Enter choice: """)

    while True:
        choice = input(prompt).strip().lower()
        if choice == "1":
            hid.toggle_led5()
        elif choice.startswith("2 "):
            params = choice.split(maxsplit=2)
            if (len(params) == 2) and params[1].isdigit():
                rate = int(params[1])
                hid.set_led4_blink_rate(rate)
            else:
                print("**Error** Invalid input: {0}".format(choice))
        elif choice == "q":
            break
        else:
            print("**Error** Invalid input: {0}".format(choice))

    hid.close()
    
except Exception as e:
    print(e)
    if hid is not None:
        hid.close()
