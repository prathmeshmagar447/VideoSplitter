#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <algorithm>
#include <ctime>
#include <utility>
#include <atomic>
#include <iomanip>
#include <chrono>

// Helper: Run FFmpeg command safely
bool run_ffmpeg(const std::string& command) {
    int ret = std::system(command.c_str());
    return (ret == 0);
}

struct Config {
    // Video processing settings
    double min_clip_duration = 3.0;
    double max_clip_duration = 7.0;
    double min_gap = 5.0;
    double max_gap = 10.0;

    // Audio settings
    bool mute_audio = true;

    // Video processing settings
    bool use_stream_copy = true; // Much faster than re-encoding
    std::string video_codec = "copy"; // or "libx264"

    // Performance settings
    int max_threads = 0; // 0 = auto-detect hardware concurrency
};

Config load_config() {
    Config cfg;
    cfg.max_threads = std::thread::hardware_concurrency();
    return cfg;
}

int main(int argc, char* argv[]) {
    std::srand(std::time(nullptr));  // Seed random number generator

    // Configuration constants
    const double MIN_CLIP_DURATION = 3.0;
    const double MAX_CLIP_DURATION = 7.0;
    const double MIN_GAP = 5.0;
    const double MAX_GAP = 10.0;

    std::string input_folder = "./input";
    std::string output_folder = "./output_clips";

    // Create input folder if it doesn't exist
    std::string mkdir_input = "mkdir -p \"" + input_folder + "\"";
    if (std::system(mkdir_input.c_str()) != 0) {
        std::cerr << "Failed to create input folder.\n";
        return 1;
    }

    std::cout << "🎬 Video Splitter\n";
    std::cout << "================\n\n";
    std::cout << "⚙️  Processing Settings:\n";
    std::cout << "   Clip duration: " << MIN_CLIP_DURATION << "-" << MAX_CLIP_DURATION << "s (random)\n";
    std::cout << "   Gap duration: " << MIN_GAP << "-" << MAX_GAP << "s (random)\n";
    std::cout << "   Audio: Muted\n";
    std::cout << "   Video codec: Stream copy (fastest)\n\n";
    std::cout << "📂 Input folder: " << input_folder << "\n";
    std::cout << "📂 Output folder: " << output_folder << "\n\n";

    // Find video files in input folder
    std::vector<std::string> video_files;
    DIR* dir = opendir(input_folder.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename != "." && filename != "..") {
                std::string ext = filename.substr(filename.find_last_of("."));
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mp4" || ext == ".avi" || ext == ".mov" || ext == ".mkv" || ext == ".webm") {
                    video_files.push_back(input_folder + "/" + filename);
                }
            }
        }
        closedir(dir);
    }

    if (video_files.empty()) {
        std::cout << "No video files found in " << input_folder << "\n";
        std::cout << "Please place video files (.mp4, .avi, .mov, .mkv, .webm) in the input folder.\n";
        return 1;
    }

    std::cout << "Found " << video_files.size() << " video file(s):\n";
    for (size_t i = 0; i < video_files.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << video_files[i] << "\n";
    }
    std::cout << "\n";

    // Process all video files
    for (const auto& input_path : video_files) {
        std::cout << "Processing: " << input_path << "\n";

        // Skip to next iteration if file doesn't exist
        if (access(input_path.c_str(), F_OK) != 0) {
            std::cout << "File not found, skipping...\n\n";
            continue;
        }

    if (access(input_path.c_str(), F_OK) != 0) {
        std::cerr << "Error: Input video does not exist.\n";
        return 1;
    }

    // Create output directory
    std::string mkdir_cmd = "mkdir -p \"" + output_folder + "\"";
    if (std::system(mkdir_cmd.c_str()) != 0) {
        std::cerr << "Failed to create output directory.\n";
        return 1;
    }

    // Get video duration using ffprobe
    std::string probe_cmd = "ffprobe -v error -show_entries format=duration "
                            "-of default=noprint_wrappers=1:nokey=1 \"" + input_path + "\" > duration.txt";
    if (std::system(probe_cmd.c_str()) != 0) {
        std::cerr << "Failed to probe video duration.\n";
        return 1;
    }

    std::ifstream dur_file("duration.txt");
    double duration = 0.0;
    dur_file >> duration;
    dur_file.close();
    std::remove("duration.txt");

    if (duration <= 0.0) {
        std::cerr << "Invalid video duration.\n";
        return 1;
    }

    std::vector<std::pair<double, double>> clips;

    // Generate clips with configurable gaps
    double current_time = 0.0;
    int clip_index = 0;
    while (current_time < duration) {
        // Random clip duration between 3-7 seconds
        double clip_duration = MIN_CLIP_DURATION + static_cast<double>(rand()) / RAND_MAX * (MAX_CLIP_DURATION - MIN_CLIP_DURATION);
        double clip_end = std::min(current_time + clip_duration, duration);
        clips.emplace_back(current_time, clip_end);
        clip_index++;

        // Add random gap within configured range
        double gap_duration = MIN_GAP + static_cast<double>(rand()) / RAND_MAX * (MAX_GAP - MIN_GAP);
        current_time = clip_end + gap_duration;
    }

    int num_clips = clips.size();

    std::cout << "Video duration: " << duration << "s\n";
    std::cout << "Generating " << num_clips << " clips with 5-10s gaps...\n";

    // Processing tracking
    std::atomic<int> processed_clips{0};
    auto start_time = std::chrono::high_resolution_clock::now();


    std::mutex io_mutex;
    auto process_clip = [&](int idx) {
        auto [start, end] = clips[idx];
        double clip_duration = end - start;

        std::ostringstream filename;
        filename << output_folder << "/clip_" << std::setw(3) << std::setfill('0') << (idx + 1) << "_"
                 << std::fixed << std::setprecision(1) << start << "s-" << end << "s.mp4";

        std::ostringstream cmd;
        cmd << "ffmpeg -y -ss " << start << " -i \"" << input_path << "\" "
            << "-t " << clip_duration << " -an -c:v copy -avoid_negative_ts make_zero -fflags +discardcorrupt \"" << filename.str() << "\" 2>/dev/null";

        bool ok = run_ffmpeg(cmd.str());

        // Update progress
        int completed = ++processed_clips;
        double progress = static_cast<double>(completed) / num_clips * 100.0;

        std::lock_guard<std::mutex> lock(io_mutex);
        if (ok) {
            std::cout << "✅ [" << std::fixed << std::setprecision(1) << progress << "%] "
                      << "Saved: " << filename.str() << "\n";
        } else {
            std::cerr << "❌ [" << std::fixed << std::setprecision(1) << progress << "%] "
                      << "Failed: " << filename.str() << "\n";
        }
    };

    // Parallelize with threads (use hardware concurrency)
    auto processing_start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    int max_threads = std::thread::hardware_concurrency();
    for (int i = 0; i < num_clips; ++i) {
        threads.emplace_back(process_clip, i);
        if (threads.size() >= max_threads || i == num_clips - 1) {
            for (auto& t : threads) t.join();
            threads.clear();
        }
    }
    auto processing_end = std::chrono::high_resolution_clock::now();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double duration_sec = duration_ms / 1000.0;

    // Performance breakdown
    std::cout << "\n🎉 Processing complete!\n";
    std::cout << "📁 Output folder: " << output_folder << "\n";
    std::cout << "⏱️  Total time: " << std::fixed << std::setprecision(2) << duration_sec << " seconds\n";
    std::cout << "🎬 Clips created: " << processed_clips << "/" << num_clips << "\n";

    if (num_clips > 0) {
        double processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(processing_end - processing_start).count() / 1000.0;
        double avg_time_per_clip = processing_time / num_clips;
        std::cout << "⚡ Average time per clip: " << std::fixed << std::setprecision(2) << avg_time_per_clip << "s\n";
        std::cout << "🎯 Processing: Muted 5-second clips with 5-10s gaps\n";
    }
    std::cout << "\n💡 Tip: Adjust settings by modifying the Config struct in the source code.\n\n";

        std::cout << "Finished processing " << input_path << "\n\n";
    }

    std::cout << "All video files processed!\n";
    return 0;
}