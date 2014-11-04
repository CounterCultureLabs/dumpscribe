
Downloads all written notes and recorded audio from a LiveScribe Pulse or Echo pen.

Functionality is currently limited: The raw data is downloaded and extracted, with no conversion to sane formats or association of data with time of recording/writing.

# TODO

* Output sane directory structure (with time and date)
* Add STF to PDF conversion
* Add command line arguments

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
