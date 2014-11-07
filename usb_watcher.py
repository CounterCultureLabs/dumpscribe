#!/usr/bin/env python

import argparse
import daemon
import sys
import glib
import gudev

#############################################################
# Watch for USB connection of LiveScribe Pulse or Echo pens #
#############################################################

# TODO should be a command line argument
dumpscript_dir = "/home/juul/projects/real_vegan_cheese/livescribe/dumpscribe"

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

def run():
    client = gudev.Client(["usb/usb_device"])
    client.connect("uevent", callback, None)
    
    loop = glib.MainLoop()
    loop.run()

parser = argparse.ArgumentParser(description='Automatically run dumpscribe when LiveScribe pen is connected.')
parser.add_argument('-d', dest='daemonize', action='store_true',
                    help='Daemonize this process')

args = parser.parse_args()

if args.daemonize:
    print "Starting dumpscribe usb watcher and daemonizing"
    with daemon.DaemonContext():
        run()
else:
    print "Starting dumpscribe usb watcher"
    run()


