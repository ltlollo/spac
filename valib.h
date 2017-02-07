/*
 * Copyright (c) 2017 ltlollo
 * Licensed under the MIT license <LICENSE-MIT or
 * http://opensource.org/licenses/MIT>. This file may not be copied,
 * modified, or distributed except according to those terms.
 */

#ifndef VALIB_H
#define VALIB_H

#include "splib.h"

#define TSFX ".trust"
#define USFX ".utrust"

#ifdef __cplusplus
extern "C" {
#endif

int validate(const char *, char *const[], int);

#ifdef __cplusplus
}
#endif

#endif // VALIB_H
