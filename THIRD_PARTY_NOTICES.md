# Third-party notices

## nlohmann/json 3.11.2

`third_party/json.hpp` is JSON for Modern C++ by Niels Lohmann and contributors.
The original copyright and SPDX notices remain in the header.

- Source: https://github.com/nlohmann/json/tree/v3.11.2
- License: MIT; full text in [third_party/LICENSE.nlohmann-json](third_party/LICENSE.nlohmann-json).

## Code::Blocks SDK

The plugin is built against Code::Blocks 25.03 SDK 2.25.0 and dynamically links
to the user's installed `codeblocks.dll`. The SDK headers used by this project
identify their license as GNU LGPL v3. The IDE and complete SDK are not bundled
in this repository or in the `.cbplugin` installer.

- Project: https://www.codeblocks.org/
- LGPL v3 text: https://www.gnu.org/licenses/lgpl-3.0.html

## wxWidgets

The tested build uses wxWidgets 3.2.7 headers and dynamically links to the
`wxmsw32u_gcc_custom.dll` supplied by the user's Code::Blocks installation.
wxWidgets is distributed under the wxWindows Library Licence. Its license text
is preserved in [licenses/wxWidgets.txt](licenses/wxWidgets.txt). The complete
development package and runtime DLL are not bundled here.

- Project: https://www.wxwidgets.org/

## MinGW / GCC

The tested compiler is the MinGW GCC 14.2 toolchain supplied with Code::Blocks.
The compiler is an external requirement and is not bundled. GCC runtime
components are subject to their own licenses and the GCC Runtime Library
Exception, where applicable:
https://www.gnu.org/licenses/gcc-exception-3.1.html
