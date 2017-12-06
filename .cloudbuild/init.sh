#!/usr/bin/env bash

# Attempt to run as user
ls -l /workspace
ls -l /builder
ls -l /builder/home

export USER=circleci
export HOME=/workspace
sudo chown -R $USER /workspace
sudo chown -R $USER /builder/home

make build_envoy

