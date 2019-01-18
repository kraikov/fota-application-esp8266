Q := @

# Project specific folders
APP_FOLDER				:= app
OBJECT_FOLDER           := obj
BIN_FOLDER              := bin
DEFAULT_BIN_FOLDER		:= default
BOOT_BIN_FOLDER			:= boot
DRIVER_FOLDER           := driver
LD_SCRIPT_FOLDER        := ld

# Binary files
DEFAULT_BIN				:= $(DEFAULT_BIN_FOLDER)/esp_init_data_default
BLANK_BIN   			:= $(DEFAULT_BIN_FOLDER)/blank
BOOT_BIN    			:= $(BOOT_BIN_FOLDER)/BootloaderDriver
USER_BIN0   			:= user_0
USER_BIN1   			:= user_1

# Include directories
SDK_BASE    			?= ../espressif/ESP8266_SDK
SDK_LIBDIR  			:= lib
SDK_INCDIR 				:= include
SDK_LIBDIR 				:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR 				:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))

# Tools
XTENSA_TOOLS_ROOT		 = ../espressif/xtensa-lx106-elf/bin
COMPILE 				:= $(addprefix $(XTENSA_TOOLS_ROOT)/,xtensa-lx106-elf-gcc)
LINK 					:= $(addprefix $(XTENSA_TOOLS_ROOT)/,xtensa-lx106-elf-gcc)
GEN_TOOL     			?= ../esptool/esptool2.exe
FLASH_TOOL 				?= ../esptool/esptool.exe

# Compiler/Linker options
LIBS    				= c gcc hal phy net80211 lwip wpa main pp crypto ssl
CFLAGS  				= -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wno-implicit -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls  -mtext-section-literals  -D__ets__ -DICACHE_FLASH
LDFLAGS 				= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static
FW_SECTS      			= .text .data .rodata
FW_USER_ARGS  			= -quiet -bin -boot2
LIBS					:= $(addprefix -l,$(LIBS))

# Compilation source files/includes
SRC_DIR					:= app drivers mqtt
BUILD_DIR				:= $(addprefix $(OBJECT_FOLDER)/,$(SRC_DIR))
C_FILES					:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c*))
INCLUDE					:= $(foreach sdir,$(SRC_DIR),$(addprefix -I/,$(sdir)))
C_OBJ					:= $(patsubst %.c,%.o,$(C_FILES))
O_FILES					:= $(patsubst %.o,$(OBJECT_FOLDER)/%.o,$(C_OBJ))

# Function
.SECONDARY:
.PHONY: all clean

info:
	@echo OBJECT: $(O_FILES)
	@echo C: $(C_FILES)
	@echo INCLUDE: $(INCLUDE)
	
build: $(BUILD_DIR) $(BIN_FOLDER) $(BIN_FOLDER)/$(USER_BIN0).bin $(BIN_FOLDER)/$(USER_BIN1).bin

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(BIN_FOLDER):
	$(Q) mkdir -p $@

$(OBJECT_FOLDER)/%.o: %.c %.h
	@echo "COMPILE $(notdir $<)"
	$(Q) $(COMPILE) -I$(INCLUDE) $(SDK_INCDIR) $(CFLAGS) -o $@ -c $<

$(OBJECT_FOLDER)/%.elf: $(O_FILES)
	@echo "LINK $(notdir $@)"
	$(Q) $(LINK) -L$(SDK_LIBDIR) -Tld/$(notdir $(basename $@)).ld $(LDFLAGS) -Wl,--start-group $(LIBS) $^ -Wl,--end-group -o $@
	
	
$(BIN_FOLDER)/%.bin: $(OBJECT_FOLDER)/%.elf
	@echo "GEN $(notdir $@)"
	$(Q) $(GEN_TOOL) $(FW_USER_ARGS) $^ $@ $(FW_SECTS)


clean:
	@echo "Cleaning..."
	$(Q) rm -rf $(OBJECT_FOLDER)
	$(Q) rm -rf $(BIN_FOLDER)
	@echo "Done"


flash:
	$(FLASH_TOOL) --port COM7 write_flash -fs 4MB 0x2000 $(BIN_FOLDER)/$(USER_BIN0).bin 0x82000 $(BIN_FOLDER)/$(USER_BIN1).bin


flashboot:
	$(FLASH_TOOL) --port COM7 write_flash -fs 4MB 0x000000 $(BOOT_BIN).bin


erase:
	$(FLASH_TOOL) --port COM7 erase_flash


flashinit:
	$(FLASH_TOOL) --port COM7 write_flash -fs 4MB 0xFC000 $(BLANK_BIN).bin 0x3FC000 $(DEFAULT_BIN).bin


flashread:
	$(FLASH_TOOL) --port COM7 read_flash 0x00000 0x400000 dumped_flash2.bin


start_Realterm:
	C:\Users\kraykov\Desktop\Diploma\RealTerm\StartRealterm.cmd