/*
 * Copyright (c) 2017 ltlollo
 * Licensed under the MIT license <LICENSE-MIT or
 * http://opensource.org/licenses/MIT>. This file may not be copied,
 * modified, or distributed except according to those terms.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>

#include "valib.h"
#include "ebutil.h"

#define UNLIKELY(x) __builtin_expect((x), 0)

typedef struct {
    Info comm;
    FILE *mfile;
    FILE *xfile;
    char *nschema;
    char namebuf[FLEN];
    char tbuf[BUFSIZE];
    char ubuf[BUFSIZE];
} Data;

typedef enum {
    EQ    =  0,
    DIFF  =  1,
    IOERR = -1,
    FGT   = -2,
    SGT   = -3
} Res;

static Res  equals(Data *);
static void cleanup(Data *, int, int, const char *, size_t);
static int  populate_info(Data *, const char *);
static int  create_phold(const char *);

int
validate(const char *file, char *const fnames[], int size) {
    if (size < 3) {
        return -1;
    }
    size_t len;
    size_t nslen;
    Data d = {
        .mfile   = NULL,
        .xfile   = NULL,
        .nschema = d.namebuf,
    };
    int efatal = 0, ncreat = 0;
    if ((len = strlen(file)) == 0) {
        warnx("empty fileput filename");
        return -1;
    }
    if (populate_info(&d, fnames[0]) == -1) {
        warn("%s", fnames[0]);
        return -1;
    }
    if (d.comm.n == MAXNUM || d.comm.n == d.comm.m) {
        warnx("cannot be cross-verified, all files are necessary");
        return -1;
    }
    u8 required = d.comm.n+1;
    if (size < required) {
        warnx("not enough files provided: got %d, need %d", size, required);
        return -1;
    }
    if (create_phold(file) == -1) {
        warn("%s", file);
        return -1;
    }
    nslen =  len  +  1   + 1;
    //format:name + [mx] + '\0'
    if (nslen > FLEN) {
        if ((d.nschema = (char *)malloc(nslen*sizeof(char))) == NULL) {
            warn(NULL);
            (void)unlink(file);
            return -1;
        }
    }
    strncpy(d.nschema, file, len);
    d.nschema[len+1] = '\0';
    d.nschema[len] = 'm';
    if ((efatal = join(d.nschema, fnames, d.comm.n)) == -1) {
        warnx("failed to join main list");
        goto CLEANUP;
    }
    ++ncreat;
    d.nschema[len] = 'x';
    if ((efatal = join(d.nschema, fnames+1, d.comm.n)) == -1) {
        warnx("failed to join cross list");
        goto CLEANUP;
    }
    ++ncreat;
    d.nschema[len] = 'm';
    if ((d.mfile = fopen(d.nschema, "r")) == NULL) {
        warn("%s", d.nschema);
        efatal = -1;
        goto CLEANUP;
    }
    d.nschema[len] = 'x';
    if ((d.xfile = fopen(d.nschema, "r")) == NULL) {
        warn("%s", d.nschema);
        efatal = -1;
        goto CLEANUP;
    }
    Res res;
    if ((res = equals(&d)) != 0) {
        switch(res) {
        case IOERR: warnx("io error");
        case FGT:   warnx("sizes do not match: trusted file bigger");
        case SGT:   warnx("sizes do not match: untrusted file bigger");
        default:    warnx("files differ");
        }
        efatal = -1;
        goto CLEANUP;
    }
    d.nschema[len] = 'm';
    if ((efatal = rename(d.nschema, file)) == -1) {
        warn(NULL);
    }
CLEANUP:
    cleanup(&d, efatal, ncreat, file, len);
    return efatal;
}

static int
populate_info(Data *d, const char *file) {
    FILE *one;
    if ((one = fopen(file, "r")) == NULL) {
        return -1;
    }
    size_t read = fread(&d->comm, 1, sizeof(Info), one);
    d->comm.size = htole64(d->comm.size);
    (void)fclose(one);
    if (read != sizeof(Info)) {
        return -1;
    }
    return 0;
}

static int
create_phold(const char *file) {
    FILE* out;
    if ((out = fopen(file, "w+x")) == NULL) {
        return -1;
    } else {
        (void)fclose(out);
    }
    return 0;
}

static Res
equals(Data *d) {
    size_t nc = 0;
    while ((nc = fread(d->tbuf, 1, BUFSIZE, d->mfile))) {
        if (UNLIKELY(fread(d->ubuf, 1, nc, d->xfile) != nc)) {
            return IOERR;
        }
        if (UNLIKELY(nc != BUFSIZE)) {
            for (unsigned i = 0; i < nc; ++i) {
                if (UNLIKELY(d->tbuf[i] != d->ubuf[i])) {
                    return DIFF;
                }
            }
            continue;
        }
        for (unsigned i = 0; i < BUFSIZE; ++i) {
            if (UNLIKELY(d->tbuf[i] != d->ubuf[i])) {
                return DIFF;
            }
        }
    }
    int ef = feof(d->mfile), es = 1;
    if (fread(d->ubuf, 1, 1, d->xfile) != 0) {
        es = 0;
    } else if (!feof(d->xfile)) {
        es = 0;
    }
    if (ef && es) {
        return EQ;
    } else if (ef) {
        return SGT;
    } else if (es) {
        return FGT;
    } else {
        return IOERR;
    }
}

static void
cleanup(Data *d, int efatal, int ncreat, const char *file, size_t len) {
    if (efatal) {
        (void)unlink(file);
    }
    d->nschema[len] = 'm';
    if (efatal && ncreat) {
        (void)unlink(d->nschema);
    }
    d->nschema[len] = 'x';
    if (ncreat == 2) {
        (void)unlink(d->nschema);
    }
    if (d->mfile != NULL) {
        (void)fclose(d->mfile);
    }
    if (d->xfile != NULL) {
        (void)fclose(d->xfile);
    }
    if (d->nschema != d->namebuf) {
        free(d->nschema);
    }
}
