all: rtl data sim

.PHONY: rtl
rtl:
	cd rtl; $(MAKE)

.PHONY: data
data:
	cd data; $(MAKE)

.PHONY: sim
sim: rtl
	cd sim/rtl; $(MAKE)
	cd sim/cpp; $(MAKE)

.PHONY: run_sim
run_sim: sim data
	cd sim/cpp; $(MAKE) run_sim

.PHONY: clean
clean:
	cd sim/cpp; $(MAKE) clean
	cd sim/rtl; $(MAKE) clean
	cd rtl; $(MAKE) clean
	cd data; $(MAKE) clean
