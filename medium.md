# Getting My Old Family Photos Out of .MIX Files

![MIX Photo Export turns legacy MIX files into JPEG photos.](medium-banner.png)

I had old family photos saved in Microsoft Picture It!'s `.mix` format.
At the time, I thought it was a format that would last. I certainly wasn't
thinking about how I'd open them years later.

Well, years later arrived. I wanted to convert those photos to JPEG, and
couldn't find an easy way to do it. The files were there, but that wasn't
much comfort when I couldn't get at the pictures.

So I put GitHub Copilot and Astra to work on it. With their help, I built
and tested a converter and got some of those lost memories back. Seeing
the family photos again made this a particularly satisfying little project.

I've called it **MIX Photo Export** and put it on GitHub. You might have
a folder of these files tucked away too.

There was a bit more to it than changing a file extension. A MIX document
can hold several embedded images. The library that makes this possible is
[libfpx, the FlashPix toolkit maintained by ImageMagick](https://github.com/ImageMagick/libfpx).
Originally developed by Digital Imaging Group and Eastman Kodak, it does
the hard work of decoding those old images. Copilot and Astra helped me
build the converter around that existing work.

The tool recovers each recognized image at its highest stored resolution
and uses [libjpeg-turbo](https://libjpeg-turbo.org/) to write the JPEGs.
Quality defaults to 100, though JPEG is still lossy at that setting.
There's no AI-generated detail in the photos.

I also wanted to avoid depending on another old download staying available.
The project includes a pinned copy of libfpx's source with its licence notices.
It won't guarantee the tool works forever, but at least the decoder source
comes with it. I'm grateful that this older work is still available and
maintained; without it, recovering these photos would have been a much
bigger project.

## Try It on a Folder

With Docker installed and running, clone the project and build the image:

```sh
git clone https://github.com/gloveboxes/mix-photo-export.git
cd mix-photo-export
docker build -t mix-photo-export:local .
```

On macOS or Linux, convert a folder with:

```sh
bash scripts/mix-photo-export --runtime docker convert \
  "/path/to/mix photos" "/path/to/recovered photos"
```

The README covers Windows PowerShell and Apple containers as well. The
first build needs internet access for the container base and packages;
photo conversion runs locally with networking disabled.

The launcher mounts your originals read-only and writes to a separate output
folder. Existing JPEGs are skipped. Keep a backup anyway, and try a few files
before running through the whole archive.

One limitation worth knowing: you get the embedded photos, not the complete
Picture It! layout. Text, layer positioning, viewing adjustments, metadata,
and ICC profiles aren't carried across.

For my family photos, getting the pictures back was enough. I'm glad I kept
those old files, even when I couldn't do much with them.

The [code and instructions are on GitHub](https://github.com/gloveboxes/mix-photo-export).
Let me know how you get on with your own archive.

Cheers, Dave