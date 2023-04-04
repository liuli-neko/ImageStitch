
add_rules("mode.debug", "mode.release", "mode.releasedbg")

add_requires("opencv","qt5base","glog","gtest", "nlohmann_json")
add_cxxflags("cl::/utf-8")
set_languages("c++17")

if is_host("windows") then
    add_defines("WIN")
end

includes("src/signal")
includes("src/core")
includes("src/ui")
-- includes("src/gtest")

target("ImageStitch")
    add_rules("qt.widgetapp")
    add_packages("qt5base", "opencv", "glog", "nlohmann_json")
    add_deps("MUI", "ImageStitchCore", "qtCommon", "common")
    add_files("src/*.hpp")
    add_files("src/*.cpp")
    -- add files with Q_OBJECT meta (only for qt.moc)
    add_files("src/mainWindow.h")