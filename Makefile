MYLIB := lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/signal.cpp lib/utils.cpp lib/Pitch.cpp

HEADERS := lib/WavReader.hpp lib/Timing.hpp lib/ReadAudio.hpp lib/BmpReader.h lib/signal.hpp lib/utils.hpp lib/Pitch.hpp
HEADERS += Landmark.hpp Database.hpp PitchDatabase.hpp

CXXFLAGS := -O3 -fopenmp

ifeq ($(OS),Windows_NT)
	LIBS += -lws2_32
else
	LIBS +=
endif

builder: builder.cpp Landmark.cpp $(MYLIB) Makefile $(HEADERS)
	$(CXX) $(CXXFLAGS) builder.cpp Landmark.cpp $(MYLIB) -o $@ $(LIBS)

matcher: matcher.cpp Landmark.cpp Database.cpp $(MYLIB) Makefile $(HEADERS)
	$(CXX) $(CXXFLAGS) matcher.cpp Landmark.cpp  Database.cpp $(MYLIB) -o $@ $(LIBS)

matchServer: matchServer.cpp Landmark.cpp Database.cpp $(MYLIB) Makefile $(HEADERS)
	$(CXX) $(CXXFLAGS) matchServer.cpp Landmark.cpp Database.cpp $(MYLIB) -o $@ $(LIBS)

qbshServer: qbshServer.cpp PitchDatabase.cpp $(MYLIB) Makefile $(HEADERS)
	$(CXX) $(CXXFLAGS) qbshServer.cpp lib/Pitch.cpp PitchDatabase.cpp $(MYLIB) -o $@ $(LIBS)
