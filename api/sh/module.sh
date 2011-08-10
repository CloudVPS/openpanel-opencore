# This file is part of the OpenPanel API
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU Lesser General Public License as published by the
# Free Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

#
# Library code for using the opencore shell module API.
#

PATH=/var/openpanel/bin:${PATH}

try_authd() {
  cmd="$1"
  line="$1"
  shift
  while [ ! -z "$1" ]; do
    line="${line} $1"
    shift
  done
  
  echo "$line" >&3
  read reply <&3
  sz=$(echo "$reply" | cut -f2 -d' ')
  reply=`echo "$reply" | cut -c1`
  if [ "$reply" = "+" ]; then
    if [ "$cmd" = "getobject" ]; then
      dd bs=1 count=$sz <&3
    fi
    return 0
  fi
  return 1
}

exiterror() {
  trap - EXIT
  echo "<openpanel.module>"
  echo "  <dict id=\"OpenCORE:Result\">"
  echo "    <integer id=\"error\">1</integer>"
  echo "    <string id=\"message\">$*"
  cat $STDERRFILE
  rm -f $STDERRFILE
  echo "    </string>"
  echo "  </dict>"
  echo "</openpanel.module>"
  exit 1
}

exitok() {
  trap - EXIT
  rm -f $STDERRFILE
  echo "<openpanel.module>"
  echo "  <dict id=\"OpenCORE:Result\">"
  echo "    <integer id=\"error\">0</integer>"
  echo "    <string id=\"message\">OK</string>"
  echo "  </dict>"
  echo "</openpanel.module>"
  echo "quit" >&3
  exit 0
}

exitquiet() {
  trap - EXIT
  rm -rf $STDERRFILE
  exit 0
}

exittrap() {
  if [ $? = 0 ]
  then
    exitok
  else
    exiterror "$REASON"
  fi
}

authd() {
  cmd="$1"
  line="$1"
  shift
  while [ ! -z "$1" ]; do
    line="${line} \"$1\""
    shift
  done
  try_authd ${line} || {
    exiterror "authd error on $cmd"
  }
}

module_create() {
  if [ -z "$CLASSID" ]; then exiterror "Create called without class"; fi
  eval ${CLASSID}.create || \
  	Module.create || \
  	exiterror "Error in ${CLASSID}.create"
}

module_update() {
  if [ -z "$CLASSID" ]; then exiterror "Update called without class"; fi
  eval ${CLASSID}.update || \
  	Module.update || \
  	exiterror "Error in ${CLASSID}.update"
}

module_delete() {
  if [ -z "$CLASSID" ]; then exiterror "Delete called without class"; fi
  eval ${CLASSID}.delete || \
  	Module.delete || \
  	exiterror "Error in ${CLASSID}.delete"
}

module_method() {
  methodname=`coreval OpenCORE:Session method`
  eval ${CLASSID}.${methodname} || \
    ${CLASSID}.dispatch || \
    Module.${methodname} || \
    Module.dispatch || \
    exiterror "Error calling ${CLASSID}.${methodname}"
}

module_listobjects() {
  eval ${CLASSID}.listobjects || \
    Module.listobjects || \
    exiterror "Error calling ${CLASSID}.listobjects"
}

mkalias() {
  echo "$1" | sed -e "s/$2$/$3/"
}

listaliases() {
  mainDomain=`coreval Domain id`
  targetDomain="$OBJECTID"
  coreval --loop Domain Domain:Alias | while read aliasDomain; do
    echo "$targetDomain" | sed -e "s/${mainDomain}$/${aliasDomain}/"
  done
}

implement() {
  MODULENAME=`echo "$1" | sed -e "s/\.module$//"`
  export MODULENAME
  cd /var/openpanel/conf/staging/$MODULENAME
  case "$COMMAND" in
	create)
	  module_create
	  ;;
	delete)
	  module_delete
	  ;;
	update)
	  module_update
	  ;;
	listobjects)
	  module_listobjects
	  ;;
	getconfig)
	  Module.getconfig || exiterror "Getconfig not implemented"
	  ;;
	crypt)
	  Module.crypt || exiterror "Crypt not implemented"
	  ;;
	callmethod)
	  module_method
	  ;;
	*)
	  exiterror "Unknown command"
	  ;;
  esac
  exitok
}

# authd installfile httpd.conf /etc/httpd/conf/openpanel.d
# authd runuserscript mklist pi test-list@openpanel.com owner@openpanel.com

# set up error handling
STDERRFILE=`mktemp -t opencore.shapi.XXXXXX`
trap exittrap EXIT
exec 2> $STDERRFILE
set -eE
