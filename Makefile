INCLUDEFLAGS=-I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -fPIC

cluster-demo: cluster-demo.cpp RoundGauge.cpp Gauge.cpp OpenVGHelper.cpp
	@rm -f $@
	g++ -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads cluster-demo.cpp Gauge.cpp RoundGauge.cpp OpenVGHelper.cpp -o $@ -lshapes
