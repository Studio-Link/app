#!/bin/bash -ex
source dist/lib/versions.sh
source dist/lib/functions.sh

sl_prepare
sl_build_webui

s3_path="s3_upload/$GIT_BRANCH/$version_t"
mkdir -p $s3_path
zip -r $s3_path/webui webui/headers 
