OBJS := $(OBJS) gigatron.o main.o

gigatron.o: gigatron.c gigatron.h
main.o: main.c gigatron.h
