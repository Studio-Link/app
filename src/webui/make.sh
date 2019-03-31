#!/bin/bash -ex

npm run dev
rm dist/fonts/fa-brands* || true
rm dist/fonts/*.svg || true
mkdir -p headers
xxd -i dist/index.html > headers/index_html.h
find dist/fonts -type f | xargs -I{} xxd -i {} > headers/fonts.h
xxd -i dist/app.css > headers/css.h
xxd -i dist/app.js > headers/js.h
xxd -i dist/images/logo.svg > headers/logo.h
mkdir -p ../modules/webapp/assets/
cp -a headers/*.h ../modules/webapp/assets/
