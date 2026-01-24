class Kf6Kstatusnotifieritem < Formula
  desc "Abstraction for status/tray"
  homepage "https://www.kde.org"
  url "https://download.kde.org/stable/frameworks/6.15/kstatusnotifieritem-6.15.0.tar.xz"
  sha256 "24132cc967570478b54e44eef1063c193217801260dc51860e0881fa0d90bd0d"
  head "https://invent.kde.org/frameworks/kstatusnotifieritem.git"

  depends_on "cmake" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "copyq/kde/extra-cmake-modules" => [:build, :test]
  depends_on "copyq/kde/kf6-kconfig"
  depends_on "copyq/kde/kf6-kwindowsystem"

  def install
    args = std_cmake_args

    args << "-DEXCLUDE_DEPRECATED_BEFORE_AND_AT=5.90.0"

    args << "-DBUILD_TESTING=OFF"
    args << "-DBUILD_QCH=OFF"
    args << "-DKDE_INSTALL_QMLDIR=lib/qt6/qml"
    args << "-DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins"
    args << "-DKDE_INSTALL_QTPLUGINDIR=lib/qt6/plugins"
    args << "-DUSE_DBUS=OFF"

    mkdir "build" do
      system "cmake", "-G", "Ninja", "..", *args
      system "ninja"
      system "ninja", "install"
      prefix.install "install_manifest.txt"
    end
  end

  test do
    (testpath/"CMakeLists.txt").write("find_package(KF6Statusnotifieritem REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
