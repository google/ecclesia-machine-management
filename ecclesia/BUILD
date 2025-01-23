# Define package groups for code to use to define various levels of visibility.
# Rules should always be one of:
#   * private (by default)
#   * a package group listed here
#   * public (rarely)
# If none of these are suitable for you then you may need a new group here.

licenses(["notice"])

exports_files(["LICENSE"])

# Code intended for GRPC Redfish usage
package_group(
    name = "redfish_grpc_users",
    packages = ["//ecclesia/..."],
)

# Code intended for redfish service and interoperability validation.
package_group(
    name = "redfish_validator_users",
    packages = ["//ecclesia/redfish_mockups/tests/..."],
)

# Machine agent shared libraries, for use in agent code.
package_group(
    name = "magent_library_users",
    packages = [
        "//ecclesia/lib/redfish/...",
        "//ecclesia/magent/...",
    ],
)

# Machine agent frontend code.
package_group(
    name = "magent_frontend_users",
    packages = ["//ecclesia/magent/daemons/..."],
)

# Machine manager frontend code.
package_group(name = "mmanager_frontend_users")

# Allowed users of the machine manager API.
package_group(
    name = "mmanager_users",
    packages = [
        "//ecclesia/lib/redfish/...",
        "//ecclesia/mmanager/...",
    ],
)

# Machine manager middle layer code.
package_group(
    name = "mmanager_middle_users",
    packages = ["//ecclesia/mmanager/middles/..."],
)

# Machine manager config code.
package_group(
    name = "mmanager_config_users",
    packages = ["//ecclesia/mmanager/..."],
)

# Tests which use the mmanager binary directly.
package_group(name = "mmanager_binary_tests")

# Query Engine users that need access to the raw RedfishInterface pointer.
package_group(
    name = "redfish_query_engine_raw_intf_users",
    packages = [
        "//ecclesia/lib/redfish/dellicius/engine/...",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine/...",
        "//ecclesia/lib/redfish/redpath/definitions/query_router/...",
    ],
)

# Query Engine users that need subquery url annotations.
package_group(
    name = "subquery_url_annotation",
    packages = [
        "//ecclesia/lib/redfish/dellicius/engine/...",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine/...",
    ],
)

# Query Engine users that want metrics per query.
package_group(
    name = "query_metrics",
    packages = [
        "//ecclesia/lib/redfish/dellicius/engine/...",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine/...",
    ],
)

# Query Engine users that want to log the redfish traces.
package_group(
    name = "log_redfish_traces",
    packages = [
        "//ecclesia/lib/redfish/dellicius/engine/...",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine/...",
    ],
)

# Jinja2 users.
#
# Jinja requires common infrastructure such as a file accessor implementation. Users of jinja2 in
# //third_party/ecclesia can use these common utilities.
package_group(
    name = "ecclesia_jinja_users",
    packages = [
        "//ecclesia/lib/redfish/toolchain/internal/...",
        "//ecclesia/tools/changelog/...",
    ],
)

# Machine manager Grpc Authentication Policy Configuration
#
# A new configuration setting to allow GRPC connections to use Google internal LOAS2 authentication
# credentials.
config_setting(
    name = "grpc_net_auth",
    define_values = {
        "grpc_creds_support": "loas2",
    },
    visibility = ["//visibility:private"],
)
