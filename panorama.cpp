/// @author Jonathan Kelaty
/// @date 2020-05-21
/// @file CS Vision - Final Project
/// @brief This program is a fully-featured implementation of panorama
/// image stitching using OpenCV. This program is intended to be used
/// in the command line, but has GUI features for ease of use, such as
/// for uploading images, previewing panorama, and other notifications
/// which gives the user relevant status updates during execution.

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

// Used to tracking arg parsing status
enum class Status {
    OK,
    EXIT,
    ERROR
};

// QOL
typedef std::string Filename;
typedef std::string Color;
typedef cv::Mat Image;

// Terminal text colors
const Color YELLOW = "\e[93m";
const Color GREEN  = "\e[32m";
const Color CYAN   = "\e[36m";
const Color RED    = "\e[31m";

// ASCII character codes
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

/**
 * Main entry for program. Expects command line arguments.
 * Use -h or --help to see available commands.
 * 
 * @return 0 Program terminated successfully, else error
 */
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

/**
 * Parses command line arguments. Calls respective functions to load image
 * vector before panorama stitching algorithm is applied.
 * 
 * @param argc main() CLI args
 * @param argv main() CLI args
 * @param images vector of images in which to read in images from desired source
 * 
 * @return Status code of argument parser, returns OK if arguments were accepted
 */
Status parseArgs(int argc, char* argv[], std::vector<Image>& images) {
    try {
        cxxopts::Options options(argv[0], "Panorama Stitcher");

        // Add argument options and descriptions
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

        // Parse args and check results
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

/**
 * Runs demo image datasets. Simply maps the subdirectory name to the
 * number of images in that dataset. Images follow a struct naming scheme,
 * so image filenames will be generated in code as needed.
 * 
 * @param images vector in which to read in images
 * @param demo enumeration of demo to read in [0,10]
 */
void runDemo(std::vector<Image>& images, std::size_t demo) {
    const std::vector<std::pair<std::string, std::size_t>> demos {
        {"carmel",    18}, {"diamondhead", 23}, {"example",   2},
        {"fishbowl",  13}, {"goldengate",   6}, {"halfdome", 14},
        {"hotel",      8}, {"office",       4}, {"rio",      56},
        {"shanghai",  30}, {"yard",         9}
    };

    // Assert enumeration is valid
    assert(0 <= demo && demo < demos.size());

    std::vector<Filename> image_files;

    // Set image filenames according to strict naming scheme. Looks messy,
    // but it gets the job done.
    for (std::size_t i = 0; i < demos[demo].second; ++i) {
        Filename file =
            "./demos/" + demos[demo].first + "/" + demos[demo].first
            + "-" + (i < 10 ? "0" : "") + std::to_string(i) + ".png";
        image_files.push_back(file);
    }

    // Upload images after we have generated filenames
    uploadImages(images, image_files);
}

/**
 * Captures frames from webcam. Will detect when user inputs either the RETURN
 * or ESCAPE keys. RETURN will capture the frame and put it into our images
 * vector. Escape will stop capturing images, and send them to the stitcher. A
 * preview window will pop up so the user can see what the frame will be before
 * it's captured.
 * 
 * @param images vector in which to read in images
 */
void cameraCapture(std::vector<Image>& images) {
    cv::VideoCapture feed;
    bool exit = false;

    feed.open( 0 );

    for (;;) {
        Image frame;
        feed >> frame;

        if ( frame.data ) {
            Image display_frame( frame.clone() );

            // We created a copy of the frame above and insert text giving instructions
            // to the user. Copy is made so final panorama will not have ugly warped text.
            // We call putText() twice to give the text a black outline for visibilty.
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

            cv::imshow("Camera feed", display_frame); // Preview frame
        }
        else {
            break;
        }

        switch( cv::waitKey(1) ) {
            case RETURN: // Capture current frame
                std::cout << YELLOW;
                std::cout << "Adding frame..." << std::endl;
                images.push_back(frame);
                break;
            case ESCAPE: // Stop capturing frames
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

/**
 * Brings up the GUI for selecting images. Essentially servers exact same purpose
 * as uploadImages(), but a bit nicer and easier to use.
 * 
 * @param images vector in which to read in images
 */
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

/**
 * Reads in images from a vector of images filenames. This is the vector which is
 * passed to the panorama stitcher.
 * 
 * @param images vector in which to read in images
 * @param files images filenames used to read in images
 */
void uploadImages(std::vector<Image>& images, const std::vector<Filename>& files) {
    for (const Filename & file : files) {
        Image image = cv::imread( file );
        images.push_back(image);
    }
}

/**
 * Can be passed a video filename which is parsed for frames to stitch together.
 * They key to this function is the frequency variable, which determines how
 * frequently frames will be taken from the video. The basic formula is fairly
 * obvious, as it's just:
 *      TOTAL_FRAMES * frequency
 * We then skip this number of frames on each iteration and capture whatever the
 * next frame is and store it into our image vector. Note that the frequency parameter
 * MUST be between 0 and 1, otherwise it just doesn't work. Default parameter is set
 * to 0.1, so 10 frames will be captured from the video. This worked fine for the short
 * test video I used, but longer videos might not have enough correspondence between
 * frames at this rate, so adjust as necessary.
 * 
 * @param images vector in which to read in images
 * @param video filename for video from which to capture frames
 * @param frequency frequency to capture frames. 1/frequency = number of frames to be captured
 */
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
            // Capture frame and increase frame position in video capture feed.
            // This is more efficient since we don't have to read in all the frames
            // that we're ignoring.
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

/**
 * This is the function which actually creates the panorama image. Accepts the vector
 * of images as a parameter and passes into the stitcher class. Note that reaching this
 * block of code doesn't guarantee that a panorama can be created from the images, despite
 * all the condition checking in the previous functions. If the set of images does not
 * have enough matching features, a panorama will not be generated. If the panorama is
 * successfully created, then the result is displayed visually to the user. On keypress,
 * the window will be begin to close, before which a dialog asking if they wish to save
 * the image. Once the user makes a decision, the window closes and the program terminates.
 * 
 * @param images vector of images which store the images to create a panorama from
 */
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

/**
 * Prompts the user if they wish to save the panorama. Dialog appears only
 * after the preview is marked to be closed. If they user chooses to save
 * the image, they can browse to the subdirectory using the GUI and name
 * the file whatever they want.
 * 
 * @param image Panorama image to be saved
 */
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

/**
 * Shows notification to the user. Message is output to std::cout and
 * a GUI notification. Used to give the user an accurate status on
 * the program's state. i.e., if the stitcher succeeded in creating
 * a panorama.
 * 
 * @param message Notification message
 */
void showNotification(const std::string& message) {
    std::cout << GREEN;
    std::cout << message << std::endl;

    pfd::notify(
        "Panorama Stitcher",
        message,
        pfd::icon::info);
}

/**
 * Shows error message. Will ouptut message to std::cout and give
 * a GUI notification to the user. Used in the case that the
 * panorama cannot be created.
 * 
 * @param message Error message
 */
void showError(const std::string& message) {
    std::cout << RED;
    std::cout << message << std::endl;

    pfd::notify(
        "Panorama Stitcher",
        message,
        pfd::icon::error);
}