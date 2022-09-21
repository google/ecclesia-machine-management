# Dellicius Library
This directory houses key modules that implement a layered architecture for
managing life cycle of a Dellicius Query.

## Dellicius Query Engine
Dellicius query is a query language that allows a redfish service to be accessed
as a single JSON document. The query needs an interpreter and dispatcher to
translate query operations into redfish resource requests and dispatch over
specific transport interface.
Dellicius Query Engine is a logical composition of interpreter, dispatcher and
normalizer, responsible for batch processing Redpath queries that are wrapped
with a specific data model within a Dellicius query.

The modular design of the query engine allows it be extended for manipulating,
tracing, decorating the Query requests and responses. A key use for extension is
for constructing devpaths by caching location metadata while traversing the tree
for a query.

### Dellicius Query
A query, uniquely identified in a set of queries by a query id,  is composed of
one or more subqueries and each subquery has following components:

1. **Redpath**: Query language based on XPath.

        <Redpath> ::= "/" <RelativeLocationPath> ;
        <RelativeLocationPath> ::= [<RelativeLocationPath> "/"] <Step> ;
        <Step> ::= <NodeTest>[<Predicate>] ;

    A redpath, based on XPath v1.0,  is a sequence of ‘Step’ expressions.
    A 'Step' expression is composed of 'NodeTest' and 'Predicate'. ‘NodeTest’ is
    the qualified name of redfish resource and ‘Predicate’ is filter operation
    to further refine the nodes returned for the NodeTest expression. Redpath
    uses abbreviated syntax where child axis is the default axis that narrows
    the scope of a ‘Step’ expression to children of the context node which is
    the base redfish resource for any ‘Step’ in the redpath. All redpaths begin
    with service root as the context node. 'Node Test' specifies the qualified
    name of the resource to match with. 'Predicate' is the expression to further
    refine the set of nodes selected by the location step.\
    Example:

        Redpath :- /Chassis[*]/Processor[*]
        Absolute Path :- /Chassis[*]/Processor[*]
        Relative Path :- Chassis[*]/Processor[*] and Processor[*]
        Location Step :- Chassis[*]
        Node Test :- Chassis
        Predicate :- [*]

2. **Property Requirements**:
   Each subquery calls out the properties to be queried
   from the filtered redfish node-set. An optional normalization specification
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
  requested redfish property name. A Dellicius query does not impose a
  relationship requirement between the subqueries. Although, performance wise,
  it is optimal to batch subqueries whose redpaths have common ancestors because
  the query engine  batch processes the subqueries and dispatches request only
  for unique resources to avoid redundant GET calls


### Dellicius Query Result
Dellicius query output comprises metadata for the query operation that describes
performance characteristics like query latency and a sequence of subquery
results.
Following are the key components of a Dellicius Query Result.

1. Query Identifier
2. Start and End Timestamp
3. Subquery Output:
4. Subquery Identifier
5. Status
6. Data:-

    **Devpath**: Uniquely identifies the source of the telemetry based on
             the physical topology of the system.

    **Key**: Identifies the redfish property parsed for this subquery when the
         engine is configured for no normalization. Else, identifies a custom
         property the redfish property maps to for property level normalization.

    **Value**: Value of requested redfish property.

## Usage

```c++
DelliciusQuery query = ParseTextFileAsProtoOrDie<DelliciusQuery>(path);
DelliciusQueryResult result;
// Instantiate query engine for specific configuration.
absl::StatusOr<
    std::unique_ptr<QueryInterface>> intf = GetQueryInterface(config);
if (intf.ok()) { (*intf)->ExecuteQuery(query, result); }
```

## References
[1] https://github.com/DMTF/libredfish#redpath

[2] https://www.w3.org/TR/1999/REC-xpath-19991116/
