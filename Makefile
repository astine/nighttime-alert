# nighttime - forced bedtime shutdown
# See LICENSE file for copyright and license details.

include config.mk

SRC = nighttime.c draw.c
OBJ = ${SRC:.c=.o}

all: options nighttime

options:
	@echo nighttime build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC -c $<
	@${CC} -c $< ${CFLAGS}

${OBJ}: config.mk draw.h

nighttime: nighttime.o draw.o
	@echo CC -o $@
	@${CC} -o $@ nighttime.o draw.o ${LDFLAGS}

install: nighttime
	@if [ ! `whoami` = root ]; then \
        echo Must be root to install; else \
		echo cp nighttime /usr/local/bin/; \
		cp nighttime /usr/local/bin/; \
		chown root:root /usr/local/bin/nighttime; \
		chmod 6555 /usr/local/bin/nighttime; \
		echo Place "0 22 * * * /usr/local/bin/nighttime -t 22:30" in crontab to trigger nighttime-alert at 10:00 PM and shutdown the computer at 10:30.; fi

clean:
	@echo cleaning
	@rm -f nighttime stest ${OBJ} nighttime-${VERSION}.tar.gz
