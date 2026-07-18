GameScript= ..


CC?= gcc
OUT?= $(GameScript)/lib/libGameScript.a
INCZ?= -I$(GameScript)/include -I. -Iinclude
DEFS= $(CC_EXTRA_FLAGS)
LIB=

OBJS= \
	GSapi.o \
	GSbaselib.o \
	GSfuncstate.o \
	GSdebug.o \
	GSlexer.o \
	GSobject.o \
	GScompiler.o \
	GSstate.o \
	GStable.o \
	GSmem.o \
	GSvm.o \
	GSclass.o

SRCS= \
	GSapi.cpp \
	GSbaselib.cpp \
	GSfuncstate.cpp \
	GSdebug.cpp \
	GSlexer.cpp \
	GSobject.cpp \
	GScompiler.cpp \
	GSstate.cpp \
	GStable.cpp \
	GSmem.cpp \
	GSvm.cpp \
	GSclass.cpp



GS32:
	$(CC) -O2 -fno-exceptions -fno-rtti -Wall -fno-strict-aliasing -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o
	rm *.o

GSprof:
	$(CC) -O2 -pg -fno-exceptions -fno-rtti -pie -gstabs -g3 -Wall -fno-strict-aliasing -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o
	rm *.o

GS64:
	$(CC) -O2 -m64 -D_GS64 -fno-exceptions -fno-rtti -Wall -fno-strict-aliasing -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o
	rm *.o

