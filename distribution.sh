#!/bin/bash 
set -e

if [ $# != 1 ];then
    exit 1;
fi

load_info=$(strace $1);
load_info=$(grep "loaded D:\\\\MSYS2\\\\" <<< $load_info)
load_info=$(sort <<< $load_info)
load_info=$(awk '{print $5}' <<< $load_info)
load_info=$(sed 's/\\/\//g' <<< $load_info)
xargs -i cp {} ./ <<< $load_info