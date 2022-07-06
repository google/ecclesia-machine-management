# Redfish Generated C++ Accessors

The overall motivations of the C++ accessors are:

*   **for clients**: provide a strongly typed library for accessing Redfish
    schemas and properties based on the official Redfish schema CSDL definitions
*   **for server maintainers**: force clients to write Redfish profiles

The accessor generators provide this mutually beneficial relationship between
C++ library ergonomics and Redfish Profiles for tracking client usage.

## Debugging

To view the raw sources, look in the genfiles directory.

If for debugging it helps to build the raw sources directly, you can directly
build the genrule:

```sh
# the redfish accessor target
my/build/path:accessors_label
# the genrule target appends __genrule
my/build/path:accessors_label__genrule
```

## C++ API

### Profiles

A profile becomes a toplevel class.

```json
{
  "SchemaDefinition": "RedfishInteroperabilityProfile.v1_0_0",
  "ProfileName": "Simple Test Profile",
  "ProfileVersion": "1.0.0",
  "Purpose": "A simple test for musical Redfish.",
  "ContactInfo": "conductor@g-major.music",
  "OwningEntity": "G Major",
  // ...
}
```

```cpp
// Generated class MachineManagerAssembly.
// Profile Version: 1.0.0
// Purpose: A simple test for musical Redfish.
// Contact Info: conductor@g-major.music
// Owning Entity: G Major.
class SimpleTestProfile {
  public:

  //...
};
```

### Schemas and properties

A schema becomes a nested class of the profile's class. The schema classes wrap
a RedfishObject. Primitive typed properties have accessor methods. Complex types
are currently unsupported.

```json
  "SchemaDefinition": "RedfishInteroperabilityProfile.v1_0_0",
  "ProfileName": "Simple Test Profile",
  "Resources": {
    "Chassis": {
      "PropertyRequirements": {
        "SerialNumber": {},
        "LocationIndicatorActive": {},
        "MinPowerWatts": {}
      }
    },
```

```cpp
class SimpleTestProfile {
  public:

  class Chassis {
    public:
      Chassis(RedfishObject *obj) : obj_(obj) {}

      std::optional<std::string> SerialNumber();
      std::optional<bool> LocationIndicatorActive();
      std::optional<int> MinPowerWatts();
    private:
      RedfishObject *obj_;
  };
  // ...
};
```

### ComplexTypes Enums

ComplexTypes and Enums are currently not supported and are just returned as
RedfishVariant.
