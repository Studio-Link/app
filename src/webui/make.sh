#!/bin/bash -ex

#npm run dev
npm run prod
mkdir -p headers
xxd -i dist/index.html > headers/index_html.h
xxd -i dist/app.css > headers/css.h
xxd -i dist/app.js > headers/js.h
find dist/images -type f | xargs -I{} xxd -i {} > headers/images.h
mkdir -p ../modules/webapp/assets/
cp -a headers/*.h ../modules/webapp/assets/
