
# Reverse-engineering recording meta-data

The meta-data for written notes is available as XML but the meta-data for recordings is stored in binary files using a proprietary format. 

Recordings are stored in e.g:

* userdata/AYE-AP7-BSH-66/Paper Replay/223/0/sessions/PRS-223277b7e/

The number after "PRS-" is the datetime of recording in hex.

The dir contains two or three files:

* audio-0.aac
* session.info
* session.pages (only present if written notes were taken during recording)

Since session.info does not seem to contain any page information, recordings seem to only become associated with a page if written notes are taken during recording.

# audio-0.aac

The .aac file appears to contain no meta-data. 

## session.info

* Byte 0 to 7 is always "fa ce 03 00 00 00 00 00"
* Byte 8 to 15 is the recording start datetime.
* Byte 16 to 23 is the recording end datetime.
* Byte 24 to 31 is not known but is also a datetime

## session.pages

* First 6 bytes: Unknown. Probably always "ca be 01 00 00 01".
* Next 8 bytes: Page identifier
* Next 8 bytes: Unknown (maybe page area?)
* Next 8 bytes: Page identifier
* and so on

### Page identifier 

An example page identifier looks like 2208.1.12.89

* Bytes 0 to 2: The first part of the page identifier (e.g. 2208)
* Bytes 9 to 10: The second part of the page identifier (e.g. 1)
* Byte 11 and first half of 12: The third part.
* Second half of 12 and 13: The fourth part.

