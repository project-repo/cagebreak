#!/bin/bash
# Copyright 2023, project-repo and the cagebreak contributors
# SPDX -License-Identifier: MIT

# This script uses the cagebreak socket to modify the compositor's behaviour in
# such a way that the currently focussed tile follows the cursor, i.e. that the
# currently focussed tile is always the one within which the cursor is located.

# Start up the ipc link
source "$(dirname "${BASH_SOURCE[0]}")/cb_script_header.sh"

while IFS= read -r -d $'\0' event
do
	# Remove the cg-ipc header
	event="${event:6}"
	if [[ "$(echo "${event}" | jq ".event_name")" = "\"cursor_switch_tile\"" ]]
	then
		new_tile="$(echo "${event}"|jq -r ".new_tile")"
		new_output_id="$(echo "${event}"|jq -r ".new_output_id")"
		# Send the new tile to focus to cagebreak
		echo -e "screen ${new_output_id}\nfocus_tile ${new_tile}" >&3
	fi
done <&4
