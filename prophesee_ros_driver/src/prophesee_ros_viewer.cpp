/*******************************************************************
 * File : prophesee_ros_viewer.cpp                                 *
 *                                                                 *
 * Copyright: (c) 2015-2019 Prophesee                              *
 *******************************************************************/

#include "prophesee_ros_viewer.h"

using namespace cv;

typedef const boost::function<void(const prophesee_event_msgs::EventArray::ConstPtr &msgs)> callback;

PropheseeWrapperViewer::PropheseeWrapperViewer() :
    nh_("~"),
    it_(nh_),
    cd_window_name_("CD Events"),
    gl_window_name_("GrayLevel Data"),
    display_acc_time_(5000),
    initialized_(false) {
    std::string camera_name("");

    // Load Parameters
    nh_.getParam("camera_name", camera_name);
    nh_.getParam("show_cd", show_cd_);
    nh_.getParam("show_graylevels", show_graylevels_);
    nh_.getParam("display_accumulation_time", display_acc_time_);

    const std::string topic_cam_info         = "/prophesee/" + camera_name + "/camera_info";
    const std::string topic_cd_event_buffer  = "/prophesee/" + camera_name + "/cd_events_buffer";
    const std::string topic_graylevel_buffer = "/prophesee/" + camera_name + "/graylevel_image";

    // Subscribe to camera info topic
    sub_cam_info_ = nh_.subscribe(topic_cam_info, 1, &PropheseeWrapperViewer::cameraInfoCallback, this);

    // Subscribe to CD buffer topic
    if (show_cd_) {
        callback displayerCDCallback = boost::bind(&CDFrameGenerator::add_events, &cd_frame_generator_, _1);
        sub_cd_events_               = nh_.subscribe(topic_cd_event_buffer, 500, displayerCDCallback);
    }

    // Subscribe to gray-level event buffer topic
    if (show_graylevels_)
        sub_gl_frame_ = it_.subscribe(topic_graylevel_buffer, 1, &PropheseeWrapperViewer::glFrameCallback, this);
    
    // publish to CD frame image topit
    pub_cd_frame_ = it_.advertise("/CD_frame", 100);
}

PropheseeWrapperViewer::~PropheseeWrapperViewer() {
    if (!initialized_)
        return;

    // Stop the CD frame generator thread
    if (show_cd_)
        cd_frame_generator_.stop();

    // Destroy the windows
    cv::destroyAllWindows();

    nh_.shutdown();
}

bool PropheseeWrapperViewer::isInitialized() {
    return initialized_;
}

void PropheseeWrapperViewer::cameraInfoCallback(const sensor_msgs::CameraInfo::ConstPtr &msg) {
    if (initialized_)
        return;

    if ((msg->width != 0) && (msg->height != 0))
        init(msg->width, msg->height);
}

void PropheseeWrapperViewer::glFrameCallback(const sensor_msgs::ImageConstPtr &msg) {
    if (!initialized_)
        return;

    try {
        cv::imshow(gl_window_name_, cv_bridge::toCvShare(msg, sensor_msgs::image_encodings::TYPE_8UC1)->image);
    } catch (cv_bridge::Exception &e) { ROS_ERROR("cv_bridge exception: %s", e.what()); }
}

bool PropheseeWrapperViewer::init(const unsigned int &sensor_width, const unsigned int &sensor_height) {
    if (show_cd_) {
        // Define the display window for CD events
        create_window(cd_window_name_, sensor_width, sensor_height, 0, 0);
        // Initialize CD frame generator
        cd_frame_generator_.init(sensor_width, sensor_height);
        cd_frame_generator_.set_display_accumulation_time_us(display_acc_time_);
        // Start CD frame generator thread
        cd_frame_generator_.start();
    }

    if (show_graylevels_) {
        // Define the display window for gray-level frame
        create_window(gl_window_name_, sensor_width, sensor_height, 0, sensor_height + 50);
    }

    initialized_ = true;

    return true;
}

void PropheseeWrapperViewer::create_window(const std::string &window_name, const unsigned int &sensor_width,
                                           const unsigned int &sensor_height, const int &shift_x, const int &shift_y) {
    cv::namedWindow(window_name, CV_GUI_EXPANDED);
    cv::resizeWindow(window_name, sensor_width, sensor_height);
    // move needs to be after resize on apple, otherwise the window stacks
    cv::moveWindow(window_name, shift_x, shift_y);
}

void PropheseeWrapperViewer::showData() {
    if (!show_cd_)
        return;

    //if (cd_frame_generator_.get_last_ros_timestamp() < ros::Time::now() - ros::Duration(0.5)) {
    //    cd_frame_generator_.reset();
    //    initialized_ = false;
    //}
    const auto &cd_frame = cd_frame_generator_.get_current_frame();
    if (!cd_frame.empty()) {
        Mat frame_cp;
        cd_frame.copyTo(frame_cp);
        frame_id ++;
        auto time_now = ros::Time::now();
        if (print_timestamp){
            std::ostringstream text;
            text << "seq:" << frame_id << " time_stamp:" << time_now;
            putText(frame_cp, text.str(), Point(20,20), FONT_HERSHEY_DUPLEX, 0.7, Scalar(0,0,255), 1);
        }
        sensor_msgs::ImagePtr img_msg = cv_bridge::CvImage(std_msgs::Header(), "mono8", frame_cp).toImageMsg();
        img_msg->header.seq = frame_id;
        img_msg->header.stamp = time_now;
        pub_cd_frame_.publish(img_msg);
    }
}

int process_ui_for(const int &delay_ms) {
    auto then = std::chrono::high_resolution_clock::now();
    int key   = cv::waitKey(delay_ms);
    auto now  = std::chrono::high_resolution_clock::now();
    // cv::waitKey will not wait if no window is opened, so we wait for him, if needed
    std::this_thread::sleep_for(std::chrono::milliseconds(
        delay_ms - std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count()));

    return key;
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "prophesee_ros_viewer");
    ros::NodeHandle n;
    PropheseeWrapperViewer wv;

    while (ros::ok() && !wv.isInitialized()) {
        ros::spinOnce();
    }

    // period in seconds
    double period = wv.display_acc_time_/1000000.0;
    ROS_INFO("period is %f",period);
    double start_time = ros::Time::now().toSec();
    ROS_INFO("%f", start_time);
    double steps = 0.0;
    double time_left = 0.0;
    while (ros::ok()){
        steps += 1.0;
        ros::spinOnce();
        wv.showData();
        time_left = start_time + steps*period - ros::Time::now().toSec();
        //ROS_INFO("time left %f", time_left);
        if (time_left > 0){
            ros::Duration(time_left).sleep();
        }
    }
    ros::spin();
    return 0;
}
