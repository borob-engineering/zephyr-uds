# zephyr-uds-server/CMakeLists.txt
if(CONFIG_UDS_SERVER)

zephyr_library()

# Dateien werden nur einkompiliert, wenn das Subsystem via Kconfig aktiviert wurde
zephyr_library_sources(src/uds_server.c)
zephyr_library_sources(src/uds_session.c)
zephyr_library_sources(src/uds_security.c)
zephyr_library_sources(src/uds_routine.c)
zephyr_library_sources(src/uds_reset.c)
zephyr_library_sources(src/uds_clear_dtc.c)
zephyr_library_sources(src/uds_read_dtc.c)
zephyr_library_sources(src/uds_write_did.c)
zephyr_library_sources(src/uds_flash_pipeline.c)
zephyr_library_sources(src/uds_iocontrol.c)
zephyr_library_sources(src/uds_weak_defaults.c)

# Macht die Header im Include-Ordner global für Applikationen verfügbar
zephyr_library_include_directories(include)

endif()