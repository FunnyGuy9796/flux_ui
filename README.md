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
gmake api (Optional build API)
gmake run
```

Note: `gmake run` must be run without any display server running or else they will compete for the framebuffer, possibly requiring the user to restart their machine. It is also important to note that this compositor must be run as root or else it will not be able to detect any input devices.