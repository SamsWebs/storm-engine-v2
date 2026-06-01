ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR      ?= $(CURDIR)
TARGET      := nx-platformer
BUILD       := bin
SOURCES     := src src/states src/components engine
DATA        := data
INCLUDES    := include
ROMFS       := $(TOPDIR)/romfs

include $(DEVKITPRO)/libnx/switch_rules

export NROFLAGS += --nacp=$(TOPDIR)/$(BUILD)/$(TARGET).nacp --romfsdir=$(TOPDIR)/romfs

APP_TITLE   := Storm Platformer (Switch)
APP_AUTHOR  := willsams

#---------------------------------------------------------------------------------
ARCH    :=  -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS  :=  `$(PREFIX)pkg-config --cflags sdl2 SDL2_mixer SDL2_image SDL2_ttf` \
            -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS  +=  $(INCLUDE) -D__SWITCH__ \
            -I$(DEVKITPRO)/portlibs/switch/include \
            -I$(TOPDIR)/../../vendor \
            -I/usr/local/include

CXXFLAGS    := $(CFLAGS) -std=c++17 -fno-exceptions -Wno-reorder -Wno-unused-parameter \
               -Wno-unused-variable -Wno-unused-function

ASFLAGS :=  -g $(ARCH)
LDFLAGS =   -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    :=  `$(PREFIX)pkg-config --libs sdl2 SDL2_mixer SDL2_image SDL2_ttf` \
            -ltinyxml2 -lnx

LIBDIRS := $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   :=  $(CURDIR)/$(BUILD)/$(TARGET)
export TOPDIR   :=  $(CURDIR)

export VPATH    :=  $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                    $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  :=  $(CURDIR)/$(BUILD)

CPPFILES    :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(CURDIR)/$(dir)/*.cpp)))
SFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(CURDIR)/$(dir)/*.s)))
BINFILES    :=  $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD   :=  $(CXX)

export OFILES_SRC   :=  $(CPPFILES:.cpp=.o) $(SFILES:.s=.o)
export OFILES       :=  $(OFILES_SRC)

export INCLUDE  :=  $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                    $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                    -I$(CURDIR)/$(BUILD)

export LIBPATHS :=  $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD)

EMULATOR ?= /usr/bin/flatpak run --branch=stable --arch=x86_64 --command=yuzu-launcher org.yuzu_emu.yuzu

run:
	$(EMULATOR) $(BUILD)/$(TARGET).nro

#---------------------------------------------------------------------------------
else

DEPENDS :=  $(OFILES:.o=.d)

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
