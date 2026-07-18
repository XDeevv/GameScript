GameScript= ..


CC?= gcc
OUT?= $(GameScript)/lib/libGSstdlib.a
INCZ?= -I$(GameScript)/include -I. -Iinclude
DEFS= $(CC_EXTRA_FLAGS)
LIB=

OBJS= \
	GSstdblob.o \
	GSstdio.o \
	GSstdstream.o \
	GSstdmath.o \
	GSstdsystem.o \
	GSstdstring.o \
	GSstdaux.o \
	GSstdrex.o

SRCS= \
	GSstdblob.cpp \
	GSstdio.cpp \
	GSstdstream.cpp \
	GSstdmath.cpp \
	GSstdsystem.cpp \
	GSstdstring.cpp \
	GSstdaux.cpp \
	GSstdrex.cpp


GS32:
	$(CC) -O2 -fno-exceptions -fno-rtti -Wall -fno-strict-aliasing -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o
	rm *.o

GSprof:
	$(CC) -O2 -pg -fno-exceptions -fno-rtti -pie -gstabs -g3 -Wall -fno-strict-aliasing -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o
	rm *.o

GS64:
	$(CC) -O2 -m64 -fno-exceptions -D_GS64 -fno-rtti -Wall -fno-strict-aliasing -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o
	rm *.o

