INCLUDEFLAGS=-I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -fPIC

cluster-demo: cluster-demo.c RoundGauge.c
	@rm -f $@
	gcc -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads cluster-demo.c RoundGauge.c -o $@ -lshapes
