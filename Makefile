
GameScript=.
MAKE=make

GS32: folders
	cd GameScript; $(MAKE)
	cd GSstdlib; $(MAKE)
	cd GS; $(MAKE)

GSprof: folders
	cd GameScript; $(MAKE) GSprof
	cd GSstdlib; $(MAKE) GSprof
	cd GS; $(MAKE) GSprof

GS64: folders
	cd GameScript; $(MAKE) GS64
	cd GSstdlib; $(MAKE) GS64
	cd GS; $(MAKE) GS64

folders:
	mkdir -p lib
	mkdir -p bin

