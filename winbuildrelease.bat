svn update
cd src
make clean
make EXT=JETI
mv er9x.hex ../er9x-jeti.hex
make clean
make EXT=FRSKY
mv er9x.hex ../er9x-frsky.hex
make clean
make
mv er9x.hex ../er9x.hex
make clean 
