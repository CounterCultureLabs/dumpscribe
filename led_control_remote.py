#!/usr/bin/env python

import time
import subprocess

p = subprocess.Popen(['./led_control.py'], shell=True, stdin=subprocess.PIPE, st$

p.stdin.write("d")
print p.stdout.readline()
p.stdin.write("o")
print p.stdout.readline()

# send sigterm and wait for child process to terminate
p.terminate()
p.wait()
