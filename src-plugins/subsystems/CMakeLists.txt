# create plugins for single-file (source plus header) subsystems
SET(SINGLE_FILE_NAMES    oonf_class
                         oonf_clock
                         oonf_duplicate_set
                         oonf_http
                         oonf_interface
                         oonf_layer2
                         oonf_packet_socket
                         oonf_socket
                         oonf_stream_socket
                         oonf_telnet
                         oonf_timer
                         oonf_viewer)

foreach(name ${SINGLE_FILE_NAMES})
    oonf_create_plugin("${name}" "${name}.c" "${name.h}" "")
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
oonf_create_plugin("oonf_rfc5444" "${RFC5444_SOURCE}" "${RFC5444_INCLUDE}" "")

# generate the os-specific plugins
# TODO: add BSD and WIN32

SET(OS_CLOCK_INCLUDE   os_clock.h)
SET(OS_NET_INCLUDE     os_net.h)
SET(OS_ROUTING_INCLUDE os_routing.h)
SET(OS_SYSTEM_INCLUDE  os_system.h)

IF(LINUX)
    SET(OS_CLOCK_SOURCE    os_linux/os_clock_linux.c)
    SET(OS_CLOCK_INCLUDE   ${OS_CLOCK_INCLUDE}
                           os_linus/os_clock_linux.h)
    
    SET(OS_NET_SOURCE      os_generic/os_net_generic_configsocket.c
                           os_generic/os_net_generic_getsocket.c
                           os_generic/os_net_generic_getrawsocket.c
                           os_generic/os_net_generic_join_mcast.c
                           os_generic/os_net_generic_set_nonblocking.c
                           os_generic/os_net_generic_set_dscp.c
                           os_linux/os_net_linux.c
                           os_linux/os_net_linux_skip_rawsocket_prefix.c)
    SET(OS_NET_INCLUDE     ${OS_NET_INCLUDE}
                           os_linux/os_net_linux.h)

    SET(OS_ROUTING_SOURCE  os_generic/os_routing_generic_rt_to_string.c
                           os_linux/os_routing_linux.c)
    SET(OS_ROUTING_INCLUDE ${OS_ROUTING_INCLUDE}
                           os_linux/os_routing_linux.h)

    SET(OS_SYSTEM_SOURCE   os_linux/os_system_linux.c)
    SET(OS_SYSTEM_INCLUDE  ${OS_SYSTEM_INCLUDE}
                           os_linux/os_system_linux.h)
ENDIF(LINUX)

oonf_create_plugin("os_clock" "${OS_CLOCK_SOURCE}" "${OS_CLOCK_INCLUDE}" "")
oonf_create_plugin("os_net" "${OS_NET_SOURCE}" "${OS_NET_INCLUDE}" "")
oonf_create_plugin("os_routing" "${OS_ROUTING_SOURCE}" "${OS_ROUTING_INCLUDE}" "")
oonf_create_plugin("os_system" "${OS_SYSTEM_SOURCE}" "${OS_SYSTEM_INCLUDE}" "")
