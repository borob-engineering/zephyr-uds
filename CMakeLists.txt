cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(uds_isotp_app)

target_sources(app PRIVATE 
    src/uds_server.c 
    src/uds_session.c
    src/uds_security.c 
    src/uds_routine.c
    src/uds_reset.c
    src/uds_clear_dtc.c
    src/uds_read_dtc.c
    src/uds_data_storage.c
    src/uds_write_did.c
    src/uds_flash_pipeline.c
    src/uds_iocontrol.c
)
