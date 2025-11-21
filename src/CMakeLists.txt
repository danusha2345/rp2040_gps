
link_directories(${CMAKE_CURRENT_LIST_DIR}/OSSL/lib
                ${CMAKE_CURRENT_LIST_DIR}/mbedtls/library  )
add_executable(podmena main.c sha256.c)
#set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

pico_generate_pio_header(podmena ${CMAKE_CURRENT_LIST_DIR}/uart_rx.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/)
pico_generate_pio_header(podmena ${CMAKE_CURRENT_LIST_DIR}/uart_tx.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/)

target_include_directories(podmena PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/OSSL/include/openssl
    ${CMAKE_CURRENT_LIST_DIR}/mbedtls
)

target_sources(podmena PRIVATE main.c)

# Указание файла memory.x
set(PICO_TARGET_LINKER_FILE "${CMAKE_SOURCE_DIR}/memory.x")

set(SSL "${CMAKE_CURRENT_LIST_DIR}/OSSL/lib/libssl.a")
set(Crypto "${CMAKE_CURRENT_LIST_DIR}/OSSL/lib/libcrypto.a")
# set(mbedtls "${CMAKE_CURRENT_LIST_DIR}/mbedtls/library/libmbedtls.a")
# set(mbedcrypto "${CMAKE_CURRENT_LIST_DIR}/mbedtls/library/libmbedcrypto.a")
# set(mbedx509 "${CMAKE_CURRENT_LIST_DIR}/mbedtls/library/libmbedx509.a")


target_link_libraries(podmena 
PUBLIC  pico_stdlib
    FreeRTOS-Kernel-Heap4
    hardware_pio
    hardware_flash   
   # ${mbedtls}
   # ${mbedcrypto}
   # ${mbedx509}
   # libssl libcrypto
   # libmbedtls libmbedcrypto
    ${SSL} 
    ${Crypto}
   # tinyusb_device    # This is required for TinyUSB
   # tinyusb_board    # Some additional board stuff for TinyUSB
    )

pico_add_extra_outputs(podmena)




