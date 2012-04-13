#!/bin/bash

HOME_SCHEMA="org.dawati.shell.home"
SCHEMA="org.dawati.shell.home.plugin"
PATH_PREFIX="/org/dawati/shell/home/plugin/"

PLUGIN_NAME="$1"
MYPATH="${PATH_PREFIX}${PLUGIN_NAME}/"
COL="$2"
ROW="$3"

function create()
{
	PLUGIN_NAME=$1
	COLUMN=$2
	ROW=$3

	echo 
	echo gsettings "set" ${SCHEMA}:${PATH_PREFIX}${PLUGIN_NAME}/ "module" "example"
	gsettings "set" ${SCHEMA}:${PATH_PREFIX}${PLUGIN_NAME}/ "module" "example"
	gsettings "set" ${SCHEMA}:${PATH_PREFIX}${PLUGIN_NAME}/ "column" "${COLUMN}"
	gsettings "set" ${SCHEMA}:${PATH_PREFIX}${PLUGIN_NAME}/ "row" "${ROW}"

}
function append_to_list()
{
	ITEM="$1"
	OLD_LIST=$(/usr/bin/gsettings get "${HOME_SCHEMA}" "widgets-list")
	# don't append anything if already present
	if grep -q $ITEM <<< $OLD_LIST; then
		return
	fi

	NEW_LIST=$(python -c "x=${OLD_LIST};x.append('$ITEM');print x")
	echo $NEW_LIST
	gsettings set "${HOME_SCHEMA}" "widgets-list" "${NEW_LIST}"
}


create $PLUGIN_NAME $COL $ROW
echo Created plugin: $PLUGIN_NAME at coords $COL,$ROW using Example.

append_to_list $MYPATH
