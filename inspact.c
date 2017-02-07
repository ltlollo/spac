/*
 * Copyright (c) 2017 ltlollo
 * Licensed under the MIT license <LICENSE-MIT or
 * http://opensource.org/licenses/MIT>. This file may not be copied,
 * modified, or distributed except according to those terms.
 */

#include <stdio.h>
#include <err.h>

#include "splib.h"
#include "ebutil.h"

extern char *__progname;

static void usage(void);

int
main(int argc, char *argv[]) {
    if (argc-1 < 1) {
        usage();
        errx(1, "not enough arguments");
    }
    Info spi;
    FILE *file;
    for (int i = 1; i < argc; ++i) {
        if ((file = fopen(argv[i], "r")) == NULL) {
            warn("%s could not be inspected", argv[i]);
            continue;
        }
        if (fread(&spi, 1, sizeof(Info), file) == sizeof(Info)) {
            spi.size = htole64(spi.size);
            (void)printf("fsig: 0x%.2X, ele: %u, n: %u, m: %u, size: %lu; %s\n"
                         , spi.fsig, spi.ele, spi.n, spi.m, spi.size, argv[i]);
            if (spi.fsig != FSIG) {
                (void)fprintf(stderr, "%s: wrong file signature\n", argv[i]);
            }
            if (spi.ele > spi.m) {
                (void)fprintf(stderr, "%s: wrong ele number\n", argv[i]);
            }
            if (spi.n > spi.m || spi.n < 2) {
                (void)fprintf(stderr, "%s: wrong n number\n", argv[i]);
            }
            if (spi.m < 2) {
                (void)fprintf(stderr, "%s: wrong m number\n", argv[i]);
            }
        } else {
            warn("could read from %s", argv[i]);
        }
        (void)fclose(file);
    }
    return 0;
}

static void
usage(void) {
    (void)fprintf(stderr, "Usage\t%s file [...files]"
                  "\n\tfile<string>: first input file"
                  "\n\tfils<strings>: additional input files"
                  "\nScope: show spac file infos of the arguments"
                  "\n", __progname);
}
