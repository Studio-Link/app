#!/bin/bash -ex
source dist/lib/versions.sh
source dist/lib/functions.sh

sl_prepare
sl_build_webui

zip -r $artifacts/webui.zip webui/headers
