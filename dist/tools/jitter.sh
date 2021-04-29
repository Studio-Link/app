#!/bin/bash

dev="wlp4s0"

function setup() {
    sudo ip link add ifb1 type ifb || :
    sudo ip link set ifb1 up
    sudo tc qdisc add dev $dev handle ffff: ingress
    sudo tc filter add dev $dev parent ffff: u32 match u32 0 0 action mirred egress redirect dev ifb1
}

function delay() {
    if [[ -z $1 ]]; then
        jitter=100
    else
        jitter=$1
    fi
    sudo tc qdisc add dev ifb1 root netem delay 10ms
    for i in {1..100}
    do
        sudo tc qdisc change dev ifb1 root netem delay ${jitter}ms
        echo "delay active"
        sleep 1
        sudo tc qdisc change dev ifb1 root netem delay 10ms
#        sudo tc qdisc del dev ifb1 root
        echo "delay inactive"
        sleep 5
    done
}

function reorder() {
    if [[ -z $1 ]]; then
        jitter=100
    else
        jitter=$1
    fi
    sudo tc qdisc add dev ifb1 root netem delay ${jitter}ms 10ms
    echo "reorder active"
    sleep 10
    sudo tc qdisc del dev ifb1 root
    echo "reorder inactive"
    sleep 5
}

if [[ "$1" == "setup" ]]; then
    setup
    exit 0
fi

if [[ "$1" == "delay" ]]; then
    delay $2
    exit 0
fi

if [[ "$1" == "reorder" ]]; then
    reorder $2
    exit 0
fi
