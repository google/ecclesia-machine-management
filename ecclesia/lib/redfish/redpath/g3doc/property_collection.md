# Redpath Query Property Collection

The Redpath query engine supports the ability to collect properties from
multiple resources into a flat list in the query result. This is useful when you
need to gather all instances of a specific property (e.g., all serial numbers)
across various resources without manually traversing the nested structure of
`QueryResultData`.

## How it Works

Property collection is triggered by the `collect_as` field in the
`RedfishProperty` message within a `DelliciusQuery`.

### Query Definition

When defining a property in a subquery, you can set the `collect_as` field to a
string identifier (e.g., "AllSerialNumbers"). If `collect_as` is specified, the
query engine will populate a map in the `QueryResult` with the values of this
property found in all resources. If multiple properties in one or more
subqueries specify the same `collect_as` key, they will be collected in the same
list in `QueryResult.collected_properties`.

```textproto
message DelliciusQuery {
  message Subquery {
    message RedfishProperty {
      string property = 1;
      string name = 2;
      ...
      // If specified, query engine will collect all values for this property
      // across all resources queried by this subquery and store them in
      // QueryResult.collected_properties map with collect_as as key.
      repeated string collect_as = 6;
    }
    ...
  }
}
```

### Query Result

The `QueryResult` message contains a `collected_properties` map field. If
`collect_as` is used in any subquery, this map will be populated. The **key** of
the map is the string provided in `collect_as`. The **value** is a
`CollectedProperties` message, which contains a list of `CollectedProperty`
messages.

Each `CollectedProperty` message includes the `Identifier` of the resource from
which the property was extracted, along with the `QueryValue` of the property
itself.

```textproto
message QueryResult {
  ...
  // Map of collected property name to list of values.
  map<string, CollectedProperties> collected_properties = 5;
}

message CollectedProperty {
  Identifier identifier = 1;
  QueryValue value = 2;
}

message CollectedProperties {
  repeated CollectedProperty properties = 1;
}
```

## Example

Assume you want to query all `Chassis` resources and collect their serial
numbers.

### Sample Query

```textproto
query_id: "all_chassis_serials"
subquery {
  subquery_id: "chassis"
  redpath: "/Chassis"
  properties {
    property: "SerialNumber"
    type: STRING
    collect_as: "serial_numbers"
  }
  properties {
    property: "Name"
    type: STRING
  }
}
```

### Sample Redfish Data

-   `/redfish/v1/Chassis/Chassis1`: `{ "@odata.id":
    "/redfish/v1/Chassis/Chassis1", "Name": "BigChassis", "SerialNumber":
    "SN123" }`
-   `/redfish/v1/Chassis/Chassis2`: `{ "@odata.id":
    "/redfish/v1/Chassis/Chassis2", "Name": "SmallChassis", "SerialNumber":
    "SN456" }`

### Resulting `QueryResult`

```textproto
query_id: "all_chassis_serials"
data {
  fields {
    key: "chassis"
    value {
      list_value {
        values {
          subquery_value {
            fields {
              key: "Name"
              value { string_value: "BigChassis" }
            }
            fields {
              key: "SerialNumber"
              value { string_value: "SN123" }
            }
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys/BIG" } }
            }
          }
        }
        values {
          subquery_value {
            fields {
              key: "Name"
              value { string_value: "SmallChassis" }
            }
            fields {
              key: "SerialNumber"
              value { string_value: "SN456" }
            }
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys/SMALL" } }
            }
          }
        }
      }
    }
  }
}
collected_properties {
  key: "serial_numbers"
  value {
    properties {
      identifier { local_devpath: "/phys/BIG" }
      value { string_value: "SN123" }
    }
    properties {
      identifier { local_devpath: "/phys/SMALL" }
      value { string_value: "SN456" }
    }
  }
}
```

## Collecting Across Subqueries

The `collect_as` tag can be used across different subqueries to aggregate
properties into a single list. If multiple properties in different subqueries
use the same `collect_as` tag, all collected values will appear in the same
`CollectedProperties` list in the result.

### Sample Query

This example collects serial numbers from both `Chassis` and `Drive` resources
into a single list called `serial_numbers`.

```textproto
query_id: "all_serials"
subquery {
  subquery_id: "chassis"
  redpath: "/Chassis"
  properties {
    property: "SerialNumber"
    type: STRING
    collect_as: "serial_numbers"
  }
}
subquery {
  subquery_id: "drives"
  redpath: "/Systems/system/Drives"
  properties {
    property: "SerialNumber"
    type: STRING
    collect_as: "serial_numbers"
  }
}
```

### Sample Redfish Data

-   `/redfish/v1/Chassis/Chassis1`: `{ "@odata.id":
    "/redfish/v1/Chassis/Chassis1", "SerialNumber": "CHAS123" }`
-   `/redfish/v1/Systems/system/Drives/Drive1`: `{ "@odata.id":
    "/redfish/v1/Systems/system/Drives/Drive1", "SerialNumber": "DRV456" }`

### Resulting `QueryResult`

```textproto
query_id: "all_serials"
data {
  fields {
    key: "chassis"
    value {
      list_value {
        values {
          subquery_value {
            fields {
              key: "SerialNumber"
              value { string_value: "CHAS123" }
            }
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys/CHASSIS" } }
            }
          }
        }
      }
    }
  }
  fields {
    key: "drives"
    value {
      list_value {
        values {
          subquery_value {
            fields {
              key: "SerialNumber"
              value { string_value: "DRV456" }
            }
            fields {
              key: "_id_"
              value { identifier { local_devpath: "/phys/SYS/DRIVE0" } }
            }
          }
        }
      }
    }
  }
}
collected_properties {
  key: "serial_numbers"
  value {
    properties {
      identifier { local_devpath: "/phys/CHASSIS" }
      value { string_value: "CHAS123" }
    }
    properties {
      identifier { local_devpath: "/phys/SYS/DRIVE0" }
      value { string_value: "DRV456" }
    }
  }
}
```

## Collecting a Single Property Under Multiple Keys

Since `collect_as` is a repeated field, a single property can be collected into
multiple lists.

### Sample Query

```textproto
query_id: "all_chassis_serials_in_two_lists"
subquery {
  subquery_id: "chassis"
  redpath: "/Chassis"
  properties {
    property: "SerialNumber"
    type: STRING
    collect_as: "serial_numbers"
    collect_as: "all_serials"
  }
}
```

### Resulting `QueryResult`

The `collected_properties` map will contain two entries, one for
`serial_numbers` and one for `all_serials`, both containing the same collected
property.

```textproto
collected_properties {
  key: "serial_numbers"
  value {
    properties {
      identifier { local_devpath: "/phys/CHASSIS" }
      value { string_value: "CHAS123" }
    }
  }
}
collected_properties {
  key: "all_serials"
  value {
    properties {
      identifier { local_devpath: "/phys/CHASSIS" }
      value { string_value: "CHAS123" }
    }
  }
}
```

Note: The `identifier` in `CollectedProperty` will be populated based on the
normalizer configuration (e.g., with `local_devpath` or `redfish_location`).
