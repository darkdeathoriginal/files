#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

// rlottie for rendering Lottie animations
#include <rlottie.h>

// libwebp for encoding
#include <webp/encode.h>
#include <webp/mux.h> // <<< FIX #1: ADD THIS LINE for animated WebP functions

void printUsage()
{
    std::cerr << "Usage: ./lottie2webp <input.json> <output.webp> <width> <height>" << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printUsage();
        return 1;
    }

    // 1. Parse Command Line Arguments
    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    int width = std::stoi(argv[3]);
    int height = std::stoi(argv[4]);

    if (width <= 0 || height <= 0)
    {
        std::cerr << "Error: Width and height must be positive integers." << std::endl;
        return 1;
    }

    // 2. Load the Lottie Animation
    std::unique_ptr<rlottie::Animation> animation =
        rlottie::Animation::loadFromFile(inputPath);

    if (!animation)
    {
        std::cerr << "Error: Could not load Lottie file from " << inputPath << std::endl;
        return 1;
    }

    // 3. Get Animation Properties
    size_t totalFrames = animation->totalFrame();
    double frameRate = animation->frameRate();
    if (totalFrames == 0)
    {
        std::cerr << "Error: The animation has 0 frames." << std::endl;
        return 1;
    }
    std::cout << "Animation Info:" << std::endl;
    std::cout << " - Frames: " << totalFrames << std::endl;
    std::cout << " - Framerate: " << frameRate << " fps" << std::endl;
    std::cout << " - Output Size: " << width << "x" << height << std::endl;

    // 4. Initialize WebP Animated Encoder
    WebPAnimEncoderOptions enc_options;
    WebPAnimEncoderOptionsInit(&enc_options);
    enc_options.anim_params.loop_count = 0; // 0 for infinite loop

    WebPAnimEncoder *enc = WebPAnimEncoderNew(width, height, &enc_options);
    if (!enc)
    {
        std::cerr << "Error: Could not create WebP animated encoder." << std::endl;
        return 1;
    }

    // Configure encoding parameters (e.g., quality)
    WebPConfig config;
    WebPConfigInit(&config);
    config.quality = 80; // Quality factor (0=bad, 100=good)
    config.method = 4;   // Quality/speed trade-off (0=fast, 6=slowest)
    if (!WebPValidateConfig(&config))
    {
        std::cerr << "Error: Invalid WebP config." << std::endl;
        WebPAnimEncoderDelete(enc);
        return 1;
    }

    // 5. Render Each Frame and Add to WebP Encoder
    int timestamp_ms = 0;
    int frame_duration_ms = 1000 / frameRate;
    std::vector<uint32_t> buffer(width * height);

    std::cout << "Encoding frames..." << std::endl;

    for (size_t frame_num = 0; frame_num < totalFrames; ++frame_num)
    {

        // <<< FIX #2: Use the new rlottie::Surface API
        // Create a Surface object that wraps our pixel buffer
        rlottie::Surface surface(buffer.data(), width, height, width * sizeof(uint32_t));

        // Render the frame into the Surface
        animation->renderSync(frame_num, surface);

        // Create a WebPPicture for the current frame
        WebPPicture pic;
        WebPPictureInit(&pic);
        pic.width = width;
        pic.height = height;
        pic.use_argb = 1;

        WebPPictureImportBGRA(&pic, (const uint8_t *)buffer.data(), width * sizeof(uint32_t));

        if (!WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config))
        {
            std::cerr << "Error adding frame " << frame_num << " to WebP encoder." << std::endl;
            WebPPictureFree(&pic);
            WebPAnimEncoderDelete(enc);
            return 1;
        }

        timestamp_ms += frame_duration_ms;
        WebPPictureFree(&pic);
        std::cout << "\rProgress: " << frame_num + 1 << "/" << totalFrames << std::flush;
    }
    std::cout << std::endl;

    // 6. Finalize the WebP Animation
    WebPAnimEncoderAdd(enc, NULL, timestamp_ms, NULL);

    WebPData webp_data;
    WebPDataInit(&webp_data);
    if (!WebPAnimEncoderAssemble(enc, &webp_data))
    {
        std::cerr << "Error assembling WebP animation." << std::endl;
        WebPAnimEncoderDelete(enc);
        return 1;
    }

    // 7. Save the WebP Data to a File
    std::ofstream outfile(outputPath, std::ios::binary);
    if (!outfile)
    {
        std::cerr << "Error: Cannot open output file " << outputPath << " for writing." << std::endl;
        WebPDataClear(&webp_data);
        WebPAnimEncoderDelete(enc);
        return 1;
    }
    outfile.write(reinterpret_cast<const char *>(webp_data.bytes), webp_data.size);
    outfile.close();

    // 8. Clean Up Resources
    WebPDataClear(&webp_data);
    WebPAnimEncoderDelete(enc);

    std::cout << "Successfully converted " << inputPath << " to " << outputPath << std::endl;

    return 0;
}