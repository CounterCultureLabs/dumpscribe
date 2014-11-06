#!/usr/bin/env python

import sys
import glib
import gudev

# Watch for USB connection of LiveScribe Pulse or Echo pens

LS_VENDOR_ID = 7419
LS_PEN_IDS = [4128, 4112, 4144, 4146]

def pen_detected():
    print "LiveScribe pen detected!"
    # TODO run relevant scripts

def callback(client, action, device, user_data):
    if action != "add":
        return

    vendor_id = device.get_property("ID_VENDOR_ID")
    model_id = device.get_property("ID_MODEL_ID")

    if vendor_id != LS_VENDOR_ID:
        return
    
    for dev_id in LS_PEN_IDS:
        if model_id == dev_id:
            pen_detected()


client = gudev.Client(["usb/usb_device"])
client.connect("uevent", callback, None)

loop = glib.MainLoop()
loop.run()
