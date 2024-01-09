%global project CopyQ
%global revision _REVISION_
%global snapinfo _DATE_._REV_
%global branch _BRANCH_
%global last_version _LAST_VERSION_
Name:		copyq
Summary:	Advanced clipboard manager
Version:	%{last_version}.99.%{branch}
Release:	1.%{snapinfo}%{?dist}
License:	GPLv3+
Url:		https://hluk.github.io/%{project}/
Source0:	https://github.com/hluk/%{project}/archive/%{revision}.tar.gz
BuildRequires:	libXtst-devel, libXfixes-devel, desktop-file-utils
BuildRequires:	kf5-rpm-macros, qt5-qtbase-devel, qt5-qtsvg-devel
BuildRequires:	qt5-qttools-devel, qt5-qtscript-devel, qwt-qt5-devel, qt5-qtx11extras-devel
BuildRequires:	extra-cmake-modules, appstream-qt-devel, libappstream-glib
BuildRequires:	qt5-qtbase-private-devel
BuildRequires:	qt5-qtwayland-devel wayland-devel
BuildRequires:	kf5-knotifications-devel

# Following line used to track qt5 private api usage
%{?_qt5:Requires: %{_qt5}%{?_isa} = %{_qt5_version}}

%description
CopyQ is advanced clipboard manager with searchable and editable history with
support for image formats, command line control and more.

%prep
chmod 644 %{SOURCE0}
%setup -qn %{project}-%{revision}
sed -i 's/^set(copyq_version .*)$/set(copyq_version "%{version}-%{snapinfo}")/' src/version.cmake
grep -Fq '"v%{version}-%{snapinfo}"' src/version.cmake

%build
%cmake_kf5 \
				-DWITH_TESTS=TRUE \
				-DPLUGIN_INSTALL_PREFIX=%{_libdir}/%{name}/plugins \
				-DTRANSLATION_INSTALL_PREFIX:PATH=%{_datadir}/%{name}/locale

%cmake_build

%install
%cmake_install
%find_lang %{name} --with-qt

%check
desktop-file-validate $RPM_BUILD_ROOT%{_datadir}/applications/com.github.hluk.%{name}.desktop
appstream-util validate-relax --nonet $RPM_BUILD_ROOT%{_datadir}/metainfo/com.github.hluk.%{name}.appdata.xml

%files -f %{name}.lang
%doc AUTHORS CHANGES.md HACKING README.md
%license LICENSE
%{_bindir}/%{name}
%{_libdir}/%{name}/
%{_datadir}/bash-completion/completions/%{name}
%{_datadir}/metainfo/com.github.hluk.%{name}.appdata.xml
%{_datadir}/applications/com.github.hluk.%{name}.desktop
%dir %{_datadir}/icons/hicolor/*/
%dir %{_datadir}/icons/hicolor/*/apps/
%{_datadir}/icons/hicolor/*/apps/%{name}*.png
%{_datadir}/icons/hicolor/*/apps/%{name}*.svg
%dir %{_datadir}/%{name}/
%dir %{_datadir}/%{name}/locale/
%{_datadir}/%{name}/themes/
%{_mandir}/man1/%{name}.1.*

%changelog
* Fri Oct 16 2020 Gerald Cox <gbcox@fedoraproject.org> - 3.13.0-1
- Reverse 3.12.0-6 changes (#1888764)
- Upstream release - rhbz#1888986

* Thu Oct 15 2020 Rex Dieter <rdieter@fedoraproject.org> - 3.12.0-6
- drop BR: qt5-qtbase-private-devel, minor cleanup (#1888764)

* Thu Oct 15 2020 Gerald Cox <gbcox@fedoraproject.org> - 3.12.0-5
- rhbz#1888764

* Thu Aug 06 2020 Gerald Cox <gbcox@fedoraproject.org> - 3.12.0-4
- rhbz#1863364

* Sat Aug 01 2020 Fedora Release Engineering <releng@fedoraproject.org> - 3.12.0-3
- Second attempt - Rebuilt for
  https://fedoraproject.org/wiki/Fedora_33_Mass_Rebuild

* Mon Jul 27 2020 Fedora Release Engineering <releng@fedoraproject.org> - 3.12.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_33_Mass_Rebuild

* Sun Jul 12 2020 Gerald Cox <gbcox@fedoraproject.org> - 3.12.0-1
- Upstream release rhbz#1856080

* Fri May 08 2020 Gerald Cox <gbcox@fedoraproject.org> - 3.11.1-1
- Upstream release rhbz#1830240

* Fri May 01 2020 Gerald Cox <gbcox@fedoraproject.org>- 3.11.0-1
- Upstream release rhbz#1830240

* Sun Feb 02 2020 Gerald Cox <gbcox@fedoraproject.org>- 3.10.0-1
- Upstream release rhbz#1797335

* Tue Jan 28 2020 Fedora Release Engineering <releng@fedoraproject.org> - 3.9.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_32_Mass_Rebuild

* Sat Nov 16 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.9.3-1
- Upstream release rhbz#1773189

* Tue Oct 22 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.9.2-2
- Add rpmlintrc

* Sun Aug 25 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.9.2-1
- Upstream release rhbz#1742997

* Sun Aug 18 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.9.1-1
- Upstream release rhbz#1742997

* Wed Jul 24 2019 Fedora Release Engineering <releng@fedoraproject.org> - 3.9.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_31_Mass_Rebuild

* Thu Jun 27 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.9.0-1
- Upstream release rhbz#1724540

* Wed Apr 10 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.8.0-1
- Upstream release rhbz#1698544

* Mon Feb 04 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.7.3-2
- Patch for missing tray icon rhbz#1672064

* Sun Feb 03 2019 Gerald Cox <gbcox@fedoraproject.org> - 3.7.3-1
- Upstream release rhbz#1672064

* Thu Jan 31 2019 Fedora Release Engineering <releng@fedoraproject.org> - 3.7.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Mon Dec 31 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.7.2-1
- Upstream release rhbz#1662682

* Sun Nov 18 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.7.1-1
- Upstream release rhbz#1651015

* Sun Nov 04 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.7.0-1
- Upstream release rhbz#1645873

* Tue Sep 25 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.6.1-1
- Upstream release rhbz#1632031

* Sun Sep 23 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.6.0-1
- Upstream release rhbz#1632031

* Thu Jul 12 2018 Fedora Release Engineering <releng@fedoraproject.org> - 3.5.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Sun Jun 17 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.5.0-1
- Upstream release rhbz#1592132

* Sun Apr 29 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.4.0-1
- Upstream release rhbz#1573011

* Fri Apr 13 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.3.1-1
- Upstream release rhbz#1567094

* Sat Mar 17 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.3.0-1
- Upstream release rhbz#1557681

* Sun Feb 18 2018 Gerald Cox <gbcox@fedoraproject.org> - 3.2.0-1
- Upstream release rhbz#1546543

* Wed Feb 07 2018 Fedora Release Engineering <releng@fedoraproject.org> - 3.1.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Thu Jan 11 2018 Igor Gnatenko <ignatenkobrain@fedoraproject.org> - 3.1.2-2
- Remove obsolete scriptlets

* Sun Oct 22 2017 Gerald Cox <gbcox@fedoraproject.org> - 3.1.2-1
- Upstream release rhbz#1505132

* Thu Sep 28 2017 Gerald Cox <gbcox@fedoraproject.org> - 3.1.1-1
- Upstream release rhbz#1496733

* Thu Sep 28 2017 Gerald Cox <gbcox@fedoraproject.org> - 3.1.0-1
- Upstream release rhbz#1496733

* Wed Aug 02 2017 Fedora Release Engineering <releng@fedoraproject.org> - 3.0.3-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Wed Jul 26 2017 Fedora Release Engineering <releng@fedoraproject.org> - 3.0.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Sun Jul 02 2017 Gerald Cox <gbcox@fedoraproject.org> - 3.0.3-1
- Upstream release rhbz#1467112

* Sat Jun 03 2017 Gerald Cox <gbcox@fedoraproject.org> - 3.0.2-1
- Upstream release rhbz#1456802

* Tue May 09 2017 Gerald Cox <gbcox@fedoraproject.org> - 3.0.1-1
- Upstream release rhbz#1449207

* Tue Apr 04 2017 Gerald Cox <gbcox@fedoraproject.org> - 3.0.0-1
- Upstream release rhbz#1438781

* Fri Feb 17 2017 Gerald Cox <gbcox@fedoraproject.org> - 2.9.0-1
- Upstream release rhbz#1423475

* Fri Feb 10 2017 Fedora Release Engineering <releng@fedoraproject.org> - 2.8.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Mon Jan 30 2017 Gerald Cox <gbcox@fedoraproject.org> - 2.8.3-1
- Upstream release rhbz#1417607

* Sun Jan 15 2017 Gerald Cox <gbcox@fedoraproject.org> - 2.8.2-1
- Upstream release rhbz#1413359

* Sat Dec 03 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.8.1-1
- Upstream release rhbz#1401190

* Tue Nov 22 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.8.0-1
- Upstream release rhbz#1397608

* Sun Jul 24 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.7.1-3
- Open tray menu on left mouse click rhbz#1359526

* Sat Jul 16 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.7.1-2
- Make translations available rhbz#1357235

* Sun Jun 19 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.7.1-1
- Upstream release rhbz#1347965

* Sun May 1 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.7.0-1
- Upstream release rhbz#1332032

* Sun Feb 14 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.6.1-1
- Upstream release rhbz#1308340

* Sat Feb 06 2016 Gerald Cox <gbcox@fedoraproject.org> - 2.6.0-1
- Upstream release rhbz#1305247

* Wed Feb 03 2016 Fedora Release Engineering <releng@fedoraproject.org> - 2.5.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Sat Nov 14 2015 Gerald Cox <gbcox@fedoraproject.org> 2.5.0-1
- Upstream release rhbz#1282017

* Sun Aug 30 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.9-1
- Upstream release rhbz#1258225

* Tue Jul 7 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.8-2
- Upstream release rhbz#1240642

* Tue Jul 7 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.8-1
- Upstream release rhbz#1240642

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.4.7-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Mon Jun 1 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.7-1
- Upstream release rhbz#1227038

* Fri May 8 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.6-6
- Change requirement from Qt4 to Qt5

* Fri May 1 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.6-5
- Deactivate DEBUG messages rhbz#1217874

* Sat Apr 25 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.6-4
- Remove superfluous explicit requires rhbz#1211831

* Fri Apr 24 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.6-3
- Resolve duplicate file warnings, runtime dependencies rhbz#1211831

* Fri Apr 24 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.6-2
- Enable end user testing - http://goo.gl/ue6e2F rhbz#1211831

* Tue Apr 14 2015 Gerald Cox <gbcox@fedoraproject.org> 2.4.6-1
- Initial Build 2.4.6-1 rhbz#1211831
