# create plugins for single-file (source plus header) subsystems
SET(rfc5444_api_source  rfc5444.c
                        rfc5444_context.c
                        rfc5444_iana.c
                        rfc5444_msg_generator.c
                        rfc5444_pkt_generator.c
                        rfc5444_print.c
                        rfc5444_reader.c
                        rfc5444_tlv_writer.c
                        rfc5444_writer.c)

add_library(oonf_static_rfc5444_api OBJECT ${rfc5444_api_source})
