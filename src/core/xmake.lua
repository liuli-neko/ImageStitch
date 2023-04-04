add_rules("mode.debug", "mode.release")

add_requires("opencv", "eigen", "glog","gtest", "qt5base", "nlohmann_json")
add_cxxflags("cl::/utf-8")
set_languages("c++17")

if is_host("windows") then
    add_defines("WIN")
end

includes("../ui")
includes("../signal")

target("common")
    set_kind("static")
    add_packages("opencv", "eigen", "glog", "nlohmann_json")
    add_headerfiles("common/*.hpp")
    add_files("common/*.cpp")

target("qtCommon")
    add_rules("qt.static")
    add_packages("opencv", "qt5base", "glog")
    add_headerfiles("qtCommon/*.hpp")
    add_files("qtCommon/*.cpp")
    add_frameworks("QtGui")

target("ImageStitchCore")
    set_kind("static")
    add_deps("signal")
    add_packages("opencv", "eigen", "glog", "nlohmann_json")
    add_headerfiles("imageStitch/*.h")
    add_files("imageStitcher/*.cpp")

target("parametersTest")
    set_kind("binary")
    add_packages("glog", "gtest", "nlohmann_json")
    add_deps("common")
    add_files("test/parametersTest.cpp")
    add_files("../gtest/testMain.cpp")

target("stitcherTest")
    add_rules("qt.widgetapp")
    add_packages("opencv", "eigen", "glog", "gtest", "qt5base", "nlohmann_json")
    add_deps("ImageStitchCore", "MUI", "qtCommon")
    add_files("test/stitcherTest.cpp")
    add_files("../gtest/testMain.cpp")