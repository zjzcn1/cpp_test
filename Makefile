BUILD_DIR = ./build

THREAD_NUM = $(shell cat /proc/cpuinfo | grep "processor" | wc -l)

THREAD_NUM := $(strip $(THREAD_NUM))
ifeq ($(THREAD_NUM), 0)
    THREAD_NUM = 4
endif

all: build
	cd $(BUILD_DIR);  make -j$(THREAD_NUM)
clean:
	rm -rf $(BUILD_DIR)
build:
	mkdir $(BUILD_DIR);cd $(BUILD_DIR); cmake ..
