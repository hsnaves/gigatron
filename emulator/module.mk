OBJS := $(OBJS) gigatron.o

gigatron.o: gigatron.c gigatron.h
main.o: main.c gigatron.h
