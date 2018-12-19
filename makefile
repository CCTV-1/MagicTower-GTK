GST_FLAGS=$(shell pkg-config --cflags --libs gstreamer-1.0)
GLIBMM_FLAGS=$(shell pkg-config --cflags --libs glibmm-2.4)
GTKMM_FLAGS=$(shell pkg-config --cflags --libs gtkmm-3.0)
SQLITE3_FLAGS=$(shell pkg-config --cflags --libs sqlite3)
JANSSON_FLAGS=$(shell pkg-config --cflags --libs jansson)
GCC_CPP_OPTION=-Wall -Wextra -Wpedantic -std=gnu++14 -O3
GCC_CPP_PROFILE_OPTION=-Wall -Wextra -Wpedantic -std=gnu++14 -O0 -g3 -pg -m64

MagicTower : ./src/game.cpp ./build/database.o ./build/music.o ./build/game_event.o
	g++ ./src/game.cpp ./build/database.o ./build/music.o ./build/game_event.o $(GCC_CPP_OPTION) $(GTKMM_FLAGS) $(GST_FLAGS) $(SQLITE3_FLAGS) $(JANSSON_FLAGS) -o ./build/bin/MagicTower
./build/database.o : ./src/database.cpp ./src/database.h
	g++ ./src/database.cpp $(GCC_CPP_OPTION) $(SQLITE3_FLAGS) -c -o ./build/database.o
./build/music.o : ./src/music.cpp ./src/music.h
	g++ ./src/music.cpp $(GCC_CPP_OPTION) $(GST_FLAGS) -c -o ./build/music.o
./build/game_event.o : ./src/game_event.cpp ./src/game_event.h
	g++ ./src/game_event.cpp $(GCC_CPP_OPTION) $(GLIBMM_FLAGS) $(JANSSON_FLAGS) -c -o ./build/game_event.o
ProfileTest : ./src/game.cpp ./src/database.cpp ./src/music.cpp ./src/game_event.cpp
	g++ ./src/game.cpp ./src/database.cpp ./src/music.cpp ./src/game_event.cpp $(GCC_CPP_PROFILE_OPTION) $(GTKMM_FLAGS) $(GST_FLAGS) $(SQLITE3_FLAGS) $(JANSSON_FLAGS) -o ./build/bin/MagicTower
	cd build/bin/ &&\
	./MagicTower &&\
	gprof MagicTower gmon.out > analysis.txt
install :
	cp ./build/MagicTower /usr/opt/magictower/MagicTower
uninstall :
	rm /usr/opt/magictower/MagicTower
clean :
	rm ./build/bin/MagicTower
	rm ./build/database.o
	rm ./build/music.o
	rm ./build/game_event.o
