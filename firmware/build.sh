       
mkdir -p build
cd build
cmake ..
make -j4
cp adc-pulse.uf2 ../pigo_mux.uf2
