"""Test for devpath clif."""

from ecclesia.lib.devpath.python import devpath
from google3.testing.pybase import googletest


class DevpathTest(googletest.TestCase):

  def test_upstream_devpath_pass(self):
    disk_devpath = '/phys/PE0:connector:DOWNLINK'
    expected_devpath = '/phys/PE0'
    result_devpath = devpath.GetUpstreamDevpath(disk_devpath)
    self.assertEqual(result_devpath, expected_devpath)

  def test_upstream_devpath_fail(self):
    disk_devpath = '/phys'
    expected_devpath = None
    result_devpath = devpath.GetUpstreamDevpath(disk_devpath)
    self.assertEqual(result_devpath, expected_devpath)


if __name__ == '__main__':
  googletest.main()
