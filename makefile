GST_FLAGS=$(shell pkg-config --cflags --libs gstreamer-1.0)
GLIBMM_FLAGS=$(shell pkg-config --cflags --libs glibmm-2.4)
GIOMM_FLAGS=$(shell pkg-config --cflags --libs giomm-2.4)
GTKMM_FLAGS=$(shell pkg-config --cflags --libs gtkmm-3.0)
SQLITE3_FLAGS=$(shell pkg-config --cflags --libs sqlite3)
LUA_FLAGS=$(shell pkg-config --cflags --libs lua)
CXX=g++
CPP_OPTION=-Wall -Wextra -Wpedantic -std=gnu++17 -O3
CPP_PROFILE_OPTION=-Wall -Wextra -Wpedantic -std=gnu++17 -O0 -g3 -pg -m64

MagicTower : ./src/game.cpp database.o music.o game_event.o game_window.o env_var.o resources.o
	$(CXX) ./src/game.cpp database.o music.o game_event.o game_window.o env_var.o resources.o $(CPP_OPTION) $(GTKMM_FLAGS) $(GST_FLAGS)\
		$(SQLITE3_FLAGS) $(LUA_FLAGS) -o ./MagicTower
env_var.o : ./src/env_var.cpp ./src/env_var.h ./src/tower.h ./src/hero.h
	$(CXX) ./src/env_var.cpp $(CPP_OPTION) $(GLIBMM_FLAGS) $(GIOMM_FLAGS) $(LUA_FLAGS) -c -o env_var.o
database.o : ./src/database.cpp ./src/database.h ./src/tower.h ./src/hero.h
	$(CXX) ./src/database.cpp $(CPP_OPTION) $(SQLITE3_FLAGS) $(LUA_FLAGS) -c -o database.o
music.o : ./src/music.cpp ./src/music.h
	$(CXX) ./src/music.cpp $(CPP_OPTION) $(GST_FLAGS) -c -o music.o
game_window.o : ./src/game_window.cpp ./src/game_window.h ./src/env_var.h ./src/game_event.h ./src/resources.h ./src/hero.h
	$(CXX) ./src/game_window.cpp $(CPP_OPTION) $(GTKMM_FLAGS) $(GLIBMM_FLAGS) $(LUA_FLAGS) -c -o game_window.o
game_event.o : ./src/game_event.cpp ./src/game_event.h ./src/env_var.h ./src/tower.h ./src/resources.h ./src/hero.h
	$(CXX) ./src/game_event.cpp $(CPP_OPTION) $(GIOMM_FLAGS) $(LUA_FLAGS)  -c -o game_event.o
resources.o : ./src/resources.cpp ./src/resources.h
	$(CXX) ./src/resources.cpp $(CPP_OPTION) $(GIOMM_FLAGS) $(GLIBMM_FLAGS)  -c -o resources.o
ProfileTest : ./src/game.cpp ./src/database.cpp ./src/music.cpp ./src/game_event.cpp ./src/game_window.cpp\
			./src/game_window.h ./src/env_var.h ./src/game_event.h ./src/resources.h ./src/hero.h ./src/tower.h\
			./src/resources.cpp ./src/resources.h
	$(CXX) ./src/*.cpp $(CPP_PROFILE_OPTION) $(GTKMM_FLAGS) $(GST_FLAGS) $(SQLITE3_FLAGS) $(LUA_FLAGS) -o ./MagicTower
install :
	mkdir -p /opt/magictower
	cp -r ./resources /opt/magictower/resources
	cp ./MagicTower /opt/magictower/MagicTower
uninstall :
	rm -rf /opt/magictower
clean :
	-rm MagicTower
	-rm database.o
	-rm music.o
	-rm game_event.o
	-rm game_window.o
	-rm resources.o
	-rm env_var.o
