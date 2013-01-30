# - - - Modifiable paths - - -
DS2SDKPATH  := /opt/ds2sdk
CROSS       := /opt/mipsel-4.1.2-nopic/bin/mipsel-linux-

# - - - Libraries and includes - - -
FS_DIR       = $(DS2SDKPATH)/libsrc/fs
CONSOLE_DIR  = $(DS2SDKPATH)/libsrc/console
KEY_DIR      = $(DS2SDKPATH)/libsrc/key
ZLIB_DIR     = $(DS2SDKPATH)/libsrc/zlib

LIBS        := $(DS2SDKPATH)/lib/libds2b.a -lc -lm -lgcc
EXTLIBS     := $(DS2SDKPATH)/lib/libds2a.a

INCLUDE     := -Isource -Isource/unzip -Isource/nds -I$(DS2SDKPATH)/include \
               -I$(FS_DIR) -I$(CONSOLE_DIR) -I$(KEY_DIR) -I$(ZLIB_DIR)

LINK_SPEC   := $(DS2SDKPATH)/specs/link.xn
START_ASM   := $(DS2SDKPATH)/specs/start.S
START_O     := start.o

# - - - Names - - -
OUTPUT      := ds2comp
PLUGIN_DIR  := DS2COMP

# - - - Tools - - -
CC           = $(CROSS)gcc
AR           = $(CROSS)ar rcsv
LD           = $(CROSS)ld
OBJCOPY      = $(CROSS)objcopy
NM           = $(CROSS)nm
OBJDUMP      = $(CROSS)objdump

# - - - Sources and objects - - -
C_SOURCES   = source/unzip/explode.c source/unzip/unreduce.c \
              source/unzip/unshrink.c source/unzip/unzip.c \
              source/nds/bdf_font.c source/nds/bitmap.c \
              source/nds/draw.c source/nds/ds2_main.c \
              source/nds/gui.c source/minigzip.c source/nds/entry.c
CPP_SOURCES =
SOURCES      = $(C_SOURCES) $(CPP_SOURCES)
C_OBJECTS    = $(C_SOURCES:.c=.o)
CPP_OBJECTS  = $(CPP_SOURCES:.cpp=.o)
OBJECTS      = $(C_OBJECTS) $(CPP_OBJECTS)

# - - - Compilation flags - - -
CFLAGS := -mips32 -mno-abicalls -fno-pic -fno-builtin \
	      -fno-exceptions -ffunction-sections -mno-long-calls \
	      -msoft-float -G 4 \
          -O3 -fomit-frame-pointer -fgcse-sm -fgcse-las -fgcse-after-reload \
          -fweb -fpeel-loops

DEFS   := -DHAVE_MKSTEMP '-DACCEPT_SIZE_T=size_t'

.PHONY: clean
.SUFFIXES: .elf .dat .plg .c .cpp .o

all: $(OUTPUT).plg

release: all
	-rm -f $(OUTPUT).zip
	zip -r $(OUTPUT).zip $(PLUGIN_DIR) $(OUTPUT).plg $(OUTPUT).bmp $(OUTPUT).ini copyright installation.txt README.md source.txt version

# $< is the source (OUTPUT.dat); $@ is the target (OUTPUT.plg)
.dat.plg:
	$(DS2SDKPATH)/tools/makeplug $< $@

# $< is the source (OUTPUT.elf); $@ is the target (OUTPUT.dat)
.elf.dat:
	$(OBJCOPY) -x -O binary $< $@

$(OUTPUT).elf: Makefile $(OBJECTS) $(START_O) $(LINK_SPEC) $(EXTLIBS)
	$(CC) -nostdlib -static -T $(LINK_SPEC) -o $@ $(START_O) $(OBJECTS) $(EXTLIBS) $(LIBS)

$(EXTLIBS):
	$(MAKE) -C $(DS2SDKPATH)/source/

$(START_O): $(START_ASM)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

clean:
	-rm -rf $(OUTPUT).plg $(OUTPUT).dat $(OUTPUT).elf depend $(OBJECTS) $(START_O)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) $(DEFS) -o $@ -c $<
.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDE) $(DEFS) -fno-rtti -o $@ -c $<

Makefile: depend

depend: $(SOURCES)
	$(CC) -MM $(CFLAGS) $(INCLUDE) $(DEFS) $(SOURCES) > $@
	touch Makefile

-include depend
