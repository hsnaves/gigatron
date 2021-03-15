all: data rtl emulator tests

.PHONY: data
data:
	cd data; $(MAKE)


.PHONY: rtl
rtl:
	cd rtl; $(MAKE)

.PHONY: emulator
emulator:
	cd emulator; $(MAKE)

.PHONY: tests
tests: rtl emulator
	cd tests/rtl; $(MAKE)
	cd tests/cpp; $(MAKE)

.PHONY: run_tests
run_tests: tests
	cd tests/cpp; $(MAKE) run_sim

.PHONY: clean
clean:
	cd tests/cpp; $(MAKE) clean
	cd tests/rtl; $(MAKE) clean
	cd emulator; $(MAKE) clean
	cd rtl; $(MAKE) clean
	cd data; $(MAKE) clean
