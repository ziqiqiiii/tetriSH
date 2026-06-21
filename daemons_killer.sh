#!/bin/bash
ps aux | grep "dspawn\|dplant"
pkill -f dspawn
pkill -f dplant
