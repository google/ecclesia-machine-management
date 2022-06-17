"""Utility to generate Redfish accessor classes.

This binary does not accept any command line arguments. The accessors are
generated based entirely upon embedded data.

"""
import dataclasses
import sys
from typing import Sequence

import jinja2

from ecclesia.lib.jinja2 import loader

# Directory relative to ecclesia/.
_PACKAGE_DIR = 'ecclesia/lib/redfish/toolchain/accessors'


@dataclasses.dataclass(frozen=True)
class Resource:
  """A Redfish resource."""
  name: str


def main(argv: Sequence[str]) -> None:
  # We do not use any non-flag arguments.
  if len(argv) > 1:
    raise RuntimeError('Non-flag arguments are not accepted')

  # TODO(b/234745697): get these resources from the actual proto data.
  resources = []
  resources.append(Resource(name='ServiceRoot'))
  resources.append(Resource(name='Processor'))
  resources.append(Resource(name='Memory'))

  render_dict = {'resources': resources}

  # Use the constructed environment to render the template.
  jinja_env = jinja2.Environment(
      loader=loader.ResourceLoader(_PACKAGE_DIR, 'ecclesia'),
      undefined=jinja2.StrictUndefined)
  jinja_template = jinja_env.get_template('accessors.cc.jinja2')
  print(jinja_template.render(**render_dict))


if __name__ == '__main__':
  main(sys.argv)
