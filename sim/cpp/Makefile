CXX      := g++
CXXFLAGS := -Wall -Og -g
OBJDIR   := obj_dir
RTLDIR   := ../rtl
VERILATOR_ROOT ?= $(shell bash -c 'verilator -V|grep VERILATOR_ROOT | head -1 | sed -e " s/^.*=\s*//"')
VROOT    := $(VERILATOR_ROOT)
INCS     := -I$(RTLDIR)/obj_dir/ -I$(VROOT)/include
VOBJDIR  := $(RTLDIR)/obj_dir
SYSVDIR  := $(VROOT)/include
VSRC     := verilated.cpp verilated_vcd_c.cpp
VLIB     := $(addprefix $(OBJDIR)/,$(subst .cpp,.o,$(VSRC)))
# Sources necessary to build the simulation program
SRCS     := sim_main.cpp
OBJS     := $(subst .cpp,.o,$(SRCS))
OBJS     := $(addprefix $(OBJDIR)/,$(OBJS)) $(VLIB)
RM       := rm -rf

#.PHONY: all
all: sim run_sim

$(OBJDIR)/%.o: %.cpp
	$(mk-objdir)
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

$(OBJDIR)/%.o: $(SYSVDIR)/%.cpp
	$(mk-objdir)
	$(CXX) $(CXXFLAGS) $(INCS) -c $< -o $@

sim: $(OBJS) $(VOBJDIR)/Vtop__ALL.a
	$(CXX) $(CXXFLAGS) $(INCS) $^ -o $@

.PHONY: run_sim
run_sim: sim
	./sim +trace

define	mk-objdir
	@bash -c "if [ ! -e $(OBJDIR) ]; then mkdir -p $(OBJDIR); fi"
endef

.PHONY: clean
clean:
	$(RM) $(OBJDIR)/ logs sim
