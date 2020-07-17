#!/bin/sh

ps -ef | grep udpserver/udpserver | grep -v grep
if [ $? -ne 0 ] ; then
  echo "restart udpserver process..."
  /weike/udpserver/udpserver &
else
  echo "udpserver is running..."
fi
