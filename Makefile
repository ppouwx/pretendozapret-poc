#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitPro")
endif

WUT_ROOT	?= $(DEVKITPRO)/wut
WUPS_ROOT	?= $(DEVKITPRO)/wups
include $(WUT_ROOT)/share/wut_rules

# Link through GCC frontend so -g and other flags pass through correctly
LD	:= $(CC)

TARGET		:=	wiiu-bypass
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	source

CFLAGS	:=	-g -Wall -O2 -ffunction-sections -fdata-sections \
			$(MACHDEP) $(INCLUDE)

CFLAGS	+=	-DWUPS_PLUGIN

CXXFLAGS	:= $(CFLAGS) -std=c++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-g $(ARCH) $(RPLSPECS) -Wl,-Map,$(notdir $*.map) -Wl,--unresolved-symbols=ignore-in-object-files

# Core libraries - wut.specs/rpl.specs handle system RPL imports automatically
LIBS	:= -lwups -lwut

# Optional: uncomment if NotificationModule is installed (dkp-pacman -S wiiu-notifications)
# LIBS += -lnotifications

LIBDIRS	:= $(WUPS_ROOT) $(WUT_ROOT) $(PORTLIBS)

#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CXXFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export OFILES	:=	$(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(SFILES:.s=.o) \
			$(BINFILES:.bin=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).wps $(OUTPUT).elf $(OUTPUT).map

#-------------------------------------------------------------------------------
else
#-------------------------------------------------------------------------------

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).wps: $(OUTPUT).elf
	$(SILENTCMD)elf2rpl --rpl $< $(OUTPUT).rpl $(ERROR_FILTER)
	$(SILENTCMD)mv $(OUTPUT).rpl $@
	@echo built ... $(notdir $@)

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
