/// @author Jonathan Kelaty
/// @date 2020-05-21
/// @file CS Vision - Final Project
/// @brief Panorama Stitching

// STL includes
#include <iostream>
#include <vector>
#include <string>
#include <utility>

// OpenCV
#include "opencv2/stitching.hpp"
#include "opencv2/highgui.hpp"

// CLI Args and GUI
#include "includes/cxxopts.hpp"
#include "includes/portable-file-dialogs.h"

enum class Status {
    OK,
    EXIT,
    ERROR
};

typedef std::string Filename;
typedef std::string Color;
typedef cv::Mat Image;

const Color YELLOW = "\e[93m";
const Color GREEN  = "\e[32m";
const Color CYAN   = "\e[36m";
const Color RED    = "\e[31m";

const int RETURN = 13;
const int ESCAPE = 27;

Status parseArgs(int argc, char* argv[], std::vector<Image>& images);
void runDemo(std::vector<Image>& images, std::size_t demo);
void cameraCapture(std::vector<Image>& images);
void fileSelectGUI(std::vector<Image>& images);
void uploadImages(std::vector<Image>& images, const std::vector<Filename>& files);
void videoCapture(std::vector<Image>& images, const Filename& video, double frequency = 0.1);
void createPanorama(const std::vector<Image>& images);
void promptSaveImage(const Image& image);
void showNotification(const std::string& message);
void showError(const std::string& message);

int main(int argc, char* argv[]) {
    std::vector<Image> images;

    Status status = parseArgs(argc, argv, images);

    if ( status == Status::OK ) {
        if ( images.size() > 1 ) {
            createPanorama(images);
        }
        else {
            showError("Not enough images provided");
        }
    }

    return 0;
}

Status parseArgs(int argc, char* argv[], std::vector<Image>& images) {
    try {
        cxxopts::Options options(argv[0], "Panorama Stitcher");

        options
            .add_options()
            ("c,camera", "Enable camera input")
            ("s,select", "Use file select GUI")
            ("i,images", "Input image files",
                cxxopts::value<std::vector<Filename>>())
            ("v,video", "Input video file",
                cxxopts::value<Filename>())
            ("d,demo",  "Try demo image sets [0..10]",
                cxxopts::value<std::size_t>())
            ("h,help", "Print help");

        auto result = options.parse(argc, argv);

        if ( result.count("help") ) {
            std::cout << CYAN;
            std::cout << options.help() << std::endl;
            return Status::EXIT;
        }
        else if ( result.count("demo")   ) {
            runDemo(images, result["demo"].as<std::size_t>());
        }
        else if ( result.count("camera") ) {
            cameraCapture(images);
        }
        else if ( result.count("select") ) {
            fileSelectGUI(images);
        }
        else if ( result.count("images") ) {
            uploadImages(images, result["images"].as<std::vector<Filename>>());
        }
        else if ( result.count("video")  ) {
            videoCapture(images, result["video"].as<Filename>());
        }
        else {
            std::cout << YELLOW;
            std::cout << "Use -h or --help for more information" << std::endl;
            return Status::EXIT;
        }

        return Status::OK;
    }
    catch (const cxxopts::OptionException& e) {
        std::cout << RED;
        std::cout << "Error parsing args: " << e.what() << std::endl;
        return Status::ERROR;
    }

    return Status::ERROR;
}

void runDemo(std::vector<Image>& images, std::size_t demo) {
    const std::vector<std::pair<std::string, std::size_t>> demos {
        {"carmel",    18}, {"diamondhead", 23}, {"example",   2},
        {"fishbowl",  13}, {"goldengate",   6}, {"halfdome", 14},
        {"hotel",      8}, {"office",       4}, {"rio",      56},
        {"shanghai",  30}, {"yard",         9}
    };

    assert(0 <= demo && demo < demos.size());

    std::vector<Filename> image_files;

    for (std::size_t i = 0; i < demos[demo].second; ++i) {
        Filename file =
            "./demos/" + demos[demo].first + "/" + demos[demo].first
            + "-" + (i < 10 ? "0" : "") + std::to_string(i) + ".png";
        image_files.push_back(file);
    }

    uploadImages(images, image_files);
}

void cameraCapture(std::vector<Image>& images) {
    cv::VideoCapture feed;
    bool exit = false;

    feed.open( 0 );

    for (;;) {
        Image frame;
        feed >> frame;

        if ( frame.data ) {
            Image display_frame( frame );

            cv::putText(
                display_frame,
                "Press RETURN to capture frame or ESC to exit",
                cv::Point(20, display_frame.rows - 30),
                cv::FONT_HERSHEY_COMPLEX_SMALL,
                1.0,
                cv::Scalar(0,0,0),
                3
            );
            cv::putText(
                display_frame,
                "Press RETURN to capture frame or ESC to exit",
                cv::Point(20, display_frame.rows - 30),
                cv::FONT_HERSHEY_COMPLEX_SMALL,
                1.0,
                cv::Scalar(255,255,255),
                1
            );

            cv::imshow("Camera feed", display_frame);
        }
        else {
            break;
        }

        switch( cv::waitKey(1) ) {
            case RETURN:
                std::cout << YELLOW;
                std::cout << "Adding frame..." << std::endl;
                images.push_back(frame);
                break;
            case ESCAPE:
                std::cout << CYAN;
                std::cout << "Finished taking images..." << std::endl;
                exit = true;
                break;
        }

        if ( exit ) {
            break;
        }
    }

    feed.release();
    cv::destroyAllWindows();
}

void fileSelectGUI(std::vector<Image>& images) {
    auto files =
        pfd::open_file(
            "Select images to create panorama of",
            "./",
            { "All Files", "*" },
            pfd::opt::multiselect
        );

    uploadImages(images, files.result());
}

void uploadImages(std::vector<Image>& images, const std::vector<Filename>& files) {
    for (const Filename & file : files) {
        Image image = cv::imread( file );
        images.push_back(image);
    }
}

void videoCapture(std::vector<Image>& images, const Filename& video, double frequency) {
    assert(0 < frequency && frequency < 1);

    cv::VideoCapture feed;
    
    feed.open( video );

    std::size_t frame_position  = feed.get(cv::CAP_PROP_POS_FRAMES);
    std::size_t frame_frequency = feed.get(cv::CAP_PROP_FRAME_COUNT) * frequency;
    
    for (;;) {
        Image frame;
        feed >> frame;

        if ( frame.data ) {
            images.push_back(frame);
            feed.set(cv::CAP_PROP_POS_FRAMES, frame_position + frame_frequency);
            frame_position = feed.get(cv::CAP_PROP_POS_FRAMES);
        }
        else {
            break;
        }
    }

    feed.release();
}

void createPanorama(const std::vector<Image>& images) {
    std::cout << GREEN;
    std::cout << "Creating panorama..." << std::endl;
    
    Image panorama;

    cv::Ptr<cv::Stitcher> stitcher = cv::Stitcher::create( cv::Stitcher::PANORAMA );
    cv::Stitcher::Status status    = stitcher->stitch( images, panorama );
    
    if ( status == cv::Stitcher::OK ) {
        showNotification("Panorama successfully created!");
        
        cv::imshow( "Panorama", panorama );
        cv::waitKey(0);

        promptSaveImage(panorama);

        cv::destroyAllWindows();
    }
    else {
        showError("Panorama could not be created.");
    }
}

void promptSaveImage(const Image& image) {
    auto save =
        pfd::message(
            "Save image?",
            "Would you like to save the panorama image?",
            pfd::choice::yes_no,
            pfd::icon::question
        );

    while ( ! save.ready(1000) );

    if ( save.result() == pfd::button::yes ) {
        auto dir = pfd::save_file("Choose save location", "./").result();

        if ( dir != "" ) {
            cv::imwrite(dir, image);
            showNotification("Panorama saved at: " + dir);
        }
    }
}

void showNotification(const std::string& message) {
    std::cout << GREEN;
    std::cout << message << std::endl;

    pfd::notify(
        "Panorama Stitcher",
        message,
        pfd::icon::info);
}

void showError(const std::string& message) {
    std::cout << RED;
    std::cout << message << std::endl;

    pfd::notify(
        "Panorama Stitcher",
        message,
        pfd::icon::error);
}
