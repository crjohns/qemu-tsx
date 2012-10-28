QEMU_BUILD = ../build
QEMU_INSTALL = ../build/install
QEMU_BUILD_JOBS = 6

QEMU_USER = $(QEMU_INSTALL)/bin/qemu-x86_64
QEMU_SYSTEM = $(QEMU_INSTALL)/bin/qemu-system-x86_64

QEMU_CPU ?= Haswell

TESTS := htm-test


all: $(TESTS)


clean:
	rm -f $(TESTS) *.o

$(TESTS) : %: %.o
	$(CXX) $< -o $@

%.o: %.c
	$(CXX) $< -c -o $@ -g

test: install-qemu $(TESTS) $(foreach test, $(TESTS), $(test)-test)

%-test:
	$(QEMU_USER) -cpu $(QEMU_CPU) ./$*


	


install-qemu: build-qemu
	make -C $(QEMU_BUILD) install

build-qemu:
	make -C $(QEMU_BUILD) -j$(QEMU_BUILD_JOBS)
