# Dellicius Library
This directory houses key modules that implement a layered architecture for
managing life cycle of a Dellicius Query.

## Dellicius Query Engine
Dellicius query is a query language that allows a Redfish service to be accessed
as a single JSON document. The query needs an interpreter and dispatcher to
translate query operations into Redfish resource requests and dispatch over
specific transport interface.
Dellicius Query Engine is a logical composition of interpreter, dispatcher and
normalizer, responsible for batch processing RedPath queries that are wrapped
with a specific data model within a Dellicius query.

The modular design of the query engine allows it be extended for manipulating,
tracing, decorating the Query requests and responses. A key use for extension is
for constructing devpaths by caching location metadata while traversing the tree
for a query.

### Dellicius Query
A query, uniquely identified in a set of queries by a query id,  is composed of
one or more subqueries and each subquery has following components:

1. **RedPath**: Query language based on XPath.

        <RedPath> ::= "/" <RelativeLocationPath> ;
        <RelativeLocationPath> ::= [<RelativeLocationPath> "/"] <Step> ;
        <Step> ::= <NodeTest>[<Predicate>] ;

    A redpath, based on XPath v1.0,  is a sequence of ‘Step’ expressions.
    A 'Step' expression is composed of 'NodeTest' and 'Predicate'. ‘NodeTest’ is
    the qualified name of Redfish resource and ‘Predicate’ is filter operation
    to further refine the nodes returned for the NodeTest expression. RedPath
    uses abbreviated syntax where child axis is the default axis that narrows
    the scope of a ‘Step’ expression to children of the context node which is
    the base Redfish resource for any ‘Step’ in the redpath. All redpaths begin
    with service root as the context node. 'Node Test' specifies the qualified
    name of the resource to match with. 'Predicate' is the expression to further
    refine the set of nodes selected by the location step.\
    Example:

        RedPath :- /Chassis[*]/Processor[*]
        Absolute Path :- /Chassis[*]/Processor[*]
        Relative Path :- Chassis[*]/Processor[*] and Processor[*]
        Location Step :- Chassis[*]
        Node Test :- Chassis
        Predicate :- [*]

2. **Property Requirements**:
   Each subquery calls out the properties to be queried
   from the filtered Redfish node-set. An optional normalization specification
   for mapping properties to specific variables can be specified using the ‘key’
   attribute.
3. **Subquery Id**: String field uniquely identifying subquery in a
   query.

   Example:-

    ```textproto
    query_id: "AssemblyCollector"
    subquery {
        subquery_id: "Memory"
        redpath: "/Systems[*]/Memory[*]"
        properties { key: "serial_number" property: "SerialNumber"}
        properties { key: "part_number" property: "PartNumber"}
    }

    subquery {
        subquery_id: "Processors"
        redpath: "/Systems[*]/Processors[*]"
        properties { key: "serial_number" property: "SerialNumber"}
        properties { key: "part_number" property: "PartNumber"}
    }

    subquery {
        subquery_id: "Chassis"
        redpath: "/Chassis[*]"
        properties { key: "serial_number" property: "SerialNumber"}
        properties { key: "part_number" property: "PartNumber"}
    }
    ```

>'Key' is an optional attribute  in the properties specification within a
  subquery. The default normalization level is no normalization and data shall
  transparently be sent to the client with the value of key attribute set to the
  requested Redfish property name. A Dellicius query does not impose a
  relationship requirement between the subqueries. Although, performance wise,
  it is optimal to batch subqueries whose redpaths have common ancestors because
  the query engine  batch processes the subqueries and dispatches request only
  for unique resources to avoid redundant GET calls


### Dellicius Query Result
Dellicius query output comprises metadata for the query operation that describes
performance characteristics like query latency and a sequence of subquery
results.

Following are the key components of a Dellicius Query Result:

1. **Query Identifier**
2. **Start Timestamp**: Represents the point in time when a delicious query is received by the query engine.
3. **End Timestamp**: Represents the point in time when the  last subquery response within a delicious query is processed by the query engine.
4. **Subquery Output**: Map where key is unique subquery id string and value is collection of data sets. A data set in this context is a subset of properties collected from a Redfish resource per the property specification in the corresponding subquery of a Dellicius query. A data set has following properties:
    1. **Devpath**: Uniquely identifies the source of the dataset based on the physical topology of the system.
    2. **Data**: Repeated field capturing the requested property and the identifier.
        1. **Name**: Identifies the Redfish property parsed for this subquery when the engine is configured for no normalization. Else, represents a variable the Redfish property is mapped to for property level normalization.
        2. **Value**: Value of requested Redfish property.

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
