class ExtraCmakeModules < Formula
  desc "Extra modules and scripts for CMake"
  homepage "https://api.kde.org/frameworks/extra-cmake-modules/html/index.html"
  url "https://download.kde.org/stable/frameworks/6.15/extra-cmake-modules-6.15.0.tar.xz"
  sha256 "f7cd022095a9e6bdbe5897720a24bfda81d211757b7c08b173061962bf2ee0b2"
  license all_of: ["BSD-2-Clause", "BSD-3-Clause", "MIT"]
  head "https://invent.kde.org/frameworks/extra-cmake-modules.git"

  depends_on "cmake" => [:build, :test]
  depends_on "ninja" => :build

  depends_on "qt" => :build

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
