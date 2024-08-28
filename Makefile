SOURCES := cluster.cpp Gauge.cpp RectGauge.cpp RoundGauge.cpp DigitalGauge.cpp RaylibHelper.cpp Label.cpp logging.cpp
CXX = g++
CXXFLAGS = -std=c++0x -Wall -pedantic-errors -g

INCLUDEFLAGS := -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads
OBJS = $(SOURCES:.cpp=.o)

LIBFLAGS := -lraylib -lpthread -lGL -lm -ldl -lgbm -ldrm -lEGL -lzmq -lzmqpp -lsodium

cluster: $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDEFLAGS) $(OBJS) -o $@ $(LIBFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) *.o *~.
