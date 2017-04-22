ifeq ($(strip $(SCDS2_TOOLS)),)
$(error "Please set SCDS2_TOOLS in your environment. export SCDS2_TOOLS=<path to scds2 tools>")
endif

include $(SCDS2_TOOLS)/scds2_rules

# - - - Libraries and includes - - -
LIBS        := -lz -lds2

INCLUDE     := -Isource -Isource/unzip -Isource/nds

# - - - Names - - -
OUTPUT      := ds2comp
PLUGIN_DIR  := DS2COMP

# - - - Sources and objects - - -
C_SOURCES   = source/unzip/unzip.c source/unzip/ioapi.c \
              source/nds/bdf_font.c source/nds/bitmap.c \
              source/nds/draw.c source/nds/ds2_main.c \
              source/nds/gui.c source/minigzip.c source/miniunz.c
CPP_SOURCES  =
SOURCES      = $(C_SOURCES) $(CPP_SOURCES)
C_OBJECTS    = $(C_SOURCES:.c=.o)
CPP_OBJECTS  = $(CPP_SOURCES:.cpp=.o)
OBJECTS      = $(C_OBJECTS) $(CPP_OBJECTS)

# - - - Compilation flags - - -
DEFS        := -DHAVE_MKSTEMP '-DACCEPT_SIZE_T=size_t'                        \
               -DUSE_FILE32API -DHAS_STDINT_H

CFLAGS      += -fno-builtin -ffunction-sections -O3                           \
               $(DEFS) $(INCLUDE)

.PHONY: clean
.SUFFIXES: .elf .dat .plg .c .cpp .o

all: $(OUTPUT).plg

$(OUTPUT).plg: $(OUTPUT).pak

release: all
	-rm -f $(OUTPUT).zip
	zip -r $(OUTPUT).zip $(PLUGIN_DIR) $(OUTPUT).plg $(OUTPUT).bmp $(OUTPUT).ini copyright installation.txt README.md source.txt

$(OUTPUT).elf: $(OBJECTS)

clean:
	-rm -rf $(OUTPUT).plg $(OUTPUT).dat $(OUTPUT).pak $(OUTPUT).elf depend $(OBJECTS)

Makefile: depend

depend: $(SOURCES)
	$(CC) -MM $(CFLAGS) $(SOURCES) > $@
	touch Makefile

-include depend
