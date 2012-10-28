QEMU_BUILD = ../build
QEMU_INSTALL = ../build/install
QEMU_BUILD_JOBS = 6

QEMU_USER = $(QEMU_INSTALL)/bin/qemu-x86_64
QEMU_SYSTEM = $(QEMU_INSTALL)/bin/qemu-system-x86_64

QEMU_CPU ?= Haswell


default: htm-test


clean: 
	rm -f htm-test

htm-test: htm-test.c htm.h
	gcc $^ -o $@ -g


test: htm-test install-qemu
	$(QEMU_USER) -cpu $(QEMU_CPU) ./htm-test
	


install-qemu: build-qemu
	make -C $(QEMU_BUILD) install

build-qemu:
	make -C $(QEMU_BUILD) -j$(QEMU_BUILD_JOBS)
