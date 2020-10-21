#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <opencv2/highgui/highgui.hpp>
#include <cv_bridge/cv_bridge.h>
#include <ros/console.h>

void imageCallback(const sensor_msgs::ImageConstPtr& msg)
{
  cv::Mat img;
  try
  {
    img = cv_bridge::toCvShare(msg, "mono8")->image;
  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("Could not convert from '%s' to 'mono8'.", msg->encoding.c_str());
  }
  //cv::imshow("view", img);
  //cv::waitKey(30);
  //ROS_INFO("id = %i", msg->header.frame_id);
  //ROS_INFO("t = %i, %i", msg->header.stamp.sec, msg->header.stamp.nsec);
  //std::cout << msg->header << std::endl;
  ROS_INFO_STREAM(msg->header);
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "image_listener");
  ros::NodeHandle nh;
  cv::namedWindow("view",CV_WINDOW_NORMAL);
  //cv::startWindowThread();
  image_transport::ImageTransport it(nh);
  image_transport::Subscriber sub = it.subscribe("/CD_frame", 1, imageCallback);
  ros::spin();
  cv::destroyWindow("view");
}