ORIGINAL_ROM=ROMv5a.rom
URL=https://github.com/kervinck/gigatron-rom/raw/master/ROMv5a.rom

all: rom.hex

.PRECIOUS: $(ORIGINAL_ROM)

$(ORIGINAL_ROM):
	wget $(URL)

rom.hex: $(ORIGINAL_ROM)
	python convert.py $(ORIGINAL_ROM) > rom.hex

rom.b: $(ORIGINAL_ROM)
	python convert.py --binary $(ORIGINAL_ROM) > rom.b

.PHONY: clean
clean:
	rm -f rom.hex rom.b
