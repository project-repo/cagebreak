#!/bin/bash

# This script displays the process names of the views on the current workspace.

# Start up the ipc link
source "$(dirname "${BASH_SOURCE[0]}")/cb_script_header.sh"

echo "dump" >&3

while IFS= read -r -d $'\0' event
do
	# Remove the cg-ipc header
	event="${event:6}"
	if [[ "$(echo "${event}" | jq ".event_name")" = "\"dump\"" ]]
	then
		curr_output="$(echo "${event}"|jq ".curr_output")"
		curr_workspace="$(echo "${event}"|jq -r ".outputs.${curr_output}.curr_workspace")"
		# Print the process names of the view on the current workspace. jq retrieves
		# their PID and ps is then used to retrieve the process names.
		(echo -n "message ";ps -o comm=Command -p $(echo "${event}"|jq -r ".outputs.${curr_output}.workspaces[$((curr_workspace-1))].views[].pid")|tail +2|sed ':a; N; $!ba; s/\n/||/g') >&3
		break
	fi
done <&4
