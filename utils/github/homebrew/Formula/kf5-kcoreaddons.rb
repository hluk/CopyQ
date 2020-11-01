class Kf5Kcoreaddons < Formula
  desc "Addons to QtCore"
  homepage "https://www.kde.org"
  url "https://download.kde.org/stable/frameworks/5.75/kcoreaddons-5.75.0.tar.xz"
  sha256 "9ca2cf82ddc12e27ff73aaefdf616c3eb6552a3a5531f014b5bf52a34a4c73f9"
  head "https://invent.kde.org/frameworks/kcoreaddons.git"

  depends_on "cmake" => [:build, :test]
  depends_on "kde-extra-cmake-modules" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "qt"

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
