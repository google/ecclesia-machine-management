# Define package groups for code to use to define various levels of visibility.
# Rules should always be one of:
#   * private (by default)
#   * a package group listed here
#   * public (rarely)
# If none of these are suitable for you then you may need a new group here.

licenses(["notice"])

exports_files(["LICENSE"])

# Code allowed to use any of our generic libraries.
package_group(
    name = "library_users",
    packages = ["//ecclesia/..."],
)

# Code intended for use in the Redfish backend.
package_group(
    name = "redfish_users",
    packages = [
        "//ecclesia/lib/http/...",
        "//ecclesia/lib/redfish/...",
        "//ecclesia/magent/redfish/...",
        "//ecclesia/mmanager/backends/redfish/...",
        "//ecclesia/mmanager/frontends/...",
        "//ecclesia/mmanager/middles/collector/...",
        "//ecclesia/redfish_mockups/tests/...",
    ],
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
        "//ecclesia/magent/...",
        "//ecclesia/redfish/profiles/...",
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
    packages = ["//ecclesia/mmanager/..."],
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
