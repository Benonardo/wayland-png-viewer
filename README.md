# A pixel-perfect PNG-viewer for Wayland

## Building

Requires make, libpng, libwayland-client and a modern C compiler to be installed.

## Usage

Requires libpng and Wayland to be installed.

The filepath is the only command line argument.

It has inbuilt pixel-perfect scaling, so there might be a lot of padding with excentric aspect ratios. For an experimental solution using the Wayland viewporter, see the `viewporter` branch.
