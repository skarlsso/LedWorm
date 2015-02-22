# Use ino (http://inotool.org) to build.
#
# Needs FastLED linked into the lib/ directory.
#
# The --alibs="false" flag turns off the searching of libraries in the arduino lib directory, which is causing problems for me. This flag is not in the upstream version of ino.

ino build --alibs="false" -m leonardo --cxxflags="-ffunction-sections -fdata-sections -fno-exceptions -fno-threadsafe-statics" --cflags="-ffunction-sections -fdata-sections"
