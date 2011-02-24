# This file is part of the OpenPanel API
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

import os, string, sys
from OpenPanel import error

try:
	s = os.fdopen(3, "w+")
except Exception:
	print
	print "!!! something failed, most likely no authd socket was provided"
	print
	raise

# TODO: do something smart with *cmd or **cmd ?
def do(*cmd):
	escaped = ""
	for c in cmd:
		if len(escaped): escaped += " "
		escaped += escape(c)

	# print "cmd:", escaped
	escaped += "\n"
	s.write(escaped)
	s.flush()
	result = s.readline()

	if result[0] == "+":
		return (True, error.ERR_OK, "")
	else:
	    # we should raise
		return (False, result.split(":")[1], result.split(":")[2])

def escape(str):
	# TODO: sanitycheck
	r = string.replace(str, "\\", "\\\\")
	r = string.replace(r, "\"", "\\\"")
	if r != str:
		r = '"' + r + '"'
	return r

def getobject(obj):
	r = do(["getobject", obj])
	# parse r, unclear for now
