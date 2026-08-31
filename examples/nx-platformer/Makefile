ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR      ?= $(CURDIR)
TARGET      := nx-platformer
BUILD       := bin
SOURCES     := src src/states src/components include/stormengine2
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
# glm is at vendor/android/glm, not vendor/glm. The -I on vendor/ alone was
# meant to supply it and never could: <glm/glm.hpp> resolves to
# vendor/glm/glm.hpp, and there is no vendor/glm. devkitPro ships no glm
# portlib either, so nothing on the include path provided it and this build
# failed on the first engine header it reached. The desktop build hid it --
# there glm comes from the system /usr/include, which a cross build must not
# use. The submodule is shared with the Android example rather than vendored
# twice; the directory is named for where it was first needed, not for a
# platform restriction.
CFLAGS  +=  $(INCLUDE) -D__SWITCH__ \
            -I$(DEVKITPRO)/portlibs/switch/include \
            -I$(TOPDIR)/../../vendor \
            -I$(TOPDIR)/../../vendor/android/glm \
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

# The same directory list, kept so the sub-make below can narrow VPATH to
# source files only. See the note there.
export SEARCHDIRS := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
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

# VPATH searches for TARGETS as well as prerequisites, and `include/stormengine2`
# is a symlink to the engine's `common/` -- where `make -f Makefile.debian`
# leaves its x86-64 object files. With a blanket VPATH, make finds
# `common/ecs.o` while looking for the aarch64 `ecs.o`, decides the target is
# already satisfied, never compiles it, and hands a bare name to the linker:
#
#     ld: cannot find ecs.o: No such file or directory
#
# Six engine translation units silently do not get built, and the failure blames
# the linker rather than the desktop build that caused it. Narrowing to `vpath`
# per-pattern keeps source lookup working while object lookup ignores these
# directories entirely, so a stale desktop build cannot poison a Switch build.
VPATH :=
vpath %.cpp $(SEARCHDIRS)
vpath %.c   $(SEARCHDIRS)
vpath %.s   $(SEARCHDIRS)
vpath %.S   $(SEARCHDIRS)
vpath %.bin $(SEARCHDIRS)

DEPENDS :=  $(OFILES:.o=.d)

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
