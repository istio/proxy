*******************************************************************************

	    Intel Dynamic Load Balancer 2.x Driver Release 8.8.0

********************************************************************************
Disclaimer: This code is being provided to potential customers of Intel DLB to
enable the use of Intel DLB well ahead of the kernel.org and DPDK.org
upstreaming process. Please be aware that based on the open source community
feedback, the design of the code module can change, including API
interface definitions. If the open source implementation differs from what is
presented in this release, Intel may reserve the right to update the
implementation to align with the open source version at a later time and stop
supporting this early enablement version.

===================
Table of Contents
===================

* Intel Dynamic Load Balancer
* Intel DLB Software Installation Notes
* Kernel and DPDK Versions
* Build and Installation
* Examples / Test Cases
* Alerts & Notices
* Known Issues

=========================================
Intel Dynamic Load Balancer (Intel DLB)
=========================================

Intel DLB 2.x is a PCIe device that provides load-balanced, prioritized
scheduling of events across CPU cores enabling efficient core-to-core
communication. It can also serve as an accelerator for the event-driven programming 
model of DPDK's Event Device Library. This DPDK library is used in packet processing 
pipelines for multi-core scalability, dynamic load-balancing, and variety of packet
distribution and synchronization schemes. For non-DPDK applications, a separate library
named libdlb is provided with the release. 

=======================================
Intel DLB Software Installation Notes
=======================================

The Intel DLB software comes in the following package:

    dlb_linux_src_release_<rel-id>.txz

The package contains the Intel DLB kernel driver, DPDK Patch for Intel DLB 2.x
PMD (Poll Mode Driver) with other required changes to standard DPDK, and libdlb
client library for non-dpdk applications. The package can be extracted using
the following command:

    $ tar xfJ dlb_linux_src_release_<rel-id>.txz

DLB Software feature details can be found in DLB Programmer Guides.

============================
Kernel and DPDK Versions
============================

Linux Kernel Version: Tested with Linux Kernel versions 5.15, 5.19, 6.2, 6.6.
Pre-Silicon Validation supported on Linux kernel versions 6.7 and 6.8.

DPDK Base Version: v22.11.2

===========================
Build and Installation
===========================

Refer to document "DLB_SW_User_Guide.pdf" for detailed instructions on build
and installation.

==========================
Examples / Test Cases
==========================

This Intel DLB release contains libdlb, the client library for building Intel
DLB applications which do not require complete DPDK framework. It also includes
a set of sample tests to demonstrate various features. Accompanying User Guide
has details on the included samples.

====================================================
Resolved issues and new features in release 8.8.0
====================================================

- HQM-678: API should report error if no interrupts are available inside VM - Resolved.
- HQM-766: Live migration will fail if an application repeats multiple times - Resolved.
- HQM-899: Driver loading time is long (10 s) when port probing is enabled by default - Resolved.
- HQM-903: DLB2 reset fail when running dpdk-test-eventdev in container - Resolved.
- HQM-911: DPDK DLB2 PMD Enqueue time reordering support - Resolved.
- HQM-920: LM - DMA failures in destination VM at step 6 of live migration - Resolved.
- HQM-928: Fix race condition in dynamic linking - Resolved.
- HQM-935: Add make option to turn on off SIOV for dlb driver - Resolved.
- HQM-941: Remove dlb2_user.h in libdlb and use the one from kernel driver - Resolved.
- HQM-951: Credit return issue in deadlock avoidance mechanism - Resolved.
- CSSY-5037: copy producer coremask to DLB PMD devargs - Resolved.
- CSSY-5296: Restore QID inside event for DIR queue correctly - Resolved.

=====================
Alerts & Notices
=====================

- The software distributed in this release is not intended for secure use
  and should not be used to transmit or store security sensitive or privacy
  protected data.
- DLB2.x application only supports simultaneous use of either Bi-furcated mode
  devices  (bound to dlb2 driver) or PF PMD mode devices (bound to vfio-pci).
  Application should use all devices of the same type when some devices are
  bound to dlb2 driver and some to vfio-pci module. Different application can
  use different types and run at the same time. To avoid picking devices bound
  to vfio-pci while using devices bound to dlb2 driver, use the --no-pci
  option.
- HQM-897: Live Migration is supported only on intel-next kernel v5.15 until
  the DMA transfer issue is fixed in qemu/kernel 6.2 and 6.6.
- SIOV is not supported with SUSE Linux Enterprise Server 15 because functions
  needed are not back ported.
- HQM-417: For older kernels (pre 5.15) with some specific application patterns
  and kernel configuration, DLB device binding to vfio-pci driver results in a
  crash. In such cases, "uio_pci_generic" or "igb_uio" module can be used in
  place of "vfio-pci" (with IOMMU disabled or in passthrough mode) or Bifurcated
  mode can be used by linking device to DLB driver. This issue is not observed
  with kernels 5.15 and later.
- HQM-899: A new port probing value (2) is introduced for port probing which improves
  driver load times. Port_probe = 0; No probe, Port_probe = 1; enables port probing
  with longer load times but higher success rate compared to improved probe, 
  Port_probe = 2 ; (improved probe), less driver load times, less probe success rate
  compared to port_probe = 1. Improved probe is the default driver behavior.
  
=====================
Known Issues
=====================
- The movdir64b instruction does not work properly in pf passthrough mode with libdlb.
  User can disable movdir64b feature when running qemu for the pf passthrough
  by using "-cpu host,movdir64b=off" option.