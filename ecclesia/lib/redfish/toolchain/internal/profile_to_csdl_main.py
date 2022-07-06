"""Runner for the profile_to_csdl library."""

import argparse
import os

from ecclesia.lib.redfish.toolchain.internal import profile_to_csdl


def main() -> None:
  parser = argparse.ArgumentParser(
      description='Trims the Redfish CSDL definitons to provide the minimum set'
      + 'of definitions to satisfy the provided Redfish Profile.')
  parser.add_argument(
      '--csdl_dir',
      type=str,
      required=True,
      help='relative dirpath of CSDL files')
  parser.add_argument(
      '--profile', type=str, required=True, help='profile filepath')
  parser.add_argument(
      '--out_dir', type=str, required=True, help='output dir of trimmed CSDL')

  args = parser.parse_args()

  schema_files = [
      os.path.join(args.csdl_dir, filename)
      for filename in os.listdir(args.csdl_dir)
  ]
  profile_to_csdl.profile_to_csdl(args.profile, schema_files, args.out_dir)


if __name__ == '__main__':
  main()
