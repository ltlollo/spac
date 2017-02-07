/*
 * Copyright (c) 2017 ltlollo
 * Licensed under the MIT license <LICENSE-MIT or
 * http://opensource.org/licenses/MIT>. This file may not be copied,
 * modified, or distributed except according to those terms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <err.h>
#include "splib.h"

#define RNDSRC "/dev/urandom"

extern char *__progname;

static void usage(void);

int
main(int argc, char *argv[]) {
    if (argc-1 < 4) {
        usage();
        errx(1, "not enough arguments");
    }
    if (strcmp(argv[1], "-s") == 0) {
        int error = 0;
        char* file = argv[2];
        char* end;
        long n = strtol(argv[3], &end, 10);
        if ((size_t)(end-argv[3]) != strlen(argv[3])) {
            errx(1, "N is not a number");
        }
        long m = strtol(argv[4], &end, 10);
        if ((size_t)(end-argv[4]) != strlen(argv[4])) {
            errx(1, "M is not a number");
        }
        if (m > MAXNUM || n > MAXNUM || m < 2 || n < 2) {
            errx(1, "M, N must be in [2, %u]", MAXNUM);
        }
        if ((error = split(n, m, file, RNDSRC)) == -1) {
            warnx("split failed");
        }
        return error;
    } else if (strcmp(argv[1], "-j") == 0) {
        int error = 0;
        int nfnames = argc-3;
        if (nfnames > MAXNUM || nfnames < 2) {
            errx(1, "number of fnames must be in [2, %u]", MAXNUM);
        }
        if ((error = join(argv[2], argv+3, nfnames)) == -1) {
            warnx("join failed");
        }
        return error;
    } else {
        usage();
        errx(1, "option 1 must be {-s|-j}");
    }
}

static void
usage(void) {
    (void)fprintf(stderr, "Usage:\t%s -s in N M"
                  "\n\t%s -j out file1 file2 ... fileN [... fileM]"
                  "\nOptions:"
                  "\n\t-s: split a file in m parts such that n are needed to"
                        " recover"
                  "\n\tin<string>: input file"
                  "\n\tN<u8>: number of files needed"
                  "\n\tM<u8>: number of files total"
                  "\n\t-j: recover from the provided files the original"
                        " one"
                  "\n\tout<string>: output file"
                  "\n\tfile{1...M}<strings>: files used to recover"
                  "\n", __progname, __progname);
}
