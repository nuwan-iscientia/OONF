###########################################
#### Default Application configuration ####
###########################################

# set name of program the executable and library prefix
set (OONF_APP OLSRd2)
set (OONF_EXE olsrd2)

# setup custom text before and after default help message
set (OONF_HELP_PREFIX "OLSRv2 routing agent\\n")
set (OONF_HELP_SUFFIX "Visit http://www.olsr.org\\n")

# setup custom text after version string
set (OONF_VERSION_TRAILER "Visit http://www.olsr.org\\n")

# set to true to stop application running without root privileges (true/false)
set (OONF_NEED_ROOT true)

# name of default configuration handler
set (OONF_APP_DEFAULT_CFG_HANDLER Compact)

#################################
####  set static subsystems  ####
#################################

IF (NOT OONF_STATIC_PLUGINS)
    set (OONF_STATIC_PLUGINS class
                             clock
                             duplicate_set
                             interface
                             layer2
                             packet_socket
                             rfc5444
                             socket
                             stream_socket
                             telnet
                             timer
                             viewer
                             os_clock
                             os_net
                             os_routing
                             os_system
                             layer2info
                             nl80211_listener
                             systeminfo
                             nhdp
                             ff_dat_metric
                             link_config
                             neighbor_probing
                             nhdpinfo
                             olsrv2
                             olsrv2info
                             )
ENDIF (NOT OONF_STATIC_PLUGINS)
