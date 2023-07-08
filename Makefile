SOURCES := cluster-demo.cpp RoundGauge.cpp Gauge.cpp DigitalGauge.cpp RaylibHelper.cpp

#INCLUDEFLAGS=-I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -fPIC
INCLUDEFLAGS := -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads

LIBFLAGS := -lraylib -lpthread -lGL -lm -ldl -lgbm -ldrm -lEGL -lwiringPi

cluster-demo: $(SOURCES)
	@rm -f $@
	#g++ 
	g++ $(INCLUDEFLAGS) $(SOURCES) -o $@ $(LIBFLAGS)
