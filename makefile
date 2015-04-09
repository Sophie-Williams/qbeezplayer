all: qbeezplay qbeezserver

CC = g++
CXXFLAGS = -O3

SOURCES = QBeezPlayer.cpp qbeezplay.cpp
SOURCES_SV = QBeezPlayer.cpp qbeezserver.cpp

OBJECTS := $(SOURCES:%.cpp=%.o)
OBJECTS_SV := $(SOURCES_SV:%.cpp=%.o)

qbeezserver: $(OBJECTS_SV)
	$(CC) $(CXXFLAGS) -o qbeezserver $(OBJECTS_SV)

qbeezplay: $(OBJECTS)
	$(CC) $(CXXFLAGS) -o qbeezplay $(OBJECTS)

%.o : %.cpp
	$(CC) $(CXXFLAGS) -o $@ -c $<

depend:
		makedepend -- $(CXXFLAGS) -- $(SOURCES) $(SOURCES_SV)


# DO NOT DELETE
