class Kf5Kcoreaddons < Formula
  desc "Addons to QtCore"
  homepage "https://www.kde.org"
  url "https://download.kde.org/stable/frameworks/5.109/kcoreaddons-5.109.0.tar.xz"
  sha256 "ff647fc1d4dd62370f261854af0870f2a1c7ba7abe7e276e5a4c42d923f15300"
  head "https://invent.kde.org/frameworks/kcoreaddons.git"

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
    args << "-DUPDATE_MIME_DATABASE_EXECUTABLE=OFF"

    mkdir "build" do
      system "cmake", "-G", "Ninja", "..", *args
      system "ninja"
      system "ninja", "install"
      prefix.install "install_manifest.txt"
    end
  end

  test do
    (testpath/"CMakeLists.txt").write("find_package(KF5CoreAddons REQUIRED)")
    system "cmake", ".", "-Wno-dev"
  end
end
