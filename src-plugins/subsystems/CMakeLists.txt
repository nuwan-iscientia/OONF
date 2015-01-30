# add static library for testing
add_subdirectory(rfc5444)

# create plugins for single-file (source plus header) subsystems
SET(SINGLE_FILE_NAMES    class
                         clock
                         duplicate_set
                         http
                         interface
                         layer2
                         packet_socket
                         socket
                         stream_socket
                         telnet
                         timer
                         viewer)

foreach(name ${SINGLE_FILE_NAMES})
    oonf_create_plugin("${name}" "oonf_${name}.c" "oonf_${name.h}" "")
endforeach(name)

# generate rfc5444 plugin
SET(RFC5444_SOURCE  oonf_rfc5444.c
                    rfc5444/rfc5444.c
                    rfc5444/rfc5444_context.c
                    rfc5444/rfc5444_iana.c
                    rfc5444/rfc5444_msg_generator.c
                    rfc5444/rfc5444_pkt_generator.c
                    rfc5444/rfc5444_print.c
                    rfc5444/rfc5444_reader.c
                    rfc5444/rfc5444_tlv_writer.c
                    rfc5444/rfc5444_writer.c)
SET(RFC5444_INCLUDE oonf_rfc5444.h
                    rfc5444/rfc5444_context.h
                    rfc5444/rfc5444.h
                    rfc5444/rfc5444_iana.h
                    rfc5444/rfc5444_print.h
                    rfc5444/rfc5444_reader.h
                    rfc5444/rfc5444_tlv_writer.h
                    rfc5444/rfc5444_writer.h)
oonf_create_plugin("rfc5444" "${RFC5444_SOURCE}" "${RFC5444_INCLUDE}" "")

# generate the os-specific plugins
# TODO: add BSD and WIN32

SET(OS_CLOCK_INCLUDE     os_clock.h)
SET(OS_SOCKET_INCLUDE    os_socket.h)
SET(OS_ROUTING_INCLUDE   os_routing.h)
SET(OS_INTERFACE_INCLUDE os_system.h)
SET(OS_SYSTEM_INCLUDE    os_system.h)
SET(OS_VIF_INCLUDE       os_vif.h)

IF(LINUX)
    SET(OS_CLOCK_SOURCE      os_linux/os_clock_linux.c)
    SET(OS_CLOCK_INCLUDE     ${OS_CLOCK_INCLUDE}
                             os_linus/os_clock_linux.h)
    
    SET(OS_SOCKET_SOURCE     os_generic/os_socket_generic_configsocket.c
                             os_generic/os_socket_generic_getsocket.c
                             os_generic/os_socket_generic_getrawsocket.c
                             os_generic/os_socket_generic_join_mcast.c
                             os_generic/os_socket_generic_set_nonblocking.c
                             os_generic/os_socket_generic_set_dscp.c
                             os_linux/os_socket_linux.c
                             os_linux/os_socket_linux_skip_rawsocket_prefix.c)
    SET(OS_SOCKET_INCLUDE    ${OS_SOCKET_INCLUDE}
                             os_linux/os_net_linux.h)

    SET(OS_ROUTING_SOURCE    os_generic/os_routing_generic_rt_to_string.c
                             os_linux/os_routing_linux.c)
    SET(OS_ROUTING_INCLUDE   ${OS_ROUTING_INCLUDE}
                             os_linux/os_routing_linux.h)

    SET(OS_INTERFACE_SOURCE  os_linux/os_interface_linux.c)
    SET(OS_INTERFACE_INCLUDE ${OS_INTERFACE_INCLUDE}
                             os_linux/os_interface_linux.h)

    SET(OS_SYSTEM_SOURCE     os_linux/os_system_linux.c)
    SET(OS_SYSTEM_INCLUDE    ${OS_SYSTEM_INCLUDE}
                             os_linux/os_system_linux.h)

    SET(OS_VIF_SOURCE        os_linux/os_vif_linux.c)
    SET(OS_VIF_INCLUDE       os_linux/os_vif_linux.h)
ENDIF(LINUX)

# link librt to os_clock plugin to allow building with older GLIBC versions
# we need librt in the pluginloader anyways
oonf_create_plugin("os_clock" "${OS_CLOCK_SOURCE}" "${OS_CLOCK_INCLUDE}" "rt")
oonf_create_plugin("os_socket" "${OS_SOCKET_SOURCE}" "${OS_SOCKET_INCLUDE}" "")
oonf_create_plugin("os_routing" "${OS_ROUTING_SOURCE}" "${OS_ROUTING_INCLUDE}" "")
oonf_create_plugin("os_interface" "${OS_INTERFACE_SOURCE}" "${OS_INTERFACE_INCLUDE}" "")
oonf_create_plugin("os_system" "${OS_SYSTEM_SOURCE}" "${OS_SYSTEM_INCLUDE}" "")
oonf_create_plugin("os_vif" "${OS_VIF_SOURCE}" "${OS_VIF_INCLUDE}" "")
