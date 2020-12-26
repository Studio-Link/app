sl_extra_lflags="-L ../opus -L ../my_include "
sl_extra_modules="alsa jack slaudio slogging dtls_srtp menu"
make_opt="-j4 SYSROOT_ALT=\"../3rdparty\""

if [ "$1" == "" ]; then
export CC=gcc
make $make_opt RELEASE=1 LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 g722 turn stun uuid auloop webapp menu $sl_extra_modules" \
    EXTRA_CFLAGS="-I ../my_include" \
    EXTRA_LFLAGS="$sl_extra_lflags -L ../openssl"
fi

if [ "$1" == "assembler" ]; then
export CC=gcc
make $make_opt RELEASE=1 LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 g722 turn stun uuid auloop webapp menu $sl_extra_modules" \
    EXTRA_CFLAGS="-I ../my_include -save-temps=obj" \
    EXTRA_LFLAGS="$sl_extra_lflags -L ../openssl"
fi

if [ "$1" == "stack" ]; then
export CC=gcc
make $make_opt RELEASE=1 LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 g722 turn stun uuid auloop webapp menu $sl_extra_modules" \
    EXTRA_CFLAGS="-I ../my_include -Wstack-usage=4096 -fstack-usage" \
    EXTRA_LFLAGS="$sl_extra_lflags -L ../openssl"
fi

if [ "$1" == "lib" ]; then
make $make_opt USE_OPENSSL="yes" LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 turn stun uuid auloop webapp effect" \
    EXTRA_CFLAGS="-I ../my_include -DSLPLUGIN" \
    EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a
fi

if [ "$1" == "scan" ]; then
export CC=clang
scan-build make $make_opt RELEASE=1 LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 g722 turn stun uuid auloop webapp menu $sl_extra_modules" \
    EXTRA_CFLAGS="-I ../my_include" \
    EXTRA_LFLAGS="$sl_extra_lflags -L ../openssl"
fi

if [ "$1" == "llvm" ]; then
export CC=clang
make $make_opt RELEASE=1 LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 g722 turn stun uuid auloop webapp menu $sl_extra_modules" \
    EXTRA_CFLAGS="-I ../my_include" \
    EXTRA_LFLAGS="$sl_extra_lflags -L ../openssl -fsanitize=address -fno-omit-frame-pointer"

# ASAN_OPTIONS=fast_unwind_on_malloc=0 ./baresip
fi
