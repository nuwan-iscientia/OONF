###########################################
#### Default Application configuration ####
###########################################

# set name of program the executable and library prefix
set (OONF_APP "Minimal OONF Daemon")
set (OONF_EXE oonf)

# setup custom text before and after default help message
set (OONF_HELP_PREFIX "OONF Daemon daemon\\n")
set (OONF_HELP_SUFFIX "Visit http://www.olsr.org\\n")

# setup custom text after version string
set (OONF_VERSION_TRAILER "Visit http://www.olsr.org\\n")

# set to true to stop application running without root privileges (true/false)
set (OONF_NEED_ROOT false)

# name of default configuration handler
set (OONF_APP_DEFAULT_CFG_HANDLER Compact)

#################################
####  set static subsystems  ####
#################################

set (OONF_STATIC_PLUGINS cfg_compact)
