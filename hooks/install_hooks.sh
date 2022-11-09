#!/bin/bash

cd $(dirname $(realpath "$0"))
cp ./pre-commit ../.git/hooks/pre-commit
