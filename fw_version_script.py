Import("env")

from itertools import cycle
import shutil
import re
import os
from base64 import b64decode

def format_int(i, sep='.', l=0):
	cyc = cycle(['', '', sep])
	s = str(i).zfill(l)
	last = len(s) - 1
	formatted = [(cyc.next() if idx != last else '') + char
		for idx, char in enumerate(reversed(s))]
	return ''.join(reversed(formatted))

def read_int(str, sep=','):
	i = 0
	for s in re.split(',| ', str):
		if s.isdigit():
			i = i*1000 + int(s)
	return int(i)


def read_fw_version_from_header_file(filename='fw_version_def.h'):
	if os.path.isfile(filename):
		try:
			fw_version = 0
			regex = re.compile(r"^.*#define\s+FW_VERSION\s+([0-9, \t]+)(.*)$", re.IGNORECASE)
			with open(filename) as f:
				for line in f:
					fw_version_match = regex.match(line)
					if fw_version_match:
						fw_version = read_int(fw_version_match.group(1))
		except:
			fw_version = -1
	else:
		fw_version = -1
	return fw_version

def replace_fw_version_to_header_file(fw_version=0, filename='fw_version_def.h'):
	filename_new = filename + '.new'
	fw_version = format_int(fw_version,',',8)
	done = 0
	with open(filename_new,'w') as fn:
		if os.path.isfile(filename):
			filename_bak = filename + '.bak'
			shutil.copyfile(filename, filename_bak)
			regex = re.compile(r"^.*#define\s+FW_VERSION\s+([0-9, \t]+)(.*)$", re.IGNORECASE)
			with open(filename) as f:
				for line in f:
					fw_version_match = regex.match(line)
					if fw_version_match:
						rest = fw_version_match.group(2)
						fn.write("#define FW_VERSION %s %s\n" % (fw_version, rest))
						done = 1
					else:
						fn.write(line)
		if not done:
			fn.write("#define FW_VERSION %s\n" % fw_version)
			done = 1
	shutil.move(filename_new, filename)

def write_fw_version_to_fwv_file(fw_version=0, filename='firmware.fwv'):
	fw_version = format_int(fw_version,',',8)
	with open(filename,'w') as f:
		f.write(fw_version)

def update_fw_version(source, target, env):
	custom_option = b64decode(ARGUMENTS.get("CUSTOM_OPTION"))
	re_match = re.match(r"(?i)^.*FW_VERSION_H_FILENAME\s*=\s*'([^'^$]*)'.*$",custom_option)
	if re_match:
		h_filename=re_match.group(1)
	else:
		h_filename="fw_version_def.h"
	re_match = re.match(r"(?i)^.*FW_VERSION_PUBLISH_LOCATION\s*=\s*'([^'^$]*)'.*$",custom_option)
	if re_match:
		publish_location=re_match.group(1)
	else:
		publish_location=""
	fwv_filename_bin = str(target[0])
	fwv_filename_fwv = fwv_filename_bin.replace("bin","fwv")
	fw_version = read_fw_version_from_header_file(h_filename)
	write_fw_version_to_fwv_file(fw_version, fwv_filename_fwv)
	print "Generated fwv file for FW_VERSION %s -> %s" % (format_int(fw_version,',',8), fwv_filename_bin)
	fw_version_new = fw_version+1
	replace_fw_version_to_header_file(fw_version_new, h_filename)
	print "FW_VERSION for next build increased %s -> %s" % (format_int(fw_version,',',8), format_int(fw_version_new,',',8))

print "Current build targets", map(str, BUILD_TARGETS)
env.AddPostAction("$BUILD_DIR/firmware.bin", update_fw_version)
