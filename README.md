
# Embedded Linux BSP

> Structured notes and case studies for Embedded Linux, Android BSP, and kernel subsystems.

----------

## Overview

This repository contains:

-   Structured notes for Linux kernel and subsystems
-   BSP bring-up related knowledge
-   Android system notes
-   Driver examples with trace notes
-   Real-world debugging case studies

----------

## Contents

### Notes

-   Linux subsystem overviews (memory, scheduler, networking, etc.)
-   Kernel concepts (interrupts, synchronization, power management)
-   BSP topics (boot flow, device tree, interfaces, debug tools)
-   Android system components (binder, services, boot flow)
-   Firmware notes (TF-A boot flow and runtime)

----------

### Case Studies

`cases-study/` contains debugging and analysis records, including:

-   Boot and flashing issues
-   Peripheral bring-up problems
-   Kernel / driver debugging
-   System configuration issues

Platforms include:

-   Renesas
-   Rockchip
-   NXP

----------

### Code Examples

`code/examples/` contains small kernel and userspace examples:

-   Character device
-   miscdevice
-   mmap
-   DMA allocation
-   ioctl
-   synchronization primitives

Each example includes:

-   source code
-   Makefile
-   trace notes

----------

### Driver Notes

-   `bcmdhd/` – Broadcom WiFi driver notes
-   `bluetooth/` – Bluetooth stack and transport notes
-   `mt76/` – MT76 driver analysis

----------

## Purpose

This repository is used to:

-   Organize subsystem knowledge
-   Record debugging experience
-   Keep trace notes for kernel and drivers

----------

## Author

LC Wang
