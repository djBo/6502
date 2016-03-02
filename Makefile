# Adjust the run address to match the .org in the source code
all: bios.bin


bios.bin: bios.obj
	lk65 -code "$$E000-$$FFFF" -output bios.bin -bin bios.obj

bios.obj:
	as65 bios.asm





bios-cc65.mon: bios-cc65.bin
	bintomon -v -l 0xE000 bios-cc65.bin >bios-cc65.mon


bios-cc65.bin: bios-cc65.o
#	ld65 -t none -vm -m bios-cc65.map -o bios-cc65.bin bios-cc65.o
	ld65 -C bios-cc65.cfg -o bios-cc65.bin bios-cc65.o

bios-cc65.o: bios-cc65.asm
	ca65 --listing --feature labels_without_colons -o bios-cc65.o bios-cc65.asm
#	ca65 -g -l min_mon.lst --feature labels_without_colons -o basic.o min_mon.asm

clean:
	$(RM) *.o *.lst *.map *.bin *.obj

distclean: clean

#ca65 -D $i msbasic.s -o tmp/$i.o &&
#ld65 -C $i.cfg tmp/$i.o -o tmp/$i.bin -Ln tmp/$i.lbl