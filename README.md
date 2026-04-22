# wlrctlx

wlrctlx is a command line utility for miscellaneous wlroots Wayland extensions.

Wlrctlx supports the foreign-toplevel-mangement (window/toplevel command),
virtual-keyboard (keyboard command), ext-workspaces (workspace) and virtual-pointer (pointer command) protocols.

Requires wlroots 0.13+

## Installation

Build with meson/ninja e.g.

    $ meson setup --prefix=/usr/local build
	$ ninja -C build install

## Features and Examples

wlrctlx is still experimental, and has just a few basic features.
Check the man page wlrctl(1) for full details.

Some example uses are:

    $ wlrctlx keyboard type 'Hello, world!'

... to type some text using a virtual keyboard.

    $ wlrctlx pointer move 50 -70

... to move the cursor 50 pixels right and 70 pixels up.

    $ wlrctlx window focus firefox || swaymsg exec firefox

... to focus firefox if it is running, otherwise start firefox.

    $ wlrctlx toplevel waitfor mpv state:fullscreen && makoctl dismiss

... to show focused window.

    $ wlrctlx toplevel list state:active

... to dismiss desktop notifications when mpv becomes fullscreen

    $ wlrctlx workspace list

... List all available workspaces
