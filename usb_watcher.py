#!/usr/bin/env python

import argparse
import daemon
import sys
import os
import glib
import gudev
import time
import subprocess

#############################################################
# Watch for USB connection of LiveScribe Pulse or Echo pens #
# and run dumpscribe and the unmuddle.py script on connect  #
#############################################################

LS_VENDOR_ID = '1cfb'
LS_PEN_IDS = ['1010', '1020', '1030', '1032']

led_modes = {
    'docked': 'i',
    'downloading': 'd',
    'converting': 'c',
    'uploading': 'u',
    'done': 'o'
}

led_process = None # the sub-process controlling the LEDs

def led_mode(mode):
    if not led_process:
        return
    try:
        mode_char = led_modes[mode]
    except:
        return
    led_process.stdin.write(mode_char)
    txt = led_process.stdout.readline()

def led_process_start():
    led_process = subprocess.Popen(['./led_control.py'], shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

def led_process_stop():
    led_process.terminate()
    led_process.wait()


def pen_detected():
    print "LiveScribe pen detected!"
    led_mode('docked')
    dump_dir = args.dumpscribe_output_dir[0]
    organized_dir = args.organized_output_dir[0]
    dumpscribe = os.path.join(args.dumpscribe_dir[0], 'dumpscribe')
    dumpscribe_cmd = '"%s" -c "%s"' % (dumpscribe, dump_dir)
    organize = os.path.join(args.dumpscribe_dir[0], 'unmuddle.py')
    organize_cmd = '"%s" --thumb "%s" "%s"' % (organize, dump_dir, organized_dir)
    if args.cleanup_dir:
        cleanup = os.path.join(args.dumpscribe_dir[0], 'cleanup.py')
        cleanup_cmd = '"%s" "%s"' % (cleanup, args.cleanup_dir)
    else:
        cleanup_cmd = False

    led_mode('downloading')
    ret = subprocess.call(dumpscribe_cmd, shell=True)
    if ret != 0:
        sys.stderr.write("dumpscribe failed\n")
        return

    led_mode('converting')
    ret = subprocess.call(organize_cmd, shell=True)
    if ret != 0:
        sys.stderr.write("unmuddle failed.\n")
        return

    if cleanup_cmd:
        print "Cleaning up"
        ret = subprocess.call(cleanup_cmd, shell=True)
        if ret != 0:
            sys.stderr.write("cleanup failed.\n")
            return

    if args.post_command:
        led_mode('uploading')
        print "Running user-supplied command:"
        ret = subprocess.call(args.post_command[0], shell=True)
        if ret != 0:
            sys.stderr.write("User-supplied command failed.\n")
            return

    led_mode('done')    
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

    if args.enable_leds:
        led_process_start()
    
    loop = glib.MainLoop()
    loop.run()

    if args.enable_leds:
        led_process_stop()

parser = argparse.ArgumentParser(
    description='Automatically run dumpscribe when LiveScribe pen is connected.'
)
parser.add_argument('-d', dest='daemonize', action='store_true',
                    help='Daemonize this process')
parser.add_argument('-l', dest='enable_leds', action='store_true',
                    help='Enable Beagle Bone Black LED control')
parser.add_argument('-c', dest='post_command', nargs=1,
                    help='Command to run after running unmuddle.py')
parser.add_argument('dumpscribe_dir', nargs=1,
                    help="The full path to the directory where dumpscribe is installed.")
parser.add_argument('dumpscribe_output_dir', nargs=1, 
                    help="Where dumpscribe should place its output.")
parser.add_argument('organized_output_dir', nargs=1, 
                    help="Where unmuddle.py should place its output.")
parser.add_argument('cleanup_dir', nargs='?',
                    help="Optional directory to clean up if drive is more than 50 percent full.")


args = parser.parse_args()

daemon_workdir = None

if args.daemonize:
    print "Starting dumpscribe usb watcher and daemonizing"
    with daemon.DaemonContext(working_directory=args.dumpscribe_dir[0]):
        run()
else:
    print "Starting dumpscribe usb watcher"
    run()



