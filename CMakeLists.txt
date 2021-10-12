
target_sources(third_party_FreeRTOS-Plus-TCP
                PUBLIC
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_DHCP.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_ARP.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_UDP_IP.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_DNS.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_TCP_IP.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_Sockets.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_Stream_Buffer.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_TCP_WIN.c"
                    "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS_IP.c"

            )
