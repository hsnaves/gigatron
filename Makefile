all: rtl sim

.PHONY: rtl
rtl:
	cd rtl; $(MAKE)

.PHONY: sim
sim: rtl
	cd sim/rtl; $(MAKE)
	cd sim/cpp; $(MAKE)

.PHONY: clean
clean:
	cd sim/cpp; $(MAKE) clean
	cd sim/rtl; $(MAKE) clean
	cd rtl; $(MAKE) clean
