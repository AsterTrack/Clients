# Example Clients & Language Bindings
This folder contains example clients for several languages and supported I/O protocols.

## Protocol Dependencies
`dependencies` contains the buildsystem to fetch and build the libraries of the supported protocols and their language bindings. <br>
You may use VS Codes Tasks to build these dependencies, or run the scripts `dependencies/build.bat` or `dependencies/build.sh`. <br>
The headers and libraries for C/C++ will be put in `cpp/`, and the python libraries in `python/`. <br>
Supported protocols and language bindings: <br>
- VRPN (C/C++, python)

## Examples

#### Blender
The blender integration script uses VRPN and allows real-time mirroring of trackers onto Blender objects, and recording as an animation. <br>
Full addon support will follow, for now, follow these instructions on Linux (Windows may not work):

- Build the VRPN dependency for python (see above)
- Ensure the built VRPN library is located in `examples/python/lib/`
- Paste the [blender integration script](https://github.com/AsterTrack/Clients/blob/main/examples/python/blender_vrpn.py) into the Blender Text Editor
- Edit the `vrpnLibPath` to point to the absolute path of `examples/python/lib`
    - On Windows, I could not get this to work. Perhaps Blender-internal python version. Clues appreciated.
- Run the script, and a VRPN tab should appear in the 3D Viewports side panel
- Add Trackers via their label in AsterTrack
    - if your run them on different systems, you may append the local IP ("@192.168.XX.XX")
- Ensure AsterTrack server is streaming, and VRPN protocol is active
- Hit "Start Streaming" to enable the VRPN client thread and receive tracking data
- Use "Apply Pose Directly" to apply tracking data in realtime
- Do not use "Record Directly", it has a huge performance issue
- Prefer "Record Internally" instead and hit "Apply Recording to Keyframes" afterwards

#### VRPN in Python
A very simple [python script using VRPN](https://github.com/AsterTrack/Clients/blob/main/examples/python/vrpn_client.py) demonstrates basic usage, and requires the VRPN dependency to be built first.

#### VRPN in C++
See the [AsterTrack Viewer client](https://github.com/AsterTrack/Clients/tree/main/viewer) for a full C++ application using the full capabilities of VRPN - some of which, like probing which trackers are available or monitoring a trackers connection - require [a custom VRPN fork](https://github.com/Seneral/vrpn).

## Licenses

The build scripts and example clients are licensed under the MIT license. <br>
For licenses of the dependencies in `sources` or those fetched/built by the build scripts, see the `licenses` folder in the parent directory.