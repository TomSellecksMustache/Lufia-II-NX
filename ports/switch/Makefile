# Nintendo Switch homebrew build — adapted from the standard switch-examples layout.
.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO=/opt/devkitpro)
endif
TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules
TARGET := lufia2nx
BUILD := build
SOURCES := source
DATA :=
INCLUDES := include
APP_TITLE := Lufia II NX
APP_AUTHOR := Community homebrew project
APP_VERSION ?= 0.1.0
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -Wall -Wextra -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS += -D__SWITCH__ $(INCLUDE) `sdl2-config --cflags`
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := `sdl2-config --libs`
LIBDIRS := $(PORTLIBS) $(LIBNX)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_SRC)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
.PHONY: all clean $(BUILD)
all: $(BUILD)
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nacp
else
DEPENDS := $(OFILES:.o=.d)
.PHONY: all
all: $(OUTPUT).nro
$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
