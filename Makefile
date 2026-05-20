#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wut_rules

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
LDFLAGS	=	-g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

# Core libraries
LIBS	:= -lwups -lwut -lnsysnet -lnssl

# Optional: uncomment if NotificationModule is installed (dkp-pacman -S wiiu-notifications)
# LIBS += -lnotifications

LIBDIRS	:= $(WUPSDIR) $(DEVKITPRO)/libwut $(PORTLIBS)

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
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
