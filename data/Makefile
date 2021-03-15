ORIGINAL_ROM=ROMv5a.rom
URL=https://github.com/kervinck/gigatron-rom/raw/master/ROMv5a.rom

all: rom.hex

.PRECIOUS: $(ORIGINAL_ROM)

$(ORIGINAL_ROM):
	wget $(URL)

rom.hex: $(ORIGINAL_ROM)
	hexdump -v -e '1/2 "%04x\n"' $(ORIGINAL_ROM) > rom.hex

.PHONY: clean
clean:
	rm -f rom.hex
