#!/bin/bash

HOME_SCHEMA=org.dawati.shell.home
SCHEMA=org.dawati.shell.home.plugin
PATH_PREFIX=/org/dawati/shell/home/plugin/

function get_widget_list()
{
	LIST="`gsettings get ${HOME_SCHEMA} widgets-list`"
	# GSetttings lists are in python-like list syntax, so let's parse with it
	# return the last item of the path
	python -c "for x in ${LIST}: print x.strip('/').split('/')[-1]"
}

function get_key()
{
	PLUGIN_NAME=$1
	KEY=$2

	gsettings get ${SCHEMA}:${PATH_PREFIX}${PLUGIN_NAME}/ ${KEY}
}



if [ $# -eq 0 ];
then
	requested_plugins=`get_widget_list`
	echo No arguments, showing plugins in widgets-list key:
else
	requested_plugins=$*
fi

for plugin_name in $requested_plugins; do
	echo Plugin: $plugin_name
	for key in column row module type; do
		echo -en "\t$key: "
		get_key $plugin_name $key
	done
done
