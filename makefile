GST_FLAGS=$(shell pkg-config --cflags --libs gstreamer-1.0)
GLIBMM_FLAGS=$(shell pkg-config --cflags --libs glibmm-2.4)
GIOMM_FLAGS=$(shell pkg-config --cflags --libs giomm-2.4)
GTKMM_FLAGS=$(shell pkg-config --cflags --libs gtkmm-3.0)
SQLITE3_FLAGS=$(shell pkg-config --cflags --libs sqlite3)
LUA_FLAGS=$(shell pkg-config --cflags --libs lua)
GCC_CPP_OPTION=-Wall -Wextra -Wpedantic -std=gnu++14 -O3
GCC_CPP_PROFILE_OPTION=-Wall -Wextra -Wpedantic -std=gnu++14 -O0 -g3 -pg -m64

MagicTower : ./src/game.cpp database.o music.o game_event.o game_window.o env_var.o
	g++ ./src/game.cpp database.o music.o game_event.o game_window.o env_var.o $(GCC_CPP_OPTION) $(GTKMM_FLAGS) $(GST_FLAGS) $(SQLITE3_FLAGS) $(LUA_FLAGS) -o ./build/bin/MagicTower
env_var.o : ./src/env_var.cpp ./src/env_var.h
	g++ ./src/env_var.cpp $(GLIBMM_FLAGS) $(GIOMM_FLAGS) -c -o env_var.o
database.o : ./src/database.cpp ./src/database.h
	g++ ./src/database.cpp $(GCC_CPP_OPTION) $(SQLITE3_FLAGS) -c -o database.o
music.o : ./src/music.cpp ./src/music.h
	g++ ./src/music.cpp $(GCC_CPP_OPTION) $(GST_FLAGS) -c -o music.o
game_window.o : ./src/game_window.cpp ./src/game_window.h
	g++ ./src/game_window.cpp $(GCC_CPP_OPTION) $(GTKMM_FLAGS) $(GLIBMM_FLAGS) $(LUA_FLAGS) -c -o game_window.o
game_event.o : ./src/game_event.cpp ./src/game_event.h
	g++ ./src/game_event.cpp $(GCC_CPP_OPTION) $(GIOMM_FLAGS) -c -o game_event.o
ProfileTest : ./src/game.cpp ./src/database.cpp ./src/music.cpp ./src/game_event.cpp ./src/game_window.cpp
	g++ ./src/game.cpp ./src/database.cpp ./src/music.cpp ./src/game_event.cpp ./src/game_window.cpp ./src/env_var.cpp $(GCC_CPP_PROFILE_OPTION) $(GTKMM_FLAGS) $(GST_FLAGS) $(SQLITE3_FLAGS) $(JANSSON_FLAGS) $(LUA_FLAGS) -o ./build/bin/MagicTower
	./build/bin/MagicTower &&\
	gprof ./build/bin/MagicTower gmon.out > analysis.txt
install :
	cp MagicTower /usr/opt/magictower/MagicTower
uninstall :
	rm /usr/opt/magictower/MagicTower
clean :
	-rm ./build/bin/MagicTower
	-rm database.o
	-rm music.o
	-rm game_event.o
	-rm game_window.o
	-rm env_var.o
