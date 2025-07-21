# Build Engine with Qt5/6 support

cmake -S . -B build
make -C build -j8

# Build Engine without Qt5/6 support

cmake -S . -B build -DQT_DEACTIVATED=ON
make -C build -j8