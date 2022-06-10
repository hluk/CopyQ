class Kf6Kconfig < Formula
  desc "Configuration system"
  homepage "https://www.kde.org"
  url "https://download.kde.org/unstable/frameworks/5.249.0/kconfig-5.249.0.tar.xz"
  sha256 "7d24cbc6c12dd8cb25959fd4ffc232efb14719ebb8a3a9eefd40673a57ff4d0d"
  head "https://invent.kde.org/frameworks/kconfig.git"

  depends_on "cmake" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "copyq/kde/extra-cmake-modules" => [:build, :test]

  depends_on "qt"

  def install
    args = std_cmake_args
    args << "-DBUILD_TESTING=OFF"
    args << "-DBUILD_QCH=OFF"
    args << "-DKDE_INSTALL_QMLDIR=lib/qt6/qml"
    args << "-DKDE_INSTALL_PLUGINDIR=lib/qt6/plugins"
    args << "-DKDE_INSTALL_QTPLUGINDIR=lib/qt6/plugins"

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
    (testpath/"CMakeLists.txt").write("find_package(KF6Config REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
