# Ecclesia

Ecclesia is a set of tools and libraries for building machine hardware
management and monitoring agents. It provides implementations of several
different server-specific agents designed to run on a Linux host OS and export
data via a [Redfish](https://www.dmtf.org/standards/redfish) interface. This is
done to mirror one of the standard APIs used to provide an equivalent function
on BMCs, in particular, by [OpenBMC](https://github.com/openbmc/openbmc)-based
BMCs.

In addition to these server-specific implementations of management agents, this
project also contains a set of tools for building *more* implementations, as
well as a variety of smaller libraries to simplify accessing the lower-level
system interfaces needed to interact with hardware.

This project also provides an implementation of a machine manager service that
can provide a single unified RPC interface (defined and implemented using
[gRPC](https://grpc.io/)) using one or more Redfish agents as backends.
