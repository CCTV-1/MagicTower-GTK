strace ./MagicTower.exe | grep \ loaded\ D:\\\\MSYS2\\\\mingw64\\\\bin | sort | awk '{print $5}' | sed 's/\\/\//g' | xargs -i cp {} ./
