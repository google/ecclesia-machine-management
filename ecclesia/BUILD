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

# Allowed users of the machine manager API.
package_group(
    name = "mmanager_users",
    packages = [
        "//ecclesia/lib/redfish/...",
        "//ecclesia/lib/root_of_trust/...",
        "//ecclesia/mmanager/...",
        "//ecclesia/node/...",
    ],
)

# Allowed users of the machine manager service metadata.
#
# Note that the metadata attached to the service definition is NOT considered to
# be part of the public API and is subject to change is more frequent and less
# compatible ways. Code outside of the mmanager code itself should not be
# accessing the service metadata.
package_group(
    name = "mmanager_metadata_users",
    packages = [
        "//ecclesia/mmanager/middles/aggregator/...",
        "//ecclesia/mmanager/service/...",
    ],
)

# Machine manager shared libraries
package_group(
    name = "mmanager_library_users",
    packages = ["//ecclesia/mmanager/..."],
)

# Machine manager backend code.
package_group(
    name = "mmanager_backend_users",
    packages = [
        "//ecclesia/mmanager/backends/...",
        "//ecclesia/mmanager/frontends/...",
        "//ecclesia/mmanager/middles/...",
    ],
)

# Machine manager middle layer code.
package_group(
    name = "mmanager_middle_users",
    packages = [
        "//ecclesia/mmanager/frontends/...",
        "//ecclesia/mmanager/middles/...",
    ],
)

# Machine manager frontend code.
package_group(
    name = "mmanager_frontend_users",
    packages = ["//ecclesia/mmanager/mock/..."],
)

# Machine manager config code.
package_group(
    name = "mmanager_config_users",
    packages = ["//ecclesia/mmanager/..."],
)

# Users of the machine manager mock daemon.
package_group(
    name = "mmanager_mock_users",
    packages = ["//ecclesia/mmanager/mock/..."],
)

# Tests which use the mmanager binary directly.
package_group(name = "mmanager_binary_tests")

# Machine Manager functest package users.
package_group(
    name = "mmanager_functest_users",
    packages = ["//ecclesia/mmanager/testing/..."],
)

# Machine manager shared resource libraries
#
# The libraries which are visible to this group are intended for the very
# limited use case where code requires dependencies on ALL resource
# identifiers or RPC messages, and should not be utilized by clients
# outside of mmanager.
#
# The default and preferred option for including resource dependencies should
# always be directly importing only the resource messages explicitly required
# by code, wherever possible.
package_group(
    name = "mmanager_resource_library_users",
    packages = [
        "//ecclesia/mmanager/mock/...",
        "//ecclesia/mmanager/service/...",
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
    visibility = ["//ecclesia/mmanager/__subpackages__"],
)
