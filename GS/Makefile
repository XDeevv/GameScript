GameScript= ..


OUT= $(GameScript)/bin/GS
INCZ= -I$(GameScript)/include -I. -I$(GameScript)/GSlibs
LIBZ= -L$(GameScript)/lib
LIB= -lGameScript -lGSstdlib

OBJS= GS.o

SRCS= GS.c


GS32:
	g++ -O2 -fno-exceptions -fno-rtti -o $(OUT) $(SRCS) $(INCZ) $(LIBZ) $(LIB)

GSprof:
	g++ -O2 -pg -fno-exceptions -fno-rtti -pie -gstabs -g3 -o $(OUT) $(SRCS) $(INCZ) $(LIBZ) $(LIB)

GS64:
	g++ -O2 -m64 -fno-exceptions -fno-rtti -D_GS64 -o $(OUT) $(SRCS) $(INCZ) $(LIBZ) $(LIB)

