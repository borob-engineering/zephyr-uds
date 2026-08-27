# Copyright (c) 2026 borob-engineering
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_UDS_SERVER)

  zephyr_library()

  # Expose internal module header locations to the compilation unit scope
  zephyr_library_include_directories(include/zephyr/canbus)

  # Consolidate all subsystem driver source files within a single unified call
  zephyr_library_sources(
    src/uds_server.c
    src/uds_session.c
    src/uds_security.c
    src/uds_routine.c
    src/uds_reset.c
    src/uds_clear_dtc.c
    src/uds_read_dtc.c
    src/uds_write_did.c
    src/uds_flash_pipeline.c
    src/uds_iocontrol.c
    src/uds_weak_defaults.c
    src/uds_periodic.c
    src/uds_dynamic_did.c
    src/uds_data_storage.c
  )

  # Export public interface directory globally to the application layer
  zephyr_include_directories(include)

endif()
