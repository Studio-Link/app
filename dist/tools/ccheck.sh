#!/bin/bash -e

dist/tools/ccheck.py src/core
dist/tools/ccheck.py src/standalone
# lv2.h is vendored (upstream style)
dist/tools/ccheck.py -e lv2.h src/plugin
dist/tools/ccheck.py src/modules/slaudio
dist/tools/ccheck.py src/modules/slogging
dist/tools/ccheck.py src/modules/webapp
dist/tools/ccheck.py src/modules/effect
