#!/usr/bin/env python

import argparse
import daemon
import sys
import os
import glib
import gudev
import subprocess

#############################################################
# Watch for USB connection of LiveScribe Pulse or Echo pens #
# and run dumpscribe and convert_and_organize.py on connect "
#############################################################

LS_VENDOR_ID = '1cfb'
LS_PEN_IDS = ['1010', '1020', '1030', '1032']

def pen_detected():
    print "LiveScribe pen detected!"
    dump_dir = args.dumpscribe_output_dir[0]
    organized_dir = args.organized_output_dir[0]
    dumpscribe = os.path.join(args.dumpscribe_dir[0], 'dumpscribe')
    dumpscribe_cmd = '"%s" "%s"' % (dumpscribe, dump_dir)
    organize = os.path.join(args.dumpscribe_dir[0], 'convert_and_organize.py')
    organize_cmd = '"%s" --thumb "%s" "%s"' % (organize, dump_dir, organized_dir)

    ret = subprocess.call(dumpscribe_cmd, shell=True)
    if ret != 0:
        sys.stderr.write("dumpscribe failed\n")
        return

    ret = subprocess.call(organize_cmd, shell=True)
    if ret != 0:
        sys.stderr.write("convert_and_organize.py failed.\n")
        return

    if len(args.post_command) > 0:
        print "Running user-supplied command:"
        ret = subprocess.call(args.post_command[0], shell=True)
        if ret != 0:
            sys.stderr.write("User-supplied command failed.\n")
            return

    print "All scripts completed successfully!"

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

parser = argparse.ArgumentParser(
    description='Automatically run dumpscribe when LiveScribe pen is connected.'
)
parser.add_argument('-d', dest='daemonize', action='store_true',
                    help='Daemonize this process')
parser.add_argument('-c', dest='post_command', nargs=1,
                    help='Command to run after running convert_and_organize.py')
parser.add_argument('dumpscribe_dir', nargs=1,
                    help="The full path to the directory where dumpscribe is installed.")
parser.add_argument('dumpscribe_output_dir', nargs=1, 
                    help="Where dumpscribe should place its output.")
parser.add_argument('organized_output_dir', nargs=1, 
                    help="Where convert_and_organize should place its output.")

args = parser.parse_args()

if args.daemonize:
    print "Starting dumpscribe usb watcher and daemonizing"
    with daemon.DaemonContext():
        run()
else:
    print "Starting dumpscribe usb watcher"
    run()


