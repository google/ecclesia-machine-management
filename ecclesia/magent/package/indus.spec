# Copyright 2020 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Don't build debuginfo packages.
%define debug_package %{nil}

Name: google-ecclesia-management-agent
Epoch:   1
Version: %{_version}
Release: g1%{?dist}
Summary: Google Redfish machine management agent.
License: ASL 2.0
Url: https://github.com/google/ecclesia-machine-management

Source0: {magent_indus_oss.stripped}
Source1: {assemblies_tar.tar.gz}
Source2: {magent_indus.service}

BuildArch: %{_arch}

%description
Contains the Google Redfish machine management agent.

%prep

%build

%install
install -d %{buildroot}%{_bindir}
install -p -m 0755 {magent_indus_oss.stripped} %{buildroot}%{_bindir}/magent_indus
tar -xvf {assemblies_tar.tar.gz}
install -d %{buildroot}%{_sysconfdir}/google/magent
install -m 0644 assemblies/*.json %{buildroot}%{_sysconfdir}/google/magent
install -d %{buildroot}%{_sysconfdir}/systemd/system
install -m 0644 {magent_indus.service} %{buildroot}%{_sysconfdir}/systemd/system

%files
%{_bindir}/magent_indus
%{_sysconfdir}/google/magent
%{_sysconfdir}/systemd/system/magent_indus.service

%post
if [ $1 -eq 1 ]; then
  # Initial installation
  systemctl enable magent_indus.service >/dev/null 2>&1 || :

  if [ -d /run/systemd/system ]; then
    systemctl start magent_indus.service >/dev/null 2>&1 || :
  fi
else
  # Package upgrade
  if [ -d /run/systemd/system ]; then
    systemctl try-restart magent_indus.service >/dev/null 2>&1 || :
  fi
fi

%preun
if [ $1 -eq 0 ]; then
  # Package removal, not upgrade
  systemctl --no-reload disable magent_indus.service >/dev/null 2>&1 || :
  if [ -d /run/systemd/system ]; then
    systemctl stop magent_indus.service >/dev/null 2>&1 || :
  fi
fi