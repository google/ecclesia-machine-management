# PCI I/O Library

This library provides a variety of classes for interacting with PCI hardware.

In order to provide maximum flexibility, the APIs for accessing devices are
split into a number of layers. This is helpful in situations where you need to
mix and match different access methods for different devices, or for different
aspects of the same device.

This README does not provide a detailed definition of every single class and API
provided in this package. The goal is only to summarize the general structure.

## Layout

The library is divided into a number of layers, some of which are built on top
of the others. The high level list of the layers provided is:

*   Locations
*   Signatures
*   Regions
*   Config Space
*   Resources
*   Devices
*   Discovery

There are also a couple of side libraries, the Mock and Protobuf libraries. The
former provides mock versions of some of the interface classes, for use in
testing. The latter provides some adapters that can convert between some of the
data classes and protocol buffer equivalents.

## Locations

The location classes are one of the most basic layers. They provide several
classes which represent the "address" of a PCI device. The most commonly used of
these is the `PciDbdfLocation` class which represents a devices location in the
fairly standard "domain-bus-device-function" format (BDF, or DBDF).

## Signatures

The signature classes are another basic layer. They provide a standard
representation of the identifiers that devices use to indicate what hardware
they are: vendor and device IDs, as well as subsystem IDs.

## Regions

These classes provide an interface (`PciRegion`) that provides access to a
generic "memory region" associated with a PCI device. These regions could be a
config space, but they could also be a memory region associated with a device
BAR. The operations available are very low-level (fixed sized reads and writes)
but are an important building block for some of the higher layers.

## Config Space

These classes provide an API for doing structured reads and writes to and from
config space. The classes rely on the Region interface for providing the actual
low-level access, but unlike region they provide higher level operations. For
example, you can read the vendor ID of a device using the `BaseSignature`
function on `PciConfigSpace` rather than using `Read16(0)` on `PciRegion`.

The number of classes this library provides is substantial, as the general
approach is to make different subclasses for each different config space layout.
Thus, rather than having one big "config space class" that does everything,
there is instead a generic `PciConfigSpace` that represents the features common
to all device, which is then bundled with subclasses that provide type-specific
information.

For example if you wanted to access a property that is unique to type 1 devices
like the secondary bus number, you need to use the following access pattern:

*   Start with the generic `PciConfigSpace` for the object
*   Use `GetIf` to try and get a `PciType1ConfigSpace` adapter
*   If the `GetIf` fails then the device is not actually a type 1 device
*   If it succeeds then you can use `SecondaryBusNum()` on the adapter

While this sounds like a somewhat cumbersome access pattern, it has a major
advantage in that it forces you to explicitly check that you have the right type
of device for doing the accesses you want.

The library also provides a similar access pattern for capabilities, providing a
generic `PciCapability` class which is akin to `PciConfigSpace` and then various
subclasses for accessing different, specific capbilities.

## Resources

This layer provides an interface for providing access to the resources that are
associated with a device; basically, for accessing BARs.

While that might seem unnecessary given that BAR information can be accessed via
config space, in practice it is useful to separate out because operating systems
(e.g. Linux) often provide access to resources via a distinct API. Thus, it is
useful to define an API that does not explicitly depend on the config space
layer. This does not preclude providing an implementation that uses the config
space classes, but it also does not require providing such an implementation.

## Devices

The `PciDevice` interface is provided to allow all of these more basic layers to
be assembled into a single object that provides concrete instances of all the
different interfaces defined in the lower layers. In particular it gives you
access to the device's:

*   Location
*   ConfigSpace
*   Resources

It also provides some basic high-level access features, but the bulk of the
telemetry it exposes is expected to be read through its various subinterfaces.

## Discovery

The final layer is the discovery layer. Unlike most of the other layers, instead
of providing APIs for interacting with single endpoint, its focus is on
identifying what endpoints are available and how they link to each other. If you
wanted to enumerate and access *all* of the devices in a system this is the
layer you would use as a starting point.
