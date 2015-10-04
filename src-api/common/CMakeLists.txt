SET(OONF_COMMON_SRCS  autobuf.c
                      avl.c
                      bitmap256.c
                      heap.c
                      isonumber.c
                      json.c
                      key_comp.c
                      netaddr.c
                      netaddr_acl.c
                      string.c
                      template.c)

SET(OONF_COMMON_INCLUDES autobuf.h
                         avl.h
                         bitmap256.h
                         heap.h
                         common_types.h
                         container_of.h
                         isonumber.h
                         json.h
                         key_comp.h
                         list.h
                         netaddr.h
                         netaddr_acl.h
                         string.h
                         template.h)

oonf_create_library("common" "${OONF_COMMON_SRCS}" "${OONF_COMMON_INCLUDES}" "" "")
