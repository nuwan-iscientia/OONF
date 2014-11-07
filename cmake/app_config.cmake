###########################
#### API configuration ####
###########################

# remove help texts from application, core-api and plugins
set (OONF_REMOVE_HELPTEXT false CACHE BOOL
     "Set if you want to remove the help texts from application to reduce size")

# name of default configuration handler
set (OONF_APP_DEFAULT_CFG_HANDLER Compact CACHE STRING
     "Name of default configuration handler")

###########################################
#### Default Application configuration ####
###########################################

# set name of program the executable and library prefix
set (OONF_APP OLSRd2)
set (OONF_EXE olsrd2)

# setup custom text before and after default help message
set (OONF_HELP_PREFIX "OLSRv2 routing agent\\\\n")
set (OONF_HELP_SUFFIX "")

# setup custom text after version string
set (OONF_VERSION_TRAILER "Visit http://www.olsr.org\\\\n")

# set static plugins (list of plugin names, separated by space)
set (OONF_CUSTOM_STATIC_PLUGINS "" CACHE STRING
     "Space separated list of plugins to compile into application")

# choose if framework should be linked static or dynamic
set (OONF_FRAMEWORD_DYNAMIC false CACHE BOOL
     "Compile the application with dynamic libraries instead of linking everything static")

# set to true to stop application running without root privileges (true/false)
set (OONF_NEED_ROOT true)

# set to true to link packetbb API to application
set (OONF_NEED_PACKETBB true)

##############################
#### Handle default cases ####
##############################

# use default static plugins if custom variable not set
IF (NOT OONF_CUSTOM_STATIC_PLUGINS OR OONF_CUSTOM_STATIC_PLUGINS STREQUAL "")
	set (OONF_STATIC_PLUGINS oonf_class
	                         oonf_clock
	                         oonf_duplicate_set
	                         oonf_interface
	                         oonf_layer2
	                         oonf_packet_socket
	                         oonf_rfc5444
	                         oonf_socket
	                         oonf_stream_socket
	                         oonf_telnet
	                         oonf_timer
	                         oonf_viewer
	                         os_clock
	                         os_net
	                         os_routing
	                         os_system)
ELSE ()
	set (OONF_STATIC_PLUGINS "${OONF_CUSTOM_STATIC_PLUGINS}")
ENDIF ()
