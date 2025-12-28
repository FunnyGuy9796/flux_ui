# Flux UI

An open-source compositor for the FreeBSD operating system that utilizes KMS/DRM, GBM, and OpenGL to render custom applications. 

## Technologies

- DRM/KMS
- GBM
- OpenGL

## Features

- Window rendering
- Widget creation
  - Text
  - Images
  - Transparency
  - Rounded corners

## Development Roadmap

- Implement user-input
  - Add hotplug detection
- Implement audio streaming
- Proper window management

## Build process

```
gmake

# Optional: build API
gmake api

gmake run

# Build a test program to draw a red square using the API
cc test.c api/flux_api.o -o test
```

Note: `gmake run` must be run without any display server running or else they will compete for the framebuffer, possibly requiring the user to restart their machine. It is also important to note that this compositor must be run as root or else it will not be able to detect any input devices.