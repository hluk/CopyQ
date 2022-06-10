class Kf6Kwindowsystem < Formula
  desc "Access to the windowing system"
  homepage "https://www.kde.org"
  url "https://download.kde.org/unstable/frameworks/5.249.0/kwindowsystem-5.249.0.tar.xz"
  sha256 "cc5e62380e1ef354a3088ddeb1db93d0e018e47e9ec0869a4b493ab737e81508"
  head "https://invent.kde.org/frameworks/kwindowsystem.git"

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

    mkdir "build" do
      system "cmake", "-G", "Ninja", "..", *args
      system "ninja"
      system "ninja", "install"
      prefix.install "install_manifest.txt"
    end
  end

  test do
    (testpath/"CMakeLists.txt").write("find_package(KF6WindowSystem REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
