CC = cc
BIN = spac
IBIN = inspact
XBIN = xspac
MAN = spac.1
IMAN = inspact.1
SRCS = spac.c splib.c
ISRC = inspact.c
XSRC = xspac.c valib.c splib.c
XMAN = xspac.1
SOPTS = -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith -Wcast-qual \
	-Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
	-fstack-protector-all -fPIE
ROPTS = ${SOPTS} -Ofast -m64 -mtune=native -DNDEBUG -s -ftree-vectorize
DOPTS = ${SOPTS} -O0 -g -fno-omit-frame-pointer -fsanitize=address -ggdb


PREFIX = /usr/local
BINDIR = ${DESTDIR}${PREFIX}/bin
MANDIR = ${DESTDIR}${PREFIX}/share/man/man1

all: release

release:
	@echo compiling release build
	@${CC} ${SRCS} ${ROPTS} -o ${BIN}
	@${CC} ${ISRC} ${ROPTS} -o ${IBIN}
	@${CC} ${XSRC} ${ROPTS} -o ${XBIN}

debug:
	@echo compiling debug build
	@${CC} ${SRCS} ${DOPTS} -o ${BIN}
	@${CC} ${ISRC} ${DOPTS} -o ${IBIN}
	@${CC} ${XSRC} ${DOPTS} -o ${XBIN}

clean:
	@echo cleaning
	@${RM} ${BIN} ${IBIN} ${XBIN}

install: all
	@mkdir -p ${BINDIR}
	@echo installing executable ${BIN} in ${BINDIR}
	@cp -f ${BIN} ${BINDIR}
	@echo installing executable ${IBIN} in ${BINDIR}
	@cp -f ${IBIN} ${BINDIR}
	@echo installing executable ${XBIN} in ${BINDIR}
	@cp -f ${XBIN} ${BINDIR}
	@mkdir -p ${MANDIR}
	@echo installing manual page ${MAN} in ${MANDIR}
	@cp -f ${MAN} ${MANDIR}
	@echo installing manual page ${IMAN} in ${MANDIR}
	@cp -f ${IMAN} ${MANDIR}
	@echo installing manual page ${XMAN} in ${MANDIR}
	@cp -f ${XMAN} ${MANDIR}

uninstall:
	@echo removing executable ${BIN} from ${BINDIR}
	@rm -f ${BINDIR}/${BIN}
	@echo removing executable ${IBIN} from ${BINDIR}
	@rm -f ${BINDIR}/${IBIN}
	@echo removing executable ${XBIN} from ${BINDIR}
	@rm -f ${BINDIR}/${XBIN}
	@echo removing manual page ${MAN} from ${MANDIR}
	@rm -f ${MANDIR}/${MAN}
	@echo removing manual page ${IMAN} from ${MANDIR}
	@rm -f ${MANDIR}/${IMAN}
	@echo removing manual page ${XMAN} from ${MANDIR}
	@rm -f ${MANDIR}/${XMAN}

.PHONY: all debug release clean install uninstall
