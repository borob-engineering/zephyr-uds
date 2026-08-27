# Zephyr UDS (ISO 14229-1) Server Module

[![License](https://shields.io)](https://opensource.org)
[![Zephyr](https://shields.io)](https://zephyrproject.org)

A highly modular, hardware-abstracted, and fully compliant **Unified Diagnostic Services (UDS / ISO 14229-1)** server subsystem tailored specifically for **Zephyr RTOS v4.4.0**. 

This module decouples network-layer packet processing (ISO-TP over CAN) and diagnostic protocol execution from physical memory and actuator control via safe, application-defined `__weak` execution hooks.

---

## 🚀 Reference Application

To see this driver module in action within a fully working environment, check out the official companion application repository:

👉 **[zephyr-uds-app](https://github.com/borob-engineering/zephyr-uds-app)**

This reference application demonstrates how to mount the required flash filesystem, override the protocol hooks, configure specific board overlays (e.g., for STM32), and integrate with MCUboot for full over-the-air firmware update streaming.

---

## Key Features

- **ISO-TP Core Server (`0x10`, `0x3E`):** Multi-threaded physical and functional request processing leveraging Zephyr's native CAN and ISO-TP driver stacks.
- **Persistent Fault Memory (`0x14`, `0x19`):** Non-Volatile Storage (NVS) integrated DTC management utilizing Zephyr's KVSS/NVS flash subsystem with hardware-optimized wear leveling.
- **Brute-Force Attack Prevention (`0x27`):** Non-volatile key-fail counting and automated hardware lockout tracking using true hardware entropy generators.
- **Flexible Data Routines (`0x22`, `0x2E`, `0x2C`, `0x2A`):** Supports dynamic Data Identifiers (DID aggregation), rapid periodic identifiers via background scheduling threads, and fully persistent VIN management.
- **Bootloader & Flash Pipeline (`0x34`, `0x36`, `0x37`):** Sequential streaming architecture enabling block-wise software flashing sequences with pre-erase hooks.
- **Actuator Signal Substitution (`0x2F`):** Abstracted IO Control interface with session/security guardrails.
- **Asynchronous NRC 0x78 Supervision:** Dynamically manages `ResponsePending` frames via discrete kernel hardware timers during long-running flash erasings or test routines.

---

## Directory Structure

```text
.
├── CMakeLists.txt              # Standardized multi-source Zephyr library compilation
├── include
│   └── zephyr
│       └── canbus
│           ├── uds_app_interface.h # Persistent/Weak OS hooks interface definitions
│           └── uds_types.h         # Standard ISO 14229-1 NRCs and structural types
├── LICENSE                     # Apache 2.0 License Manifest
├── README.md                   # This project guide
├── src
│   ├── Kconfig.uds             # System environment and hardware layer dependencies
│   ├── uds_clear_dtc.c         # Service 0x14 - Persistent flash log clearing logic
│   ├── uds_clear_dtc.h
│   ├── uds_data_storage.c      # Volatile cache & persistent VIN storage binding
│   ├── uds_data_storage.h
│   ├── uds_dynamic_did.c       # Service 0x2C - Dynamic composite DID management
│   ├── uds_dynamic_did.h
│   ├── uds_flash_pipeline.c    # Services 0x34/36/37 - Binary streaming pipeline
│   ├── uds_flash_pipeline.h
│   ├── uds_iocontrol.c         # Service 0x2F - Input/Output signal intervention
│   ├── uds_iocontrol.h
│   ├── uds_periodic.c          # Service 0x2A - Fast periodic DID worker thread
│   ├── uds_periodic.h
│   ├── uds_read_dtc.c          # Service 0x19 - Permanent NVS diagnostic memory engine
│   ├── uds_read_dtc.h
│   ├── uds_reset.c             # Service 0x11 - Delayed native core SoC reboot handler
│   ├── uds_reset.h
│   ├── uds_routine.c           # Service 0x31 - Parallel background work execution
│   ├── uds_routine.h
│   ├── uds_security.c          # Service 0x27 - Hardware seed entropy & lockout tracker
│   ├── uds_security.h
│   ├── uds_server.c            # Central network engine interface loop
│   ├── uds_session.c           # Service 0x10 - Diagnostic session state machine
│   ├── uds_session.h
│   └── uds_weak_defaults.c     # Safe application interface default fallback hooks
└── zephyr
    └── module.yml              # West integration meta-descriptor for module discovery
```

---

## Module Integration (West)

To include this library as an external module inside your workspace topology, add the following reference into your `west.yml` project manifest:

```yaml
manifest:
  remotes:
    - name: borob-engineering
      url-base: https://github.com
  projects:
    - name: zephyr-uds
      remote: borob-engineering
      revision: devel
      path: modules/lib/zephyr-uds
```

Alternatively, you can compile it directly by passing the path parameters to your build command:
```bash
west build -b <your_board> -- -DZEPHYR_EXTRA_MODULES=<path_to_module>/zephyr-uds
```

---

## Prerequisites & Kconfig Configuration

The application layer must activate the following components inside its `prj.conf` to build successfully:

```properties
# Network Subsystem Dependencies
CONFIG_CAN=y
CONFIG_ISOTP=y

# Subsystem Dependencies required by UDS Module
CONFIG_UDS_SERVER=y
CONFIG_FLASH=y
CONFIG_FLASH_PAGE_LAYOUT=y
CONFIG_NVS=y
CONFIG_ENTROPY_GENERATOR=y
CONFIG_REBOOT=y

# Customizable Modulating Constants
CONFIG_UDS_BUFF_SIZE=4095
CONFIG_UDS_S3_TIMEOUT_MS=5000
CONFIG_UDS_SEC_MAX_FAILED_ATTEMPTS=3
CONFIG_UDS_SEC_LOCKOUT_TIME_MS=10000
CONFIG_UDS_SEC_SEED_SIZE=4
```

### Devicetree Requirements
The module relies on the presence of a dedicated hardware partition for permanent parameter storage and anti-tamper tracking. Ensure your `app.overlay` contains the `storage_partition` nodelabel pointing to a valid fixed flash allocation block:

```dts
&flash0 {
	partitions {
		compatible = "fixed-partitions";
		#address-cells = <1>;
		#size-cells = <1>;

		storage_partition: partition@f8000 {
			label = "storage";
			reg = <0x000f8000 DT_SIZE_K(16)>;
		};
	};
};
```

---

## Application Implementation Pattern

To leverage the driver, the application must instantiate and provide its active storage filesystem handle by overriding the weak abstraction boundaries. Code examples for this bridging pattern can be found directly within the `app/src/app_uds_impl.c` file of the **zephyr-uds-app** project.

```c
#include <zephyr/kernel.h>
#include <zephyr/canbus/uds_app_interface.h>

/* Application managed NVS filesystem handle defined in app initialization layer */
extern struct nvs_fs my_application_nvs;

struct nvs_fs *uds_app_get_nvs_context(void)
{
	return &my_application_nvs;
}

int uds_app_clear_persistent_dtcs(struct nvs_fs *fs, uint32_t dtc_group)
{
	/* Implement your hardware sector erasure / nvs_delete looping logic here */
	return 0;
}
```

---

## License

Distributed under the terms of the **Apache License, Version 2.0**. For explicit details, reference the `LICENSE` file within the repository base.
