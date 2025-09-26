.. SPDX-License-Identifier: GPL-2.0-only

===========================================
Intel(R) Dynamic Load Balancer 2.0 Overview
===========================================

:Author: Gage Eads

Contents
========

- Introduction
- Scheduling
- Queue Entry
- Port
- Queue
- Credits
- Scheduling Domain
- Interrupts
- Power Management
- Virtualization
- User Interface
- Reset

Introduction
============

The Intel(r) Dynamic Load Balancer 2.0 (Intel(r) DLB 2.0) is a PCIe device that
provides load-balanced, prioritized scheduling of core-to-core communication.

The Intel DLB 2.0 device consists of queues and arbiters that connect producer
cores and consumer cores. The device implements load-balanced queueing features
including:
- Lock-free multi-producer/multi-consumer operation.
- Multiple priority levels for varying traffic types.
- 'Direct' traffic (i.e. multi-producer/single-consumer)
- Simple unordered load-balanced distribution.
- Atomic lock free load balancing across multiple consumers.
- Queue element reordering feature allowing ordered load-balanced distribution.

Intel DLB 2.0 can be used in an event-driven programming model, such as DPDK's
Event Device Library[2]. Such frameworks are commonly used in packet processing
pipelines that benefit from the framework's multi-core scalability, dynamic
load-balancing, and variety of packet distribution and synchronization schemes.

Scheduling Types
================

Intel DLB 2.0 supports four types of scheduling of 'events' (using DPDK
terminology), where an event can represent any type of data (e.g. a network
packet). The first, ``directed``, is multi-producer/single-consumer style
core-to-core communication. The remaining three are
multi-producer/multi-consumer, and support load-balancing across the consumers.

- ``Unordered``: events are load-balanced across consumers without any ordering
                 guarantees.

- ``Ordered``: events are load-balanced across consumers, and when the consumer
               re-injects each event into the device it is re-ordered into the
               original order. This scheduling type allows software to
               parallelize ordered event processing without the synchronization
               cost of re-ordering packets.

- ``Atomic``: events are load-balanced across consumers, with the guarantee that
              events from a particular 'flow' are only scheduled to a single
              consumer at a time (but can migrate over time). This allows, for
              example, packet processing applications to parallelize while
              avoiding locks on per-flow data and maintaining ordering within a
              flow.

Intel DLB 2.0 provides hierarchical priority scheduling, with eight priority
levels within each. Each consumer selects up to eight queues to receive events
from, and assigns a priority to each of these 'connected' queues. To schedule
an event to a consumer, the device selects the highest priority non-empty queue
of the (up to) eight connected queues. Within that queue, the device selects
the highest priority event available (selecting a lower priority event for
starvation avoidance 1% of the time, by default).

The device also supports four load-balanced scheduler classes of service. Each
class of service receives a (user-configurable) guaranteed percentage of the
scheduler bandwidth, and any unreserved bandwidth is divided evenly among the
four classes.

Queue Entry
===========

Each event is contained in a queue entry (QE), the fundamental unit of
communication through the device, which consists of 8B of data and 8B of
metadata, as depicted below.

QE structure format
::
    data     :64
    opaque   :16
    qid      :8
    sched    :2
    priority :3
    msg_type :3
    lock_id  :16
    rsvd     :8
    cmd      :8

The ``data`` field can be any type that fits within 8B (pointer, integer,
etc.); DLB 2.0 merely copies this field from producer to consumer. The
``opaque`` and ``msg_type`` fields behave the same way.

``qid`` is set by the producer to specify to which DLB 2.0 queue it wishes to
enqueue this QE. The ID spaces for load-balanced and directed queues are both
zero-based; the ``sched`` field is used to distinguish whether the queue is
load-balanced or directed.

``sched`` controls the scheduling type: atomic, unordered, ordered, or
directed. The first three scheduling types are only valid for load-balanced
queues, and the directed scheduling type is only valid for directed queues.

``priority`` is the priority with which this QE should be scheduled.

``lock_id``, used only by atomic scheduling, identifies the atomic flow to
which the QE belongs. When sending a directed event, ``lock_id`` is simply
copied like the ``data``, ``opaque``, and ``msg_type`` fields.

``cmd`` specifies the operation, such as:
- Enqueue a new QE
- Forward a QE that was dequeued
- Complete/terminate a QE that was dequeued
- Return one or more consumer queue tokens.
- Arm the port's consumer queue interrupt.

Port
====

A core's interface to the DLB 2.0 is called a "port," and consists of an MMIO
region through which the core enqueues a queue entry, and an in-memory queue
(the "consumer queue") to which the device schedules QEs. A core enqueues a QE
to a device queue, then the device schedules the event to a port. Software
specifies the connection of queues and ports; i.e. for each queue, to which
ports the device is allowed to schedule its events. The device uses a credit
scheme to prevent overflow of the on-device queue storage.

Applications interface directly with the device by mapping the port's memory
and MMIO regions into the application's address space for enqueue and dequeue
operations, but call into the kernel driver for configuration operations. An
application can also be polling- or interrupt-driven; DLB 2.0 supports both
modes of operation.

Queue
=====

The device contains 32 load-balanced (i.e. capable of atomic, ordered, and
unordered scheduling) queues and 64 directed queues. Each queue comprises 8
internal queues, one per priority level. The internal queue that an event is
enqueued to is selected by the event's priority field.

A load-balanced queue is capable of scheduling its events to any combination of
load-balanced ports, whereas each directed queue has one-to-one mapping with a
directed port. There is no restriction on port or queue types when a port
enqueues an event to a queue; that is, a load-balanced port can enqueue to a
directed queue and vice versa.

Credits
=======

The Intel DLB 2.0 uses a credit scheme to prevent overflow of the on-device
queue storage, with separate credits for load-balanced and directed queues. A
port spends one credit when it enqueues a QE, and one credit is replenished
when a QE is scheduled to a consumer queue. Each scheduling domain has one pool
of load-balanced credits and one pool of directed credits; software is
responsible for managing the allocation and replenishment of these credits among
the scheduling domain's ports.

Scheduling Domain
=================

Device resources -- including ports, queues, and credits -- are contained
within a scheduling domain. Scheduling domains are isolated from one another; a
port can only enqueue to and dequeue from queues within its scheduling domain.

A scheduling domain's resources are configured through a domain file descriptor,
which is acquired through an ioctl. This design means that any application with
sufficient permissions to access the device file can request the fd of any
scheduling domain within that device. When necessary to prevent independent dlb2
applications from potentially accessing each other's scheduling domains, the
user can create multiple virtual functions (each with its own device file) and
restrict access via file permissions.

Consumer Queue Interrupts
=========================

Each port has its own interrupt which fires, if armed, when the consumer queue
depth becomes non-zero. Software arms an interrupt by enqueueing a special
'interrupt arm' command to the device through the port's MMIO window.

Power Management
================

The kernel driver keeps the device in D3Hot when not in use. The driver
transitions the device to D0 when the first device file is opened or a virtual
function is created, and keeps it there until there are no open device files,
memory mappings, or virtual functions.

Virtualization
==============

The DLB 2.0 supports both SR-IOV and Scalable IOV, and can flexibly divide its
resources among the physical function (PF) and its virtual devices. Virtual
devices do not configure the device directly; they use a hardware mailbox to
proxy configuration requests to the PF driver. Mailbox communication is
initiated by the virtual device with a registration message that establishes
the mailbox interface version.

SR-IOV
------

Each SR-IOV virtual function (VF) has 32 interrupts, 1 for PF->VF mailbox
messages and the remainder for CQ interrupts. If a VF user (e.g. a guest OS)
needs more CQ interrupts, they have to use more than one VF.

To support this case, the driver introduces the notion of primary and auxiliary
VFs. A VF is either considered primary or auxiliary:
- Primary: the VF is used as a regular DLB 2.0 device. The primary VF has 0+
           auxiliary VFs supporting it.
- Auxiliary: the VF doesn't have any resources of its own, and serves only to
             provide the primary VF with MSI vectors for its CQ interrupts.

Each VF has an aux_vf_ids file in its sysfs directory, which is a R/W file that
controls the primary VF’s auxiliaries. When a VF is made auxiliary to another,
its resources are relinquished to the PF device.

When the VF driver registers its device with the PF driver, the PF driver tells
the VF driver whether its device is auxiliary or primary, and if so then the ID
of its primary VF. If it is auxiliary, the VF device will “claim” up to 31 of
the primary VF’s CQs, such that they use the auxiliary VF’s MSI vectors.

When a primary VF has one or more auxiliary VFs, the entire VF group must be
assigned to the same virtual machine. The PF driver will not allow the primary
VF to configure its resources until all its auxiliary VFs have been registered
by the guest OS’s driver.

Scalable IOV
------------

Scalable IOV is a virtualization solution that, compared to SR-IOV, enables
highly-scalable, high-performance, and fine-grained sharing of I/O devices
across isolated domains.

In Scalable IOV, the smallest granularity of sharing a device is the Assignable Device
Interface (ADI). Similar to SR-IOV’s Virtual Function, Scalable IOV defines the
Virtual Device (VDEV) as the abstraction at which a Scalable IOV device is exposed to
guest software, and a VDEV contains one or more ADIs.

Kernel software is responsible for composing and managing VDEV instances in
Scalable IOV. The device-specific software components are the Host (PF) Driver,
the Guest (VDEV) Driver, and the Virtual Device Composition Module (VDCM). The
VDCM is responsible for managing the software-based virtualization of (slow)
control path operations, like the mailbox between Host and Guest drivers.

For DLB 2.0, the ADI is the scheduling domain, which consists of load-balanced
and directed queues, ports, and other resources. Each port, whether
load-balanced or directed, consists of:
- A producer port: a 4-KB separated MMIO window
- A consumer queue: a memory-based ring to which the device writes queue entries
- One CQ interrupt message

DLB 2.0 supports up to 16 VDEVs per PF.

For Scalable IOV guest-host communication, DLB 2.0 uses a software-based
mailbox. This mailbox interface matches the SR-IOV hardware mailbox (i.e. PF2VF
and VF2PF MMIO regions) except that it is backed by shared memory (allocated
and managed by the VDCM). Similarly, the VF2PF interrupt trigger register
instead causes a VM exit into the VDCM driver, and the PF2VF interrupt is
replaced by a virtual interrupt injected into the guest through the VDCM.

User Interface
==============

The dlb2 driver uses ioctls as its primary interface. It provides two types of
files: the dlb2 device file and the scheduling domain file.

The two types support different ioctl interfaces; the dlb2 device file is used
for device-wide operations (including scheduling domain creation), and the
scheduling domain device file supports operations on the scheduling domain's
resources such as port and queue configuration.

The driver also exports an mmap interface through port files, which are
acquired through scheduling domain ioctls. This mmap interface is used to map
a port's memory and MMIO window into the process's address space.

Reset
=====

The dlb2 driver supports reset at two levels: scheduling domain and device-wide
(i.e. FLR).

Scheduling domain reset occurs when an application stops using its domain.
Specifically, when no more file references or memory mappings exist. At this
time, the driver resets all the domain's resources (flushes its queues and
ports) and puts them in their respective available-resource lists for later
use.

An FLR can occur while the device is in use by user-space software, so the
driver uses its reset_prepare callback to ensure that no applications continue
to use the device while the FLR executes. First, the driver blocks user-space
from executing ioctls or opening a device file, and evicts any threads blocked
on a CQ interrupt. The driver then notifies applications and virtual functions
that an FLR is pending, and waits for them to clean up with a timeout (default
of 5 seconds). If the timeout expires and the device is still in use by an
application, the driver zaps its MMIO mappings. Virtual functions, whether in
use or not, are reset as part of a PF FLR.

While PF FLR is a hardware procedure, VF FLR is a software procedure. When a
VF FLR is triggered, this causes an interrupt to be delivered to the PF driver,
which performs the actual reset. This consists of performing the scheduling
domain reset operation for each of the VF's scheduling domains.
