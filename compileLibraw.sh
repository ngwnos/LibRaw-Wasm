wget https://www.libraw.org/data/LibRaw-0.21.3.tar.gz
tar xzvf LibRaw-0.21.3.tar.gz
cd LibRaw-0.21.3
emconfigure ./configure CXXFLAGS="-pthread" LDFLAGS=" -lpthread " --disable-shared --disable-examples
emmake make -j24

rm -R ../lib
rm -R ../libraw
cp -R lib ../
cp -R libraw ../