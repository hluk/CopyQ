class Kf6Knotifications < Formula
  desc "Abstraction for system notifications"
  homepage "https://www.kde.org"
  url "https://download.kde.org/stable/frameworks/6.7/knotifications-6.7.0.tar.xz"
  sha256 "9e972f015d9b31b3283bf842a32d270096224d127c13a6e79450f4f0452de5e7"
  head "https://invent.kde.org/frameworks/knotifications.git"

  depends_on "cmake" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "copyq/kde/extra-cmake-modules" => [:build, :test]
  depends_on "copyq/kde/kf6-kconfig"

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
    (testpath/"CMakeLists.txt").write("find_package(KF6Notifications REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
