QEMU_SRC = $(shell pwd)/..
QEMU_BUILD = $(shell pwd)/build
QEMU_INSTALL = $(shell pwd)/build/install
QEMU_BUILD_JOBS = 4

QEMU_USER = $(QEMU_INSTALL)/bin/qemu-x86_64
QEMU_SYSTEM = $(QEMU_INSTALL)/bin/qemu-system-x86_64
QEMU_FLAGS =  -txstep
QEMU_CFLAGS = -g
QEMU_CPU ?= Haswell

QEMU_CONFIGURE_FLAGS = --target-list=x86_64-softmmu,x86_64-linux-user

CFLAGS := -g
CXXFLAGS := -g
 
TESTS := sse-test htm-test
EXTRA_PROGS := threaded
EXTRA_LIBS := -lpthread


all: $(TESTS) $(EXTRA_PROGS)


clean:
	rm -f $(TESTS) *.o

$(TESTS) : %: %.o
	$(CXX) $< -o $@

$(EXTRA_PROGS) : %: %.o
	$(CXX) $< -o $@ $(EXTRA_LIBS)

%.o: %.c htm.h
	$(CXX) $< -c -o $@ $(CFLAGS)

test: install-qemu $(TESTS) $(foreach test, $(TESTS), $(test)-test)

%-debug: install-qemu
	@echo "flags: $(QEMU_FLAGS) -cpu $(QEMU_CPU) ./$*"
	gdb $(QEMU_USER)

%-test: install-qemu
	$(QEMU_USER) $(QEMU_FLAGS) -cpu $(QEMU_CPU) ./$*

install-qemu: build-qemu
	make -C $(QEMU_BUILD) install

build-qemu: configure-qemu
	make -C $(QEMU_BUILD) -j$(QEMU_BUILD_JOBS)

configure-qemu: $(QEMU_BUILD)/qemu-configured

$(QEMU_BUILD)/qemu-configured: $(QEMU_SRC)/configure
	mkdir -p $(QEMU_BUILD)
	cd $(QEMU_BUILD); \
		CFLAGS="$(QEMU_CFLAGS)" $(QEMU_SRC)/configure $(QEMU_CONFIGURE_FLAGS) --prefix=$(QEMU_INSTALL)
	touch $(QEMU_BUILD)/qemu-configured

