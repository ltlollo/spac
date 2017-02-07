/*
 * Copyright (c) 2017 ltlollo
 * Licensed under the MIT license <LICENSE-MIT or
 * http://opensource.org/licenses/MIT>. This file may not be copied,
 * modified, or distributed except according to those terms.
 */

#include <stdio.h>
#include <err.h>
#include "valib.h"

extern char *__progname;

static void usage(void);

int
main(int argc, char *argv[]) {
    if (argc-1 < 4) {
        usage();
        errx(1, "not enough arguments");
    }
    if (validate(argv[1], argv+2, argc-2) == -1) {
        errx(1, "validate");
    }
    return 0;
}

static void
usage(void) {
    (void)fprintf(stderr, "Usage:\t%s out file1 file2 file3 [... fileN]"
                  "\n\tout<string>: output file"
                  "\n\tfile1<string>: spac file to test"
                  "\n\tfile{2..N}<strings>: spac files"
                  "\nScope: test if the files recovered with and without "
                        "file1 are the same, given file2...fileN"
                  "\n", __progname);
}

