all: wavedump-3.10.3/src/wavedump

CAENUSBdrvB-1.5.4/CAENUSBdrvB.o:
	$(MAKE) -c CAENUSBdrvB-1.5.4

install-deps: CAENUSBdrvB-1.5.4/CAENUSBdrvB.o
	cd CAENVMELib-3.3.0/lib && ./install_x64
	cd CAENComm-1.5.0/lib && ./install_x64
	cd CAENDigitizer-2.17.0 && ./install_64
	$(MAKE) -C CAENUSBdrvB-1.5.4 install

wavedump-3.10.3/src/wavedump/src/Makefile:
	cd wavedump-3.10.3 && ./configure

wavedump-3.10.3/src/wavedump: wavedump-3.10.3/src/Makefile
	$(MAKE) -C wavedump-3.10.3

install: CAENUSBdrvB-1.5.4/CAENUSBdrvB.o
	$(MAKE) -C wavedump-3.10.3 install
