"""Jinja2 loader that supports reading from Ecclesia runfiles.

When running a Python binary from blaze-bin, this loads templates from the
blaze-bin directory.

ecclesia:google3-begin(internal docs)

We expect templates to only be declared in the Ecclesia subdirectory because
all Redfish client libraries will be owned by the Ecclesia team. If this is
not true, then this library will need to be readapted to work within other
directories as well.

This implementation is mostly a stripped down version of the internal
resources.py implementation in google3/pyglib/resources.py

ecclesia:google3-end
"""

import os.path
from typing import Any, Callable, Optional, Tuple

import jinja2


def _DeriveEcclesiaRootFromThisFile(this_filename: str,
                                    common_root: str) -> Optional[str]:
  """Get the ecclesia root directory.

  Args:
    this_filename: The complete ecclesia path to this file (loader.py).
    common_root: A directory with a common root with this file.

  Returns:
    The root path of this file.
  """
  root = None
  if this_filename:
    # ecclesia:google3-begin(internal docs)
    # This is how google3/pyglib/resources.py does it too. Call dirname
    # the same number of times as this file is nested to find the root dir.
    #
    # This function breaks if this file is moved to a different root than any
    # of its clients.
    #
    # this_filename is like google3/ecclesia/lib/jinja2/loader.py
    # a common_root can be like "ecclesia".
    #
    # pyglib uses google3 as the common_root.
    # This library is configurable as bazel will not have google3 as a dir.
    # ecclesia:google3-end
    tentative_root = os.path.dirname(os.path.abspath(this_filename))
    while tentative_root and os.path.basename(tentative_root) != common_root:
      tentative_root = os.path.dirname(tentative_root)
    if tentative_root:
      root = os.path.dirname(tentative_root)
  return root


def _IsSubPath(root, path: str) -> bool:
  """Determine whether resource path is contained within root.

  Args:
    root: The path in which the resource is expected to be found.
    path: A resource path.

  Returns:
    True if "path" is a relative path or if it is an absolute path
    pointing within "root".
  """
  root = os.path.abspath(root)
  if not os.path.isabs(path):
    path = os.path.join(root, path)
  path = os.path.normpath(path)
  return os.path.commonprefix([root, path]) == root


def _FindResource(path: str, common_root: str) -> str:
  """Returns the absolute path for a resource."""
  name = os.path.normpath(path)
  root = _DeriveEcclesiaRootFromThisFile(__file__, common_root)
  if root and _IsSubPath(root, name):
    filename = os.path.join(root, name)
    if os.path.isfile(filename):
      return filename
  raise FileNotFoundError('Could not find absolute path for {}'.format(path))


class ResourceLoader(jinja2.BaseLoader):
  """Jinja2 loader that supports reading from ecclesia runfiles."""

  def __init__(self, path: str, common_root: str):
    """Constructor.

    Arguments:
      path: the path of the directory contaiing the templates, relative to
        common_root.
      common_root: a common root directory between this file (loader.py) and the
        directory containing all of the template files.  For example, for some
        files like "ecclesia/lib/my/subdir/templates/mytemplate.cc.jinja2"
        "ecclesia/lib/my/subdir/templates/mytemplate.h.jinja2"  path would be
        "ecclesia/lib/my/subdir/templates" common_root would be "ecclesia"
    """
    super(ResourceLoader, self).__init__()
    self.path = path
    self.common_root = common_root

  # pylint: disable=g-bad-name
  def get_source(self, unused_environment: Any,
                 template: str) -> Tuple[str, str, Callable[[], bool]]:
    """Get the source for a template.

    Loads the template from runfiles using google3.pyglib.resources. See
    module comment for differences when running from blaze-bin vs par file.

    See http://jinja.pocoo.org/docs/api/#jinja2.BaseLoader.get_source.

    Arguments:
      unused_environment: Current Jinja2 environment.
      template: Template name.

    Returns:
      A tuple in the form of (source, filename, uptodate):
        source: The source for the template (Unicode string).
        filename: None as we load the file using google3.pyglib.resources.
        uptodate: Function that will be called to see if the template is still
            up to date. If running from blaze-bin, checks mtime, otherwise
            (running from PAR), always returns True.
    """
    path = os.path.join(self.path, template)
    try:
      filename = _FindResource(path, self.common_root)
      with open(filename, mode='rb') as f:
        data = f.read()
      mtime = os.path.getmtime(filename)
      uptodate = lambda: os.path.getmtime(filename) == mtime
    except IOError as ioerror:
      raise jinja2.TemplateNotFound(template) from ioerror
    data = data.decode('utf-8')
    return (data, path, uptodate)
