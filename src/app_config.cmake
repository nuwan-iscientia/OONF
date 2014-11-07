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

# choose if framework should be linked static or dynamic
set (OONF_FRAMEWORD_DYNAMIC false)

# set to true to stop application running without root privileges (true/false)
set (OONF_NEED_ROOT true)

# name of default configuration handler
set (OONF_APP_DEFAULT_CFG_HANDLER Compact CACHE STRING
     "Name of default configuration handler")

#################################
####  set static subsystems  ####
#################################

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
                         os_system)
