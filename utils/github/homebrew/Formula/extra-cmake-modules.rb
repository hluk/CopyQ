class ExtraCmakeModules < Formula
  desc "Extra modules and scripts for CMake"
  homepage "https://api.kde.org/frameworks/extra-cmake-modules/html/index.html"
  url "https://download.kde.org/stable/frameworks/5.81/extra-cmake-modules-5.81.0.tar.xz"
  sha256 "5f57e4b843069b6098d955051bb2913558d1623fead3f3b95b7017d7e1e35b83"
  license all_of: ["BSD-2-Clause", "BSD-3-Clause", "MIT"]
  head "https://invent.kde.org/frameworks/extra-cmake-modules.git"

  depends_on "cmake" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "qt@5" => :build

  def install
    args = std_cmake_args
    args << "-DBUILD_HTML_DOCS=OFF"
    args << "-DBUILD_MAN_DOCS=OFF"
    args << "-DBUILD_QTHELP_DOCS=OFF"
    args << "-DBUILD_TESTING=OFF"

    mkdir "build" do
      system "cmake", "-G", "Ninja", "..", *args
      system "ninja"
      system "ninja", "install"
    end
  end

  test do
    (testpath/"CMakeLists.txt").write("find_package(ECM REQUIRED)")
    system "cmake", ".", "-Wno-dev"

    expected="ECM_DIR:PATH=#{HOMEBREW_PREFIX}/share/ECM/cmake"
    assert_match expected, File.read(testpath/"CMakeCache.txt")
  end
end
