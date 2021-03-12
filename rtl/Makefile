CXX       := g++
FBDIR     := .
VDIRFB    := $(FBDIR)/obj_dir
VERILATOR := verilator
VFLAGS    := -Wall --MMD --trace -cc
RM        := rm -rf

.PHONY: all
all: verilate

.PHONY: verilate
verilate: $(VDIRFB)/Vgigatron__ALL.a

.PRECIOUS: $(VDIRFB)/V%.cpp $(VDIRFB)/V%.h $(VDIRFB)/V%.mk

$(VDIRFB)/V%.mk:  $(VDIRFB)/V%.h;
$(VDIRFB)/V%.h:   $(VDIRFB)/V%.cpp;
$(VDIRFB)/V%.cpp: $(FBDIR)/%.v
	$(VERILATOR) $(VFLAGS) $*.v

$(VDIRFB)/V%__ALL.a: $(VDIRFB)/V%.mk
	cd $(VDIRFB); $(MAKE) -f V$*.mk

.PHONY: clean
clean:
	$(RM) $(VDIRFB)/
