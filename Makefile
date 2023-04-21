all: wavedump/src/wavedump

CAENUSBdrvB-1.5.4/CAENUSBdrvB.o:
	$(MAKE) -C CAENUSBdrvB-1.5.4

install-deps: CAENUSBdrvB-1.5.4/CAENUSBdrvB.o
	cd CAENVMELib-3.3.0/lib && ./install_x64
	cd CAENComm-1.5.0/lib && ./install_x64
	cd CAENDigitizer-2.17.0 && ./install_64
	$(MAKE) -C CAENUSBdrvB-1.5.4 install

wavedump/src/wavedump: wavedump/src/Makefile wavedump/src/wavedump.c
	$(MAKE) -C wavedump

install: CAENUSBdrvB-1.5.4/CAENUSBdrvB.o
	$(MAKE) -C wavedump install
	$(MAKE) -C python install
