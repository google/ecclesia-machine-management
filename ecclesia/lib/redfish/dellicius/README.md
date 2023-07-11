# Redfish Query Library
Formerly known as Dellicius, this directory houses key modules that implement a layered architecture for managing life cycle of a Redfish Query.

## Redfish Query Engine
A Redfish Query is a proto that encapsulates one or more RedPath expressions and optional property requirements.
RedPath is a string syntax inspired by XPath1.0 and spec’d in DMTF Client specification to allow a client to specify path to Redfish resources and properties in the Redfish data model. It allows a Redfish client to query Redfish Service as if it were a single JSON document.
A Redfish query needs an interpreter and dispatcher to
translate query operations into Redfish resource requests and dispatch over
specific transport interface.
Redfish Query Engine is a logical composition of interpreter, dispatcher and
normalizer, responsible for batch processing RedPath queries that are wrapped
with a specific data model within a Redfish query.

The modular design of the query engine allows it be extended for manipulating,
tracing, decorating the Query requests and responses. A key use for extension is
for constructing devpaths by caching location metadata while traversing the tree
for a query.

### Redfish Query
A query, uniquely identified in a set of queries by a query id,  is composed of
one or more subqueries and each subquery has following components:

1. **redpath**: String based on XPath path expression.

        <RedPath> ::= "/" <RelativeLocationPath> ;
        <RelativeLocationPath> ::= [<RelativeLocationPath> "/"] <Step> ;
        <Step> ::= <NodeName>[<Predicate>] ;

    A redpath, based on XPath v1.0,  is a sequence of ‘Step’ expressions.
    A 'Step' expression is composed of 'NodeName' and 'Predicate'. ‘NodeName’ is
    the qualified name of Redfish resource and ‘Predicate’ is filter operation
    to further refine the nodes returned for the NodeName expression. RedPath
    uses abbreviated syntax where child axis is the default axis that narrows
    the scope of a ‘Step’ expression to children of the context node which is
    the base Redfish resource for any ‘Step’ in the redpath. All redpaths begin
    with service root as the context node. 'NodeName' specifies the qualified
    name of the resource to match with. 'Predicate' is the expression to further
    refine the set of nodes selected by the location step.\
    Example:

        RedPath :- /Chassis[*]/Processor[*]
        Absolute Path :- /Chassis[*]/Processor[*]
        Relative Path :- Chassis[*]/Processor[*] and Processor[*]
        Location Step :- Chassis[*]
        NodeName :- Chassis
        Predicate :- [*]

2. **properties**:
   Each subquery calls out the properties to be queried
   from the filtered Redfish node-set. An optional normalization specification
   for mapping properties to specific variables can be specified using the ‘name’
   attribute.
3. **subquery_id**: String field uniquely identifying subquery in a
   query.
4. **root_subquery_ids**: Repeated string attribute, identifying parent subquery relative to which the given subquery has to execute.

   Example:-

    ```textproto
    query_id: "FirmwareListWithRelatedItem"
    subquery {
      subquery_id: "SoftwareInventory"
      redpath: "/UpdateService/FirmwareInventory[*]"
      properties { property: "Name" type: STRING }
      properties { property: "Version" type: STRING }
    }
    subquery {
      subquery_id: "RelatedItem"
      root_subquery_ids: "SoftwareInventory"
      redpath: "/RelatedItem[*]"
      properties { name: "ServiceLabel" property: "Location.PartLocation.ServiceLabel" type: STRING }
    }
    ```

5. **name**: Optional attribute in the properties specification within a
  subquery to normalize a Redfish property value into a given variable name.

  The default normalization level is no normalization and data shall
  transparently be sent to the client with the value of name attribute set as the
  requested Redfish property name.


  >Performance wise,
  it is optimal to batch subqueries whose redpaths have common parent RedPath prefix since
  the query engine batch processes all subqueries and dispatches request only
  for unique RedPath prefix to avoid redundant GET calls.


### Redfish Query Result
Redfish query output is a collection of subquery results.

Following are the key components of a Redfish Query Result:

1. **query_id**: Identifies the Redfish Query for the given output.
2. **start_timestamp**: Represents the point in time when a delicious query is received by the query engine.
3. **end_timestamp**: Represents the point in time when the  last subquery response within a delicious query is processed by the query engine.
4. **subquery_output_by_id**: Map where key is unique subquery id string and value is collection of data sets. A data set in this context is a subset of properties collected from a Redfish resource per the property requirements in the corresponding subquery of a Redfish query. A data set has following properties:
    1. **devpath**: Uniquely identifies the source of the dataset based on the physical topology of the system.
    2. **data**: Repeated field capturing the requested property and the identifier.
        1. **name**: Identifies the Redfish property parsed for this subquery when the engine is configured for no normalization. Else, represents a variable the Redfish property is mapped to for property level normalization.
        2. **[type]_value**: Value of requested Redfish property.

```textproto
query_id: "SensorCollector"
subquery_output_by_id {
  key: "Sensors"
  value {
    data_set {
      devpath: "/phys/SYS_FAN0"
      data {
        name: "Name"
        string_value: "fan0"
      }
      data {
        name: "ReadingType"
        string_value: "Rotational"
      }
      data {
        name: "ReadingUnits"
        string_value: "RPM"
      }
      data {
        name: "Reading"
        int64_value: 16115
      }
    }
    data_set {
      devpath: "/phys/SYS_FAN1"
      data {
        name: "Name"
        string_value: "fan1"
      }
      data {
        name: "ReadingType"
        string_value: "Rotational"
      }
      data {
        name: "ReadingUnits"
        string_value: "RPM"
      }
      data {
        name: "Reading"
        int64_value: 16115
      }
    }
  }
start_timestamp {
  seconds: 10
}
end_timestamp {
  seconds: 10
}
```

## Usage

```c++
FakeRedfishServer server("indus_hmb_shim/mockup.shar");
std::unique_ptr<RedfishInterface> intf = server.RedfishClientInterface();
QueryEngineConfiguration config{
    .flags{.enable_devpath_extension = true,
           .enable_cached_uri_dispatch = false},
    .query_files{kDelliciusQueries.begin(), kDelliciusQueries.end()}};
QueryEngine query_engine(config, &clock, std::move(intf));
std::vector<DelliciusQueryResult> response_entries =
    query_engine.ExecuteQuery({"SensorCollector"});
```

## References
[1] https://github.com/DMTF/libredfish#redpath

[2] https://www.w3.org/TR/1999/REC-xpath-19991116/
