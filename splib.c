/*
 * Copyright (c) 2017 ltlollo
 * Licensed under the MIT license <LICENSE-MIT or
 * http://opensource.org/licenses/MIT>. This file may not be copied,
 * modified, or distributed except according to those terms.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ebutil.h"
#include "splib.h"

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define CXLEN(sarr) (sizeof((sarr)) - sizeof((sarr[0])))

typedef struct {
    Info comm;
    FILE *rnd;
    FILE *input;
    u8 obuf[BUFSIZE];
    u8 rbuf[BUFSIZE];
    u8 combbuf[MAXNUM];
    u8 filledtab[MAXNUM];
    FILE *files[MAXNUM];
    u8 filenum[MAXNUM];
} Data;

typedef struct {
    char *str;
    char buf[FLEN];
} StrBuf;

static u8   stable_min_pos(u8 *restrict, u8 *restrict, u8);
static u8   ndigits(u8 of);
static void xorv(u8 *restrict, u8 *restrict);
static void bsort(FILE *restrict[], u8 *restrict, u8);
static void gather_combfiles_front(FILE *restrict[], u8 *restrict,
                                   u8 *restrict, u8, u8);

static int raw_split(Data *);
static int raw_mm_split(Data *);
static int raw_nm_split(Data *);
static int raw_writedep(Data *, u8);

static int raw_join(Data *);
static int raw_mm_join(Data *);
static int raw_nm_join(Data *);
static int raw_writefile(Data *, u8);

static void populate_nschema(char *, size_t, u8, u8);
static void combbuf_init(u8 *, u8);
static u8   combbuf_next(u8 *, u8, u8);
static u8   combbuf_match(u8 *, u8 *, u8, u8);

int
splitvp(u8 n, char const* paths[], u8 m, const char *file,
        const char *rndsrc) {
    if (m < n || n < 2 || m < 2) {
        warnx("M must be greater than N, and they must be greater than 1");
        return -1;
    }
    size_t len;
    size_t nslen;
    if ((len = strlen(file)) == 0) {
        warnx("filename cannot be empty");
        return -1;
    }
    Data d = {
        .rnd        = NULL,
        .input      = NULL,
        .comm       = {
            .fsig   = FSIG,
            .n      = n,
            .m      = m
        },
        .combbuf    = {0},
        .filledtab  = {0},
        .files      = {NULL},
    };
    StrBuf nschema = {
        .str = nschema.buf,
    };
    u8 nd = ndigits(m);
    nslen = len  +1  +nd     +CXLEN(EXT)+  1;
    //format:name+'.'+[0-255]+'.spl'    +'\0'
    if (paths == NULL) {
        if (nslen > FLEN) {
            if ((nschema.str = (char *)malloc(nslen*sizeof(char))) == NULL) {
                warn(NULL);
                return -1;
            }
        }
        strncpy(nschema.str, file, len);
        nschema.str[len] = '.';
        strcpy(nschema.str + len + 1 + nd, EXT);
    }
    int efatal = 0;
    u8 ifile = 0;
    if ((d.input = fopen(file, "r")) == NULL) {
        warn("%s", file);
        efatal = -1;
        goto CLEANUP;
    }
    if ((d.rnd = fopen(rndsrc, "r")) == NULL) {
        warn("%s", file);
        efatal = -1;
        goto CLEANUP;
    }
    if (fseek(d.input, 0, SEEK_END) == -1) {
        warn("%s", file);
        efatal = -1;
        goto CLEANUP;
    }
    long fsize;
    if ((fsize = ftell(d.input)) == -1) {
        warn("%s", file);
        efatal = -1;
        goto CLEANUP;
    }
    d.comm.size = fsize;
    if (paths == NULL) {
        for (; ifile < m; ++ifile) {
            populate_nschema(nschema.str, len, nd, ifile);
            if ((d.files[ifile] = fopen(nschema.str, "w+x")) == NULL) {
                warn("%s", nschema.str);
                efatal = -1;
                goto CLEANUP;
            }
        }
    } else {
        for (; ifile < m; ++ifile) {
            if ((d.files[ifile] = fopen(paths[ifile], "w+x")) == NULL) {
                warn("%s", paths[ifile]);
                efatal = -1;
                goto CLEANUP;
            }
        }
    }
    if ((efatal = raw_split(&d)) != 0) {
        warn("raw_split");
    }
CLEANUP:
    if (paths == NULL) {
        for (u8 i = 0; i < ifile; ++i) {
            (void)fclose(d.files[i]);
            if (efatal) {
                populate_nschema(nschema.str, len, nd, i);
                warnx("deleting %s", nschema.str);
                (void)unlink(nschema.str);
            }
        }
    } else {
        for (u8 i = 0; i < ifile; ++i) {
            (void)fclose(d.files[i]);
            if (efatal) {
                warnx("deleting %s", paths[i]);
                (void)unlink(paths[i]);
            }
        }
    }
    if (nschema.str != nschema.buf) {
        free(nschema.str);
    }
    if (d.input) {
        (void)fclose(d.input);
    }
    if (d.rnd) {
        (void)fclose(d.rnd);
    }
    return efatal;
}

int
split(u8 n, u8 m, const char *file, const char *rndsrc) {
    return splitvp(n, NULL, m, file, rndsrc);
}

static int
raw_split(Data *d) {
    Info comm = {
        .fsig = d->comm.fsig,
        .ele  = d->comm.ele,
        .n    = d->comm.n,
        .m    = d->comm.m,
        .size = htole64(d->comm.size)
    };
    for (u8 i = 0; i < comm.m; ++i) {
        comm.ele = i;
        if (fwrite(&comm, 1, sizeof(Info), d->files[i])
            != sizeof(Info)) {
            return -1;
        }
    }
    return (d->comm.n == d->comm.m) ? raw_mm_split(d) : raw_nm_split(d);
}

static int
raw_mm_split(Data *d) {
    if (fseek(d->input, 0, SEEK_SET) == -1) {
        return -1;
    }
    size_t nc = 0;
    while ((nc = fread(d->obuf, 1, BUFSIZE, d->input))) {
        for (u8 i = 0; i < d->comm.n-1; ++i) {
            if (fread(d->rbuf, 1, nc, d->rnd) != nc) {
                return -1;
            }
            if (fwrite(d->rbuf, 1, nc, d->files[i]) != nc) {
                return -1;
            }
            xorv(d->obuf, d->rbuf);
        }
        if (fwrite(d->obuf, 1, nc, d->files[d->comm.n-1]) != nc) {
            return -1;
        }
    }
    return (feof(d->input) == 1) ? 0 : -1;
}

static int
raw_nm_split(Data *d) {
    combbuf_init(d->combbuf, d->comm.n);
    size_t chunksize;
    do {
        chunksize = (d->comm.size < BUFSIZE) ? d->comm.size : BUFSIZE;
        for (u8 i = 0; i < d->comm.m; ++i) {
            if (fread(d->rbuf, 1, chunksize, d->rnd) != chunksize) {
                return -1;
            }
            if (fwrite(d->rbuf, 1, chunksize, d->files[i]) != chunksize) {
                return -1;
            }
        }
    } while ((d->comm.size -= chunksize));
    if (raw_writedep(d, 0) == -1) {
        return -1;
    }
    ++d->filledtab[0];
    while (combbuf_next(d->combbuf, d->comm.n, d->comm.m)) {
        u8 min_pos = stable_min_pos(d->filledtab, d->combbuf, d->comm.n);
        if (raw_writedep(d, min_pos) == -1) {
            return -1;
        }
        ++d->filledtab[d->combbuf[min_pos]];
    }
    return 0;
}

static u8
stable_min_pos(u8 *restrict filledtab, u8 *restrict combbuf, u8 n) {
    u8 pos = 0;
    u8 min = filledtab[combbuf[0]];
    for (u8 i = 1; i < n; ++i) {
        if (UNLIKELY(filledtab[combbuf[i]] < min)) {
            pos = i;
            min = filledtab[combbuf[i]];
        }
    }
    return pos;
}

static int
raw_writedep(Data *d, u8 min_pos) {
    for (u8 i = 0; i < d->comm.n; ++i) {
        if (UNLIKELY(i == min_pos)) {
            continue;
        }
        if (fseek(d->files[d->combbuf[i]], sizeof(Info), SEEK_SET) == -1) {
            return -1;
        }
    }
    if (fseek(d->input, 0, SEEK_SET) == -1) {
        return -1;
    }
    size_t nc;
    while ((nc = fread(d->obuf, 1, BUFSIZE, d->input))) {
        for (u8 i = 0; i < d->comm.n; ++i) {
            if (UNLIKELY(i == min_pos)) {
                continue;
            }
            if (fread(d->rbuf, 1, nc, d->files[d->combbuf[i]]) != nc) {
                warnx("WHAT: pos %u s", i);
                return -1;
            }
            xorv(d->obuf, d->rbuf);
        }
        if (fwrite(d->obuf, 1, nc, d->files[d->combbuf[min_pos]]) != nc) {
            return -1;
        }
    }
    for (u8 i = 0; i < d->comm.n; ++i) {
        if (UNLIKELY(i == min_pos)) {
            continue;
        }
        if (fseek(d->files[d->combbuf[i]], 0, SEEK_END) == -1) {
            return -1;
        }
    }
    return (feof(d->input) == 1) ? 0 : -1;
}

int
join(const char *out, char *const fnames[], u8 size) {
    if (size < 2) {
        warnx("must pass n in [2, %u] fnames", MAXNUM);
        return -1;
    }
    Data d = {
        .rnd        = NULL,
        .input      = NULL,
        .combbuf    = {0},
        .filledtab  = {0},
        .files      = {NULL}
    };
    if ((d.input = fopen(out, "w+x")) == NULL) {
        warn("%s", out);
        return -1;
    }
    int efatal = 0;
    u8 ifile = 0;
    for (; ifile < size; ++ifile) {
        if ((d.files[ifile] = fopen(fnames[ifile], "r")) == NULL) {
            efatal = -1;
            warn("%s", fnames[ifile]);
            goto CLEANUP;
        }
    }
    if (fread(&d.comm, 1, sizeof(Info), d.files[0]) != sizeof(Info)) {
        warn(NULL);
        efatal = -1;
        goto CLEANUP;
    }
    d.comm.size = le64toh(d.comm.size);
    if (d.comm.n > size) {
        warnx("not enough files provided");
        efatal = -1;
        goto CLEANUP;
    }
    if (d.comm.m < size) {
        warnx("too many files provided");
        efatal = -1;
        goto CLEANUP;
    }
    if ((d.filenum[0] = d.comm.ele) > d.comm.m) {
        warnx("element number out of range in %s", fnames[0]);
        efatal = -1;
        goto CLEANUP;
    }
    Info tmp;
    for (u8 i = 1; i < size; ++i) {
        if (fread(&tmp, 1, sizeof(Info), d.files[i]) != sizeof(Info)) {
            warn(NULL);
            efatal = -1;
            goto CLEANUP;
        }
        tmp.size = le64toh(tmp.size);
        if (tmp.fsig != d.comm.fsig || tmp.n    != d.comm.n    ||
            tmp.m    != d.comm.m    || tmp.size != d.comm.size) {
            warnx("incorrect file prelude, %s, %s preludes differ",
                  fnames[0], fnames[i]);
            efatal = -1;
            goto CLEANUP;
        }
        if ((d.filenum[i] = tmp.ele) > d.comm.m) {
            warnx("element number out of range in %s", fnames[i]);
            efatal = -1;
            goto CLEANUP;
        }
    }
    d.comm.ele = size;
    bsort(d.files, d.filenum, d.comm.ele);
    for (u8 i = 0; i < d.comm.ele-1; ++i) {
        if (d.filenum[i] == d.filenum[i+1]) {
            warnx("found duplicate input file");
            efatal = -1;
            goto CLEANUP;
        }
    }
    if ((efatal = raw_join(&d)) != 0) {
        warn("raw_join");
    }
CLEANUP:
    (void)fclose(d.input);
    if (efatal) {
        (void)unlink(out);
    }
    for (u8 i = 0; i < ifile; ++i) {
        (void)fclose(d.files[i]);
    }
    return efatal;
}

static int
raw_join(Data *d) {
    return (d->comm.n == d->comm.m) ? raw_mm_join(d) : raw_nm_join(d);
}

static int
raw_mm_join(Data *d) {
    size_t nc;
    while ((nc = fread(d->obuf, 1, BUFSIZE, d->files[0]))) {
        for (u8 i = 1; i < d->comm.n; ++i) {
            if (fread(d->rbuf, 1, nc, d->files[i]) != nc) {
                return -1;
            }
            xorv(d->obuf, d->rbuf);
        }
        if (fwrite(d->obuf, 1, nc, d->input) != nc) {
            return -1;
        }
    }
    return 0;
}

static int
raw_nm_join(Data *d) {
    combbuf_init(d->combbuf, d->comm.n);
    if (combbuf_match(d->combbuf, d->filenum, d->comm.n, d->comm.m)) {
        return raw_writefile(d, 0);
    }
    ++d->filledtab[0];
    while (combbuf_next(d->combbuf, d->comm.n, d->comm.m)) {
        u8 min_pos = stable_min_pos(d->filledtab, d->combbuf, d->comm.n);
        if (UNLIKELY(combbuf_match(d->combbuf, d->filenum, d->comm.n,
                                   d->comm.m))) {
            return raw_writefile(d, min_pos);
        }
        ++d->filledtab[d->combbuf[min_pos]];
    }
    return -1;
}

static int
raw_writefile(Data *d, u8 min_pos) {
    gather_combfiles_front(d->files, d->filenum, d->combbuf, d->comm.ele,
                           min_pos);
    if (fseek(d->files[0], sizeof(Info) +
              d->comm.size * (1 + d->filledtab[d->combbuf[min_pos]]),
              SEEK_SET) == -1) {
        return -1;
    }
    while (d->comm.size) {
        size_t chunksize = d->comm.size < BUFSIZE ? d->comm.size : BUFSIZE;
        if (fread(d->obuf, 1, chunksize, d->files[0]) != chunksize) {
            return -1;
        }
        for (u8 i = 1; i < d->comm.n; ++i) {
            if (fread(d->rbuf, 1, chunksize, d->files[i]) != chunksize) {
                return -1;
            }
            xorv(d->obuf, d->rbuf);
        }
        if (fwrite(d->obuf, 1, chunksize, d->input) != chunksize) {
            return -1;
        }
        d->comm.size -= chunksize;
    }
    return 0;
}

static void
bsort(FILE *restrict files[], u8 *restrict filenum, u8 ele) {
    u8 tmpf;
    FILE* tmps;
    for (u8 len = ele, n; len; len = n) {
        n = 0;
        for (u8 i = 1; i < len; ++i) {
            if (UNLIKELY(filenum[i-1] > filenum[i])) {
                tmpf = filenum[i];
                tmps = files[i];
                filenum[i] = filenum[i-1];
                filenum[i-1] = tmpf;
                files[i] = files[i-1];
                files[i-1] = tmps;
                n = i;
            }
        }
    }
}

static inline void
combbuf_init(u8 *combbuf, u8 n) {
    for (u8 i = 0; i < n; ++i) {
        combbuf[i] = i;
    }
}

static inline u8
combbuf_next(u8 *combbuf, u8 n, u8 m) {
    u8 i = n-1;
    ++combbuf[i];
    while (i && (combbuf[i] >= m - n+i+1)) {
        ++combbuf[--i];
    }
    if (UNLIKELY(combbuf[0] > m - n)) {
        return 0;
    }
    while (i++ < n) {
        combbuf[i] = combbuf[i-1]+1;
    }
    return 1;
}

static inline u8
combbuf_match(u8 *restrict combbuf, u8 *restrict filenum, u8 n, u8 ele) {
    u8  nmatch = 0;
    for (u8 i = 0; i < ele; ++i) {
        if (UNLIKELY(nmatch == n)) {
            break;
        }
        if (combbuf[nmatch] == filenum[i]) {
            ++nmatch;
        }
    }
    return (nmatch == n);
}

static inline void
populate_nschema(char *nschema, size_t len, u8 nd, u8 of) {
    for (size_t in = len + nd; in != len; --in, of /= 10) {
        nschema[in] = '0' + of%10;
    }
}

static inline u8
ndigits(u8 of) {
    if (of > 99) {
        return 3;
    } else if (of > 9) {
        return 2;
    } else if (LIKELY(of)) {
        return 1;
    } else {
        return 0;
    }
}

static inline void
xorv(u8 *restrict obuf, u8 *restrict rbuf) {
    for (unsigned i = 0; i < BUFSIZE; ++i) {
        obuf[i] ^= rbuf[i];
    }
}

static void
gather_combfiles_front(FILE *restrict files[], u8 *restrict filenum,
                       u8 *restrict combbuf, u8 ele, u8 min_pos) {
    FILE* tmp;
    u8 xele = 0;
    u8 move = 0;
    for (u8 ifn = 0, icb = 0; ifn < ele; ++ifn) {
        if (filenum[ifn] == combbuf[icb]) {
            if (icb == min_pos) {
                xele = move;
            }
            tmp = files[move];
            files[move] = files[ifn];
            files[ifn] = tmp;
            ++icb;
            ++move;
        }
    }
    if (xele != 0) {
        tmp = files[0];
        files[0] = files[xele];
        files[xele] = tmp;
    }
}
