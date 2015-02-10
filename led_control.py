#!/usr/bin/env python 

import sys
import select
import time
import Adafruit_BBIO.GPIO as GPIO

modes = {
    'i': 'docked',
    'd': 'downloading',
    'c': 'converting',
    'u': 'uploading',
    'o': 'done'
}

led1 = "P8_7"
led2 = "P8_11"
led3 = "P8_13"

mode = 'ready'
on = True

GPIO.setup(led1, GPIO.OUT)
GPIO.setup(led2, GPIO.OUT)
GPIO.setup(led3, GPIO.OUT)

def high(led):
    GPIO.output(led, GPIO.HIGH)

def low(led):
    GPIO.output(led, GPIO.LOW)

def read_input():
    c = sys.stdin.read(1)
    try:
        mode = modes[c]
        sys.stdout.write("Switched to mode: %s\n" % (mode))
        sys.stdout.flush()
	return mode
    except:
        return 'ready'

low(led1)
low(led2)
low(led3)



while True:
    if select.select([sys.stdin,],[],[],0.0)[0]:
        mode = read_input()

    if mode == 'docked':
        high(led1)
        low(led2)
        low(led3)
                
    elif mode == 'downloading':
        if on:
            high(led1)
            low(led2)
            low(led3)
        else:
            low(led1)
            low(led2)
            low(led3)

    elif mode == 'converting':
        if on:
            high(led1)
            high(led2)
            low(led3)
        else:
            high(led1)
            low(led2)
            low(led3)

    elif mode == 'uploading':
        if on:
            high(led1)
            high(led2)
            high(led3)
        else:
            high(led1)
            high(led2)
            low(led3)

    elif mode == 'done':
        high(led1)
        high(led2)
        high(led3)

    time.sleep(0.5)
    if on:
        on = False
    else:
        on = True


GPIO.cleanup()
