PROG_NAME = testrom
BUILD_DIR = build

ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-

COMMA:=,

CC = $(GCCN64PREFIX)gcc
CXX = $(GCCN64PREFIX)g++
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
SIZE = $(GCCN64PREFIX)size
STRIP = $(GCCN64PREFIX)strip
N64TOOL = $(ROOTDIR)/bin/n64tool
N64SYM = $(ROOTDIR)/bin/n64sym
N64ELFCOMPRESS = $(ROOTDIR)/bin/n64elfcompress
ECHO = echo

# To enable verbose prints, set VERBOSE=1 when building:
#   make VERBOSE=1
ifneq ($(strip $(VERBOSE)),1)
V = @
endif


C_SOURCES = \
	$(wildcard *.c) \
	$(wildcard ../../picocart64_shared/*.c)

N64_LIBDIR = $(ROOTDIR)/mips64-elf/lib
N64_INCLUDEDIR = $(ROOTDIR)/mips64-elf/include

C_INCLUDES = \
	-I$(N64_INCLUDEDIR) \
	-I$(BUILD_DIR) \
	-I../../picocart64_shared/include \

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

ASFLAGS = -mtune=vr4300 -march=vr4300 -mabi=o64 -Wa,--fatal-warnings -I$(N64_INCLUDEDIR)

N64_C_AND_CXX_FLAGS =  -march=vr4300 -mtune=vr4300 -mabi=o64 -I$(N64_INCLUDEDIR)
N64_C_AND_CXX_FLAGS += -falign-functions=32   # NOTE: if you change this, also change backtrace() in backtrace.c
N64_C_AND_CXX_FLAGS += -ffunction-sections -fdata-sections -g
N64_C_AND_CXX_FLAGS += -ffast-math -ftrapping-math -fno-associative-math
N64_C_AND_CXX_FLAGS += -DN64 -O2 -Wall -Werror -Wno-error=deprecated-declarations -fdiagnostics-color=always
N64_C_AND_CXX_FLAGS += -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-function -Wno-error=unused-parameter -Wno-error=unused-but-set-parameter -Wno-error=unused-label -Wno-error=unused-local-typedefs -Wno-error=unused-const-variable
N64_C_AND_CXX_FLAGS += -ftrivial-auto-var-init=pattern
N64_CFLAGS = $(N64_C_AND_CXX_FLAGS) -std=gnu17
CFLAGS = $(N64_CFLAGS) $(C_INCLUDES)

LDFLAGS = -g -L$(N64_LIBDIR) -ldragon -lc -lm -ldragonsys -Tn64.ld --gc-sections --wrap __do_global_ctors
N64TOOLFLAGS = -l 1M -t "OpenPico64 Test ROM"

all: $(BUILD_DIR)/$(PROG_NAME).z64


$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(V)$(ECHO) "[  CC ]" $(notdir $<)
	$(V)$(CC) -o $@ $< -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) 

$(BUILD_DIR)/$(PROG_NAME).z64: $(BUILD_DIR)/$(PROG_NAME).elf
	$(V)$(ECHO) "[ Z64 ]" $(notdir $@)
	$(N64SYM) $< $<.sym
	cp $< $<.stripped
	$(STRIP) -s $<.stripped
	$(N64ELFCOMPRESS) -o $(dir $<) -c 1 $<.stripped
	@rm -f $@
	DFS_FILE="$(filter %.dfs, $^)"; \
	if [ -z "$$DFS_FILE" ]; then \
		$(N64TOOL) $(N64TOOLFLAGS) --toc --output $@ --align 256 $<.stripped --align 8 $<.sym --align 8; \
	else \
		MSYM_FILE="$(filter %.msym, $^)"; \
		if [ -z "$$MSYM_FILE" ]; then \
			$(N64TOOL) $(N64TOOLFLAGS) --toc --output $@ --align 256 $<.stripped --align 8 $<.sym --align 16 "$$DFS_FILE"; \
		else \
			$(N64TOOL) $(N64TOOLFLAGS) --toc --output $@ --align 256 $<.stripped --align 8 $<.sym --align 8 "$$MSYM_FILE" --align 16 "$$DFS_FILE"; \
		fi \
	fi

$(BUILD_DIR)/$(PROG_NAME).elf: $(OBJECTS)
	$(V)$(ECHO) "[  LD ]" $(notdir $@)
# We always use g++ to link except for ucode and DSO files because of the inconsistencies
# between ld when it comes to global ctors dtors. Also see __do_global_ctors
	EXTERNS_FILE="$(filter %.externs, $^)"; \
	if [ -z "$$EXTERNS_FILE" ]; then \
		$(CXX) -o $@ $(filter %.o, $^) $(filter-out $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a, $(filter %.a, $^)) \
			-lc -mabi=o64 $(patsubst %,-Wl$(COMMA)%,$(LDFLAGS)) -Wl,-Map=$(BUILD_DIR)/$(notdir $(basename $@)).map; \
	else \
		$(CXX) -o $@ $(filter %.o, $^) $(filter-out $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a, $(filter %.a, $^)) \
			-lc -mabi=o64 $(patsubst %,-Wl$(COMMA)%,$(LDFLAGS)) -Wl,-T"$$EXTERNS_FILE" -Wl,-Map=$(BUILD_DIR)/$(notdir $(basename $@)).map; \
	fi
	$(V)$(SIZE) -G $@


$(BUILD_DIR):
	$(V)mkdir -p $@
	$(V)echo -n "#define GIT_REV 0x" > $(BUILD_DIR)/git_info.h
	$(V)git rev-parse --short=8 HEAD >> $(BUILD_DIR)/git_info.h
.PHONY: $(BUILD_DIR)

.PHONY: clean
clean:
	rm -rf ./build
