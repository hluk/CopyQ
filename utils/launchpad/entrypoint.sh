#!/bin/bash
set -e

gpg2 --import /tmp/gpg.key

exec bash -i
