set -euxo pipefail
isysroot=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
opt_level=-O0
debug_symbols=-g
# make radsan
# make -j12 compiler-rt
# make -j12 clang
# make -j12 opt
# ========== COMPILE ONLY IR ========== #
# ./bin/clang++  \
#   ${opt_level} \
#   -fsanitize=realtime \
#   -isysroot ${isysroot} \
#   -I/Users/david/Desktop/toydl/build/dist/include \
#   -o test_realtime.ll \
#   ${debug_symbols} \
#   ../test_realtime.cpp
#
# cat test_realtime.ll | grep attributes
# cat test_realtime.ll | grep radsan
#
# ========== RUN ONLY OPT ========== #
#.bin/opt -passes=radsan test_realtime.ll -S -o test_realtime_opt.ll
#
# ========== LINK ONLY ========== #
#./bin/clang++ \
#   ${opt_level} \
#   -isysroot ${isysroot}
#   ${debug_symbols} \
#   -fsanitize=realtime test_realtime_opt.ll -o ./test_realtime
#
# ========== FULL TOOLCHAING WITH THIRD-PARTY LIB ========== #
# ./bin/clang++  \
#   ${opt_level} \
#   ${debug_symbols} \
#   -fsanitize=realtime \
#   -isysroot ${isysroot} \
#   -I/Users/david/Desktop/toydl/build/dist/include \
#   -L/Users/david/Desktop/toydl/build/dist/lib \
#   -rpath /Users/david/Desktop/toydl/build/dist/lib \
#   -ltoys \
#   -o test_realtime.ll \
#   ../test_realtime.cpp
#
# ========== FULL TOOLCHAING WITHOUT THIRD-PARTY LIB ========== #
./bin/clang++  \
  ${opt_level} \
  ${debug_symbols} \
  -fsanitize=realtime \
  -isysroot ${isysroot} \
  -ltoys \
  -o test_realtime.ll \
  ../test_realtime.cpp

./test_realtime
