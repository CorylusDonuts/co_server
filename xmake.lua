add_rules("mode.release", "mode.debug")
-- set_languages("c++latest")
set_languages("c++23")

-- add_rules("plugin.compile_commands.autoupdate")

includes("requires.lua")

target("picohttpparser")
    set_kind("static")
    add_files("lib/picohttpparser/picohttpparser.c")
    add_includedirs("lib/picohttpparser")

target("ASIOServer")
    set_kind("binary")
    add_files("src/**.cpp")
    add_packages("asio", "glaze", "gdal")
    add_includedirs("lib")
    add_deps("picohttpparser")
    set_policy("build.c++.modules", true)
