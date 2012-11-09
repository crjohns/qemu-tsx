QEMU_SRC = $(shell pwd)/..
QEMU_BUILD = $(shell pwd)/build
QEMU_INSTALL = $(shell pwd)/build/install
QEMU_BUILD_JOBS = 6

QEMU_USER = $(QEMU_INSTALL)/bin/qemu-x86_64
QEMU_SYSTEM = $(QEMU_INSTALL)/bin/qemu-system-x86_64
QEMU_FLAGS = 
QEMU_CPU ?= Haswell

QEMU_CONFIGURE_FLAGS = "--target-list=x86_64-softmmu,x86_64-linux-user"

CFLAGS = -g
 
TESTS := sse-test htm-test


all: $(TESTS)


clean:
	rm -f $(TESTS) *.o

$(TESTS) : %: %.o
	$(CXX) $< -o $@

%.o: %.c htm.h
	$(CXX) $< -c -o $@ $(CFLAGS)

test: install-qemu $(TESTS) $(foreach test, $(TESTS), $(test)-test)

%-test:
	$(QEMU_USER) $(QEMU_FLAGS) -cpu $(QEMU_CPU) ./$*


	


install-qemu: build-qemu
	make -C $(QEMU_BUILD) install

build-qemu: $(QEMU_BUILD)/qemu-configured
	make -C $(QEMU_BUILD) -j$(QEMU_BUILD_JOBS)

$(QEMU_BUILD)/qemu-configured: 
	mkdir -p $(QEMU_BUILD)
	cd $(QEMU_BUILD); \
		$(QEMU_SRC)/configure $(QEMU_CONFIGURE_FLAGS) --prefix=$(QEMU_INSTALL)
	touch $(QEMU_BUILD)/qemu-configured

