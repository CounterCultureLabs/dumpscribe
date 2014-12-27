#!/usr/bin/env python

import sys
import os
import re
import string
import subprocess

# percent usage to stay under
# clean will delete files until usage is under this percentage
keep_usage_under = 50

def usage():
    sys.stderr.write("\n")
    sys.stderr.write("Usage: %s path_to_clean\n")
    sys.stderr.write("\n")
    sys.stderr.write("  Removes files from path_to_clean starting with the oldest\n")
    sys.stderr.write("  files and continuing until the filesystem where path_to_clean\n")
    sys.stderr.write("  is mounted is less than 50% full or no more files remain.\n")
    sys.stderr.write("\n")

def mtime_cmp(path1, path2):
    mtime1 = os.stat(path1).st_mtime
    mtime2 = os.stat(path2).st_mtime
    if mtime1 > mtime2:
        return 1
    elif mtime2 > mtime1:
        return -1
    else:
        return 0

# recursively walk directory structure
# and return list of all files ordered by date
def file_list(clean_path):
    all_files = []
    for root, dirs, files in os.walk(clean_path):
        for f in files:
            path = os.path.join(root, f)
            all_files.append(path)

    all_files.sort(cmp=mtime_cmp)        
    return all_files

            


# get filesystem usage in percent (as integer)
def filesystem_usage(path):
    df_cmd =  'df "%s"' % (path)
    output = subprocess.check_output(df_cmd, shell=True)
    lines = re.split("\r?\n", output)
    # find index of "%" in the header
    u_index = string.find(lines[0], "%")
    # find the percentage of the first item based on index of "%" in header
    usage = lines[1][(u_index - 3):u_index]
    # conver from string to integer
    usage = string.atoi(usage)
    return usage

# delete until usage on device is less than the specified percentage
# root_path is some path on same device as files 
# files is a list of paths to files to be cleaned/deleted
#   until usage is less than the less_than_usage (in percent)
def delete_until_usage_less_than(root_path, files, less_than_usage=50):
    num_deleted = 0
    for file in files:
        os.unlink(file)
        num_deleted += 1
        try: # delete dir if empty
            os.rmdir(os.path.dirname(file))
        except:
            num_deleted = num_deleted # TODO use correct python nop
        if filesystem_usage(root_path) < less_than_usage:
            return num_deleted
    return num_deleted


if len(sys.argv) != 2:
    usage()
    sys.exit(1)

clean_path = sys.argv[1]

usage = filesystem_usage(clean_path)
if usage < keep_usage_under:
    print "Usage was already under specified limit. Clean did nothing :)"
    sys.exit(0)

files = file_list(clean_path)
num_deleted = delete_until_usage_less_than(clean_path, files, keep_usage_under)
usage = filesystem_usage(clean_path)
print "Cleanup deleted %s files. Usage is now %s percent" % (num_deleted, usage)


