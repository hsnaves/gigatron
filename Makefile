all: data rtl emulator sim

.PHONY: data
data:
	cd data; $(MAKE)


.PHONY: rtl
rtl:
	cd rtl; $(MAKE)

.PHONY: emulator
emulator:
	cd emulator; $(MAKE)

.PHONY: sim
sim: rtl emulator
	cd sim/rtl; $(MAKE)
	cd sim/cpp; $(MAKE)

.PHONY: clean
clean:
	cd sim/cpp; $(MAKE) clean
	cd sim/rtl; $(MAKE) clean
	cd emulator; $(MAKE) clean
	cd rtl; $(MAKE) clean
	cd data; $(MAKE) clean
