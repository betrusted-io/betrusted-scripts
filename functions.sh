#!/bin/sh

ensure_pigpiod() {
  local LISTENING TRIES
  if ! which socat pigpiod pigs >/dev/null; then
    echo Please install all of the following packages:
    echo sudo apt install socat pigpiod pigpio-tools
    exit 1
  fi
  LISTENING=0
  socat OPEN:/dev/null TCP6:localhost:8888 2>/dev/null ||
    socat OPEN:/dev/null TCP4:localhost:8888 2>/dev/null && LISTENING=1
  if [ $LISTENING -eq 1 ]; then
    if [ "$(systemctl show pigpiod.service | grep ^ActiveState=)" = ActiveState=active ]; then
      return
    else
      echo "Something else appears to be listening on port 8888! Please fix."
      exit 1
    fi
  fi
  if ! sudo systemctl start pigpiod.service; then
    echo "Could not start pigpiod.service; please fix."
    exit 1
  fi
  TRIES=15
  while [ $TRIES -gt 0 ]; do
    socat OPEN:/dev/null TCP6:localhost:8888,retry=1,interval=0.1 2>/dev/null && break
    socat OPEN:/dev/null TCP4:localhost:8888,retry=1,interval=0.1 2>/dev/null && break
    TRIES=$((TRIES - 1))
  done
  if [ $TRIES -eq 0 ]; then
    echo Timed out waiting for pigpiod to start
    exit 1
  fi
}
