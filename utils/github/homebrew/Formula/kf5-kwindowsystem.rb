class Kf5Kwindowsystem < Formula
  desc "Access to the windowing system"
  homepage "https://www.kde.org"
  url "https://download.kde.org/stable/frameworks/5.87/kwindowsystem-5.87.0.tar.xz"
  sha256 "424962f23bd03188ef2bb5854e77cac90b6b70632fcc1ccbed86bc72b9bab567"
  head "https://invent.kde.org/frameworks/kwindowsystem.git"

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

    mkdir "build" do
      system "cmake", "-G", "Ninja", "..", *args
      system "ninja"
      system "ninja", "install"
      prefix.install "install_manifest.txt"
    end
  end

  test do
    (testpath/"CMakeLists.txt").write("find_package(KF5WindowSystem REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
