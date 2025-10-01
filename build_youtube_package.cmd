@echo off
rmdir /s /q .\build
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64 -DCMAKE_BUILD_TYPE=Release -DYOUTUBE_LIVE_STREAMER=ON -DMICROSOFT_STORE_BUILD=ON
cmake --build . --target package --config Release
