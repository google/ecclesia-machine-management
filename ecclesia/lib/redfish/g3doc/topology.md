# Repair Location Identifiers

## Motivation

Datacenter hardware require an identifier for any replaceable hardware unit
which can satisfy the following requirements:

1.  Identifiers that are stable over the life of a machine, even in the face of
    component failure.
2.  Identifiers which are consistent across telemetry producers to allow data
    correlation from different producers.
3.  Identifiers which can be understood unambiguously by humans without needing
    additional tools.

This document proposes an identification scheme for replaceable units in a
machine based on unique physical location. The proposed scheme uses well-defined
standard Redfish schemas and properties.

### Identification Scheme

In general, a replaceable unit is identified as either a:

> **Root chassis**, an unambiguous replaceable unit.

or

> <span style="color:blue">**A physical resource**</span> attached to a
> <span style="color:red">**labeled connector**</span> on
> <span style="color:green">**another replaceable unit**</span>

If a replaceable unit has multiple connectors, any connector can be arbitrarily
selected for identification as long as that connector is on an unambiguous
replaceable unit.

### Example

The following image provides a hypothetical system with modular identical
compute riser boards.

![Example system with a baseboard and two identical riser modules containing
memory and CPU resources](topology.png)

The replaceable resources identified by their repair location are:

1.  baseboard
1.  storage on the PCIE0 slot on the baseboard
1.  drive on the SATA1 slot on the baseboard
1.  memory on the DIMM0 slot on the baseboard
1.  processor on the CPU0 socket on the baseboard
1.  cable on the RISER0 connector on the baseboard
1.  cable on the RISER1 connector on the baseboard
1.  cable on the RISER2 connector on the baseboard
1.  cable on the RISER3 connector on the baseboard
1.  riser attached to the cable on RISER0 on the baseboard
1.  riser attached to the cable on RISER0 on the baseboard
1.  memory on the DIMM0 slot on the riser on the cable on RISER0 on the
    baseboard
1.  memory on the DIMM1 slot on the riser on the cable on RISER0 on the
    baseboard
1.  processor on the CPU socket on the riser on the cable on RISER0 on the
    baseboard
1.  memory on the DIMM0 slot on the riser on the cable on RISER2 on the
    baseboard
1.  memory on the DIMM1 slot on the riser on the cable on RISER2 on the
    baseboard
1.  processor on the CPU socket on the riser on the cable on RISER2 on the
    baseboard

## Redfish Schemas and Properties

### "Labeled Connector"

Any replaceable unit shall implement the following properties to identify its
repair location.

In Redfish DSP2046 v2022.1
[(pdf)](https://www.dmtf.org/sites/default/files/standards/documents/DSP2046_2022.1.pdf):

`Location.PartLocation.ServiceLabel` is defined as:

> The label of the part location, such as a silk-screened name or a printed
> label.

`Location.LocationType` is defined as:

> The type of location of the part. Possible values: *Backplane*, *Bay*,
> *Connector*, *Embedded*, *Slot*, *Socket*.

### Referencing "Another Replaceable Unit"

Any replaceable unit shall include a link or reference to another component in
order to disambiguate similar `ServiceLabel` fields.

The only exception shall be a ***root Chassis*** representing some unambiguously
identifiable component.

Some examples of a root chassis:

1.  motherboard
1.  primary backplane
1.  main board of an expansion tray

### Redfish Examples

#### Chassis Resources

##### Root Chassis

A root chassis is implied if it is not `ContainedBy` another chassis. It is
assumed that this Chassis is unambiguously identifiable in the system by a
human.

```json
{
    "@odata.type": "#Chassis.v1_14_0.Chassis",
    "Links": {
        "Contains": [
             {
                "@odata.id": "/redfish/v1/Chassis/subchassis_1"
            },
            {
                "@odata.id": "/redfish/v1/Chassis/subchassis_2"
            }
        ],
    }
}
```

##### Replaceable Chassis

```json
{
    "@odata.type": "#Chassis.v1_14_0.Chassis",
    "Links": {
        "ContainedBy": {
           "@odata.id": "/redfish/v1/Chassis/container_chassis"
        }
    },
    "Location": {
        "PartLocation": {
            "LocationType": "Slot",
            "ServiceLabel": "IO0"
      }
    }
}
```

#### Non-Chassis Resources

All replaceable non-`Chassis` resources shall have a link to a `Chassis`
resource.

##### Processor

```json
{
  "@odata.type": "#Processor.v1_11_0.Processor",
  "Links": {
    "Chassis": {
      "@odata.id": "/redfish/v1/Chassis/container_chassis"
    }
  },
  "Location": {
    "PartLocation": {
      "LocationType": "Slot",
      "ServiceLabel": "CPU0"
    }
  }
}
```

##### Memory

```json
{
  "@odata.type": "#Memory.v1_11_0.Memory",
  "Links": {
    "Chassis": {
      "@odata.id": "/redfish/v1/Chassis/container_chassis"
    }
  },
  "Location": {
    "PartLocation": {
      "LocationType": "Slot",
      "ServiceLabel": "DIMM6"
    }
  }
}
```

`ServiceLabel` is preferred over `MemoryLocation.MemoryDeviceLocator` solely for
consistency with the other resources described in this document.

##### Storage

```json
{
  "@odata.type": "#Storage.v1_4_0.Storage",
  "Links": {
    "Enclosures": [
      {"@odata.id": "/redfish/v1/Chassis/container_chassis"}
    ]
  },
  "Location": {
    "PartLocation": {
      "LocationType": "Slot",
      "ServiceLabel": "SATA3"
    }
  }
}
```

`Storage` can be potentially ambiguous if there are multiple `Links.Enclosures`
`Chassis` being referenced. The `Location.PartLocation.ServiceLabel` should
correspond to the first `Chassis`.

##### Drive

```json
{
  "@odata.type": "#Drive.v1_12_0.Drive",
  "Links": {
    "Chassis": {
      "@odata.id": "/redfish/v1/Chassis/container_chassis"
    }
  },
  "Location": {
    "PartLocation": {
      "LocationType": "Slot",
      "ServiceLabel": "NVME7"
    }
  }
}
```

##### Cable

If both `UpstreamChassis` and `DownstreamChassis` are defined, one can assume
`UpstreamChassis` will be less ambiguous and shall be used.

```json
{
  "@odata.type": "#Cable.v1_2_0.Cable",
  "Links": {
    "DownstreamChassis": {
      "@odata.id": "/redfish/v1/Chassis/some_plugin"
    },
    "UpstreamChassis": {
      "@odata.id": "/redfish/v1/Chassis/container_chassis"
    },
  },
  "Location": {
    "PartLocation": {
      "LocationType": "Slot",
      "ServiceLabel": "EXPANSION0"
    }
  }
}
```

## Device Path (Devpath) Notation

In the abstract, the repair location identifiers produce a directed acyclic
graph to include all replaceable units in a system.

A useful representation of these identifiers is a string concatenation of all
connectors leading to one replaceable unit.

A **root chassis** will have the following devpath to signify it represents the
start of a physical path:

> /phys

Any <span style="color:blue">**physical resource**</span> attached to a
<span style="color:red">**labeled connector**</span> on
<span style="color:green">**another replaceable unit**</span> will have a
devpath of:

> <span style="color:green">$OTHER_REPLACEABLE_UNIT_DEVPATH</span>/<span style="color:red">$CONNECTOR_LABEL</span>

Since cables do not typically have service labels for their ends, a
<span style="color:red">connector label</span> for a cable shall be defined to
be `"DOWNLINK"`.

Therefore, some devpath examples using the same example system:

![Example system with a baseboard and two identical riser modules containing
memory and CPU resources](topology.png)

1.  /phys
1.  /phys/PCIE0
1.  /phys/SATA1
1.  /phys/DIMM0
1.  /phys/CPU0
1.  /phys/RISER0
1.  /phys/RISER1
1.  /phys/RISER2
1.  /phys/RISER3
1.  /phys/RISER0/DOWNLINK
1.  /phys/RISER2/DOWNLINK
1.  /phys/RISER0/DOWNLINK/DIMM0
1.  /phys/RISER0/DOWNLINK/DIMM1
1.  /phys/RISER0/DOWNLINK/CPU
1.  /phys/RISER2/DOWNLINK/DIMM0
1.  /phys/RISER2/DOWNLINK/DIMM1
1.  /phys/RISER2/DOWNLINK/CPU
