#!/bin/bash
if [ ! -f "./profile.txt" ]; then
    cp -R defconf/* ./
fi
./picklelauncher