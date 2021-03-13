ORIGINAL_ROM=ROMv5a.rom
URL=https://github.com/kervinck/gigatron-rom/raw/master/ROMv5a.rom

all: rom.hex

rom.hex:
	rm -f $(ORIGINAL_ROM)
	wget $(URL)
	hexdump -v -e '1/2 "%04x\n"' $(ORIGINAL_ROM) > rom.hex

.PHONY: clean
clean:
	rm -f rom.hex
