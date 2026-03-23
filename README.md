---
title: JavaCube Minecraft Seed Finder
emoji: 🧊
colorFrom: blue
colorTo: green
sdk: docker
pinned: false
---

# JavaCube Minecraft Seed Finder

A powerful web-based tool for finding high-quality Minecraft Java edition seeds using the `cubiomes` library.

## Hosting on Hugging Face Spaces

To host this project on Hugging Face:
1. Create a new [Space](https://huggingface.co/spaces) on Hugging Face.
2. Select **Docker** as the SDK.
3. Choose the **Blank** template or sync directly from your GitHub repository.
4. Set the `PORT` secret to `7860` (default) if needed, though our server defaults to this.
5. Hugging Face will automatically detect the `Dockerfile` and build the application.

## Local Setup

1. Run `bash build.sh` to compile the `cubiomes` library and the `verify_build` executable.
2. Install requirements: `pip install -r requirements.txt`.
3. Start the server: `python server.py`.
4. Open `http://localhost:7860` in your browser.
