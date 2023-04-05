class Kf5Knotifications < Formula
  desc "Abstraction for system notifications"
  homepage "https://www.kde.org"
  url "https://download.kde.org/stable/frameworks/5.103/knotifications-5.103.0.tar.xz"
  sha256 "8f3ccd6a0303408fecb1e5d9ceb22cbdbf9b1ceb08a92d32b7167dd5e2a8d936"
  head "https://invent.kde.org/frameworks/knotifications.git"

  depends_on "cmake" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "copyq/kde/extra-cmake-modules" => [:build, :test]
  depends_on "copyq/kde/kf5-kconfig"
  depends_on "copyq/kde/kf5-kcoreaddons"
  depends_on "copyq/kde/kf5-kwindowsystem"
  depends_on "libcanberra"

  def install
    args = std_cmake_args
    args << "-DBUILD_TESTING=OFF"
    args << "-DBUILD_QCH=OFF"
    args << "-DKDE_INSTALL_QMLDIR=lib/qt5/qml"
    args << "-DKDE_INSTALL_PLUGINDIR=lib/qt5/plugins"
    args << "-DKDE_INSTALL_QTPLUGINDIR=lib/qt5/plugins"
    # setBadgeLabelText method is deprecated since 5.12
    args << "-DCMAKE_C_FLAGS_RELEASE=-DNDEBUG -DQT_DISABLE_DEPRECATED_BEFORE=0x050b00"
    args << "-DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG -DQT_DISABLE_DEPRECATED_BEFORE=0x050b00"

    mkdir "build" do
      system "cmake", "-G", "Ninja", "..", *args
      system "ninja"
      system "ninja", "install"
      prefix.install "install_manifest.txt"
    end
  end

  test do
    (testpath/"CMakeLists.txt").write("find_package(KF5Notifications REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
