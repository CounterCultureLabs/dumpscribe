
Downloads all written notes and recorded audio from a LiveScribe Pulse or Echo pen.

This program has two parts:
 
* The dumpscribe command downloads and extracts all relevant info to tmp/ in the raw format form the pen. It is written in C.
* The convert_and_organize.py command converts written notes to PDF, parses meta-data and puts everything into a sane folder structure in out/

# TODO

* usb_watcher.py
** Make it actually call dumpscribe and convert_and_organize.py
** Add upstart script
* Make dumpscript calculate and write timestamp offset to disk when run
** Needs to get the current time from peninfo, calc offset, and write
* Add command line arguments to dumpscribe
* Add support for deleting files from the pen (need to reverse-engineer)
* Get rid of compile warnings related to xmlChar vs. char

# Requirements

TODO list the required packages

# Compiling

Just run:

```
make
```

# Usage 

This usage text is aspirational.

```
./dumpscribe <destination_directory>

  -h   Print this help text.
  -v   Enable verbose mode (debug output).

  All written and audio notes will be downloaded from the smartpen and put into the destination directory. 
```

```
./convert_and_organize.py <dir_with_dumped_data> <output_dir>
```

# License and Copyright

This tool is based on, and contains code from, the following projects:

* [libsmartpen](https://github.com/srwalter/libsmartpen)
* [LibreScribe](https://github.com/dylanmtaylor/LibreScribe)

## License

This work is licensed under GPLv2. For more info see the COPYING file.

## Copyright

This work has had multiple contributors. Not all of them have identified themselves clearly.

* Copyright 2010 to 2011 Steven Walter (https://github.com/srwalter)
* Copyright 2010 Scott Hassan
* Copyright 2011 jhl (?)
* Copyright 2011 Nathanael Noblet (nathanael@gnat.ca)
* Copyright 2011 to 2013 Dylan M. Taylor (webmaster@dylanmtaylor.com)
* Copyright 2013 Yonathan Randolph (yonathan@gmail.com) 
* Copyright 2014 Robert Jordens (https://github.com/jordens)
* Copyright 2014 Ali Neishabouri (ali@neishabouri.net)
* Copyright 2014 Marc Juul (scribedump@juul.io)
