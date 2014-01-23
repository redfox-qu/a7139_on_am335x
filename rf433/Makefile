TOP_DIR = $(shell /bin/pwd)/../..

include $(TOP_DIR)/Env.make

RFP_TARGET	:= rfrepeater
RFC_TARGET	:= rfcli
RFP_SHELL	:= rf433.sh
RFP_S_SHELL	:= S90can_net
VRFP_TEST	:= rf433_udp_test
ERFP_TEST	:= rf433_se433_test
RD_TEST		:= rf433_read
TARGET 		:= $(RFP_TARGET) $(RFC_TARGET) $(VRFP_TEST) $(ERFP_TEST) $(RD_TEST)
INST_DIR  	:= $(INSTALL_ROOTFS_DIR)/rf433
INST_ETC_D  := $(INST_DIR)/etc
INST_BIN_D	:= $(INST_DIR)/usr/sbin

RFP_OBJS	:= rfrepeater.o rf433lib.o common.o buffer.o
RFC_OBJS	:= rfcli.o common.o
VRFP_OBJS	:= rf433_udp_test.o rf433lib.o common.o buffer.o
ERFP_OBJS	:= rf433_se433_test.o rf433lib.o common.o buffer.o
RD_OBJS		:= rf433_read.o rf433lib.o common.o buffer.o
RF_HEADER	:= rfrepeater.h rf433lib.h rf433pkg.h common.h rfcli.h buffer.h list.h

CFLAGS  	+= -I$(FS_DIR)/nvram -I./ -I$(LINUXKERNEL_DIR) -g -Werror -Wall
LDFLAGS 	+= -L$(FS_DIR)/nvram -lpthread -lnvram

all: $(TARGET)

$(RFP_TARGET): $(RFP_OBJS) $(RF_HEADER)
	@echo LD $@
	@$(CC) $(LDFLAGS) -o $(RFP_TARGET) $(RFP_OBJS)

$(RFC_TARGET): $(RFC_OBJS) $(RF_HEADER)
	@echo LD $@
	@$(CC) $(LDFALGS) -o $(RFC_TARGET) $(RFC_OBJS)

$(VRFP_TEST): $(VRFP_OBJS) $(RF_HEADER)
	@echo LD $@
	@$(CC) $(LCFLAGS) -o $@ $(VRFP_OBJS)

$(ERFP_TEST): $(ERFP_OBJS) $(RF_HEADER)
	@echo LD $@
	@$(CC) $(LCFLAGS) -o $@ $(ERFP_OBJS)

$(RD_TEST): $(RD_OBJS) $(RF_HEADER)
	@echo LD $@
	@$(CC) $(LCFLAGS) -o $@ $(RD_OBJS)

clean:
	@echo clean $(TARGET)
	@rm -rf $(TARGET) *.o *.bak

install:
	@echo INSTALL $(RFP_TARGET)
	@install -p -D -m 0755 $(RFP_TARGET) $(INST_BIN_D)/$(RFP_TARGET)
	@$(STRIP) $(INST_BIN_D)/$(RFP_TARGET)
	@echo INSTALL $(RFC_TARGET)
	@install -p -D -m 0755 $(RFC_TARGET) $(INST_BIN_D)/$(RFC_TARGET)
	@$(STRIP) $(INST_BIN_D)/$(RFC_TARGET)
	@echo INSTALL $(RFP_SHELL)
	@install -p -D -m 0755 $(RFP_SHELL) $(INST_ETC_D)/init.d/$(RFP_SHELL)
	@echo INSTALL $(RFP_S_SHELL)
	@install -d $(INST_ETC_D)/rc5.d
	@ln -sf ../init.d/$(RFP_SHELL) $(INST_ETC_D)/rc5.d/$(RFP_S_SHELL)

%.o: %.c $(RF_HEADER)
	@echo CC $@
	@$(CC) $(CFLAGS) -c -o $@ $< 2>&1 > /dev/null

.PHONY: all install clean release
