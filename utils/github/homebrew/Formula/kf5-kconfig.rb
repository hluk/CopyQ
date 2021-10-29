class Kf5Kconfig < Formula
  desc "Configuration system"
  homepage "https://www.kde.org"
  url "https://download.kde.org/stable/frameworks/5.87/kconfig-5.87.0.tar.xz"
  sha256 "345bf5a68de968c3301aec16e1ec6eebd1c575a891464784aa0e177771e917d1"
  head "https://invent.kde.org/frameworks/kconfig.git"

  depends_on "cmake" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "copyq/kde/extra-cmake-modules" => [:build, :test]

  depends_on "qt@5"

  def install
    args = std_cmake_args
    args << "-DBUILD_TESTING=OFF"
    args << "-DBUILD_QCH=OFF"
    args << "-DKDE_INSTALL_QMLDIR=lib/qt5/qml"
    args << "-DKDE_INSTALL_PLUGINDIR=lib/qt5/plugins"
    args << "-DKDE_INSTALL_QTPLUGINDIR=lib/qt5/plugins"

    args << "-DKCONFIG_USE_GUI=OFF"
    args << "-DKCONFIG_USE_DBUS=OFF"

    mkdir "build" do
      system "cmake", "-G", "Ninja", "..", *args
      system "ninja"
      system "ninja", "install"
      prefix.install "install_manifest.txt"
    end
  end

  test do
    (testpath/"CMakeLists.txt").write("find_package(KF5Config REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
