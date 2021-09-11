#ifndef _LIBINPUT_H
#define _LIBINPUT_H

#include <stdbool.h>

struct cg_input_device;

void cg_input_configure_libinput_device(struct cg_input_device *device);

void cg_input_reset_libinput_device(struct cg_input_device *device);

bool cg_libinput_device_is_builtin(struct cg_input_device *device);

#endif
