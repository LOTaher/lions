# LIONS

> Laith's Idea Of Networked Services

LIONS is my distributed system that automates my life, powers my development, and keeps me curious.

It is built on top of it's own custom protocol that I designed myself called [LIONS Middleware Protocol](https://github.com/LOTaher/lmp). This lets me have full control over my system at the packet level.

LIONS runs on all my hardware. My hardware shares a naming convention: Counter-Strike maps. You will see these hostnames referenced in my code:
- **mirage** is my personal laptop running MacOS Sequoia 15.6.1. It is an M1 13-inch Macbook Air. It is where I do all my development.
- **nuke** is my home server running Debian Stable. It has an AMD Ryzen 5 CPU, 1 TB SSD, 2 4 TB HDDS, and a NVIDIA GTX 1060.
- **inferno** is a Raspberry Pi 4 running a headless Debian Lite.

LIONS is currently composed of the following services:
- **admiral**: central message broker running 24/7 that allows different LIONS supported services to talk to each other using LMP.
- **archimedes**: automated ssh port forwarding tool to allow me to access my self-hosted services on any device I need.

This project is not currently structured or documented for public use. Though, it is published for transparency and source availability under the terms of the GPL.
