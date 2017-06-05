LIBMOD_DIR = $(TARGET_DIR)/usr/lib/amplayer
LIB_DST := $(LIBMOD_DIR)/libdash_mod.so

ifeq ($(ARCH),arm)
LIB_SRC := libdash_mod_arm.so
else
LIB_SRC := libdash_mod_aarch64.so
endif

all:
	mkdir $(LIBMOD_DIR) -p
	-install $(LIB_SRC) $(LIB_DST) -m 0664

clean:
	rm $(LIB_DST) -rf
