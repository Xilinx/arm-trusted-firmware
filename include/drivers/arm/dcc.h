/*
 * Copyright (c) 2020,  Xilinx Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef DCC_H
#define DCC_H

#include <drivers/console.h>
#include <stdint.h>

/*
 * Initialize a new dcc console instance and register it with the console
 * framework.
 */
int console_dcc_register(void);

#endif /* DCC */
