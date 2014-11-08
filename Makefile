# ble - bluetooth low energy
# LICENSE: BSD 

include config.mk

SRC = ble.c
OBJ = ${SRC:.c=.o}

all: options ble

options:
	@echo ble build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

ble: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f ble ${OBJ}

.PHONY: all options clean 
