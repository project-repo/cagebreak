#!/bin/bash
# Copyright 2023, project-repo and the cagebreak contributors
# SPDX -License-Identifier: MIT

named_pipe_send="$(mktemp -u)"
named_pipe_recv="$(mktemp -u)"
mkfifo "${named_pipe_send}"
mkfifo "${named_pipe_recv}"
nc -U "${CAGEBREAK_SOCKET}" < "${named_pipe_send}" > "${named_pipe_recv}"&
# The file descriptor 3 is set up to send commands to cagebreak and file
# descriptor 4 can be used to read events. Notice that events will pile up in
# file descriptor 4, so it is a good idea to continuously read from it or to
# clear it before starting a new transaction.
exec 3>"${named_pipe_send}"
exec 4<"${named_pipe_recv}"
# When the script exits, the os will clean up the pipe
rm "${named_pipe_recv}"
rm "${named_pipe_send}"
