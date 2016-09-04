# How to Install

## Installing from source

### Pre-requisites

  * cmake: ``sudo apt-get install cmake``
  * build-essentials: ``sudo apt-get install build-essential``


## Compiling
  * ``cmake .``
  * ``make``

## Configuring OLSRv2

## Starting OLSRv2

Assuming your interfaces you want olsrd2 to listen on are ``eth0, wlan0 and lo`` you could start it like this:

  * ``sudo ./olsrd2_static eth0 wlan0 lo``

You won't see much output though. You can enable more output (by default it comes on stderr) via:

  * ``sudo ./olsrd2_static --schema=log.debug``

This shows you which debug schemas exist. Let's say we are interested in the neighbor disovery protocol ("Hello message"). We can set this subsystem to debug level via:

  * ``sudo ./olsrd2_static --set=log.debug=layer2 eth0 wlan0  lo``

You should now see some output which shows you the info from the hello packets.




  
