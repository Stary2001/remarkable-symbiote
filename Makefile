CC = arm-linux-gnueabihf-gcc
CXX = arm-linux-gnueabihf-g++
CPPFLAGS = -pthread -lpthread -fdata-sections -ffunction-sections -Wl,--gc-sections

EXE=symbiote
FILES=src/main.cpp src/xournal/LineStyle.cpp src/xournal/Point.cpp src/xournal/Element.cpp src/xournal/Stroke.cpp src/xournal/ShapeRecognizer.cpp src/xournal/CircleRecognizer.cpp src/xournal/RecoSegment.cpp src/xournal/ShapeRecognizerResult.cpp src/xournal/Inertia.cpp
O_FILES=$(patsubst src/%.cpp,build/%.o,$(FILES))

# -Wno-psabi shuts up "blah was changed in gcc 7.1" warnings.. from standard headers
export CXXFLAGS += -std=c++17 -Wno-psabi
export CPPFLAGS += -I../../xournalpp/src \
-I../../xournalpp/src/model \
-I../../xournalpp/src/control/shaperecognizer/ \
-I../../xournalpp/src/util \
-Irmkit/src/build \
-Iinclude \
-Iinclude/xournal \
-DREMARKABLE=1

#ASSET_DIR=assets/

# technically bad because make symbiote doesn't work but it works for me
all: rmkit/src/build/rmkit.h rmkit/src/build/stb.arm.o build/symbiote

build/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $^

build/symbiote: $(O_FILES) rmkit/src/build/stb.arm.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^

docker:
	docker build --tag rmkit:latest rmkit
	bash scripts/docker_build.sh

clean:
	rm -f rmkit/src/build/rmkit.h rmkit/src/build/stb.arm.o
	rm -f $(O_FILES)

rmkit/src/build/rmkit.h rmkit/src/build/stb.arm.o:
	make -C rmkit/src/rmkit

.PHONY: docker clean