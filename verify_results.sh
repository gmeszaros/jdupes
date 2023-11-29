#!/bin/bash

[[ -z "$@" ]] && echo "Specify options and dirs" && exit 1

echo -n "Installed: "
jdupes $@ 2>/dev/null | sort -g | md5sum
echo -n "Built:     "
./jdupes $@ 2>/dev/null | sort -g | md5sum
