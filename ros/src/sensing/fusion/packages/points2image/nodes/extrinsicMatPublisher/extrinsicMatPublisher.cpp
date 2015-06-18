#include <ros/ros.h>
#include <sensor_msgs/CameraInfo.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_datatypes.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <sensor_msgs/Image.h>

static cv::Mat  CameraExtrinsicMat;
static cv::Mat  CameraMat;
static cv::Mat  DistCoeff;
static cv::Size ImageSize;

static ros::Publisher camera_info_pub;

void tfRegistration (const cv::Mat &camExtMat, const ros::Time& timeStamp)
{
  tf::Matrix3x3 rotation_mat;
  double roll = 0, pitch = 0, yaw = 0;
  tf::Quaternion quaternion;
  tf::Transform transform;
  static tf::TransformBroadcaster broadcaster;

 rotation_mat.setValue(camExtMat.at<double>(0, 0), camExtMat.at<double>(0, 1), camExtMat.at<double>(0, 2),
                        camExtMat.at<double>(1, 0), camExtMat.at<double>(1, 1), camExtMat.at<double>(1, 2),
                        camExtMat.at<double>(2, 0), camExtMat.at<double>(2, 1), camExtMat.at<double>(2, 2));

  rotation_mat.getRPY(roll, pitch, yaw, 1);

  quaternion.setRPY(roll, pitch, yaw);

  transform.setOrigin(tf::Vector3(camExtMat.at<double>(0, 3),
                                  camExtMat.at<double>(1, 3),
                                  camExtMat.at<double>(2, 3)));

  transform.setRotation(quaternion);


  broadcaster.sendTransform(tf::StampedTransform(transform, timeStamp, "velodyne", "camera"));
  //broadcaster.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "velodyne", "camera"));
}


static void image_raw_cb(const sensor_msgs::Image& image_msg)
{
  //ros::Time timeStampOfImage = image_msg.header.stamp;

  ros::Time timeStampOfImage;
  timeStampOfImage.sec = image_msg.header.stamp.sec;
  timeStampOfImage.nsec = image_msg.header.stamp.nsec;


  /* create TF between velodyne and camera with time stamp of /image_raw */
  tfRegistration(CameraExtrinsicMat, timeStampOfImage);

}

void cameraInfo_sender(const cv::Mat  &camMat,
                       const cv::Mat  &distCoeff,
                       const cv::Size &imgSize)
{
  sensor_msgs::CameraInfo msg;

  msg.header.frame_id = "camera";

  msg.height = imgSize.height;
  msg.width  = imgSize.width;

  for (int row=0; row<3; row++) {
      for (int col=0; col<3; col++) {
        msg.K[row * 3 + col] = camMat.at<double>(row, col);
        }
    }

  for (int row=0; row<3; row++) {
    for (int col=0; col<4; col++) {
      if (col == 3) {
        msg.P[row * 4 + col] = 0.0f;
      } else {
        msg.P[row * 4 + col] = camMat.at<double>(row, col);
      }
    }
  }

  for (int row=0; row<distCoeff.rows; row++) {
    for (int col=0; col<distCoeff.cols; col++) {
      msg.D.push_back(distCoeff.at<double>(row, col));
    }
  }

  camera_info_pub.publish(msg);
}


int main(int argc, char* argv[])
{
  ros::init(argc, argv, "extrinsicMatPublisher");
  ros::NodeHandle n;

  camera_info_pub = n.advertise<sensor_msgs::CameraInfo>("/camera/camera_info", 10, true);

  if (argc < 2)
    {
      std::cout << "Usage: extrinsicMatPublisher <calibration-file>." << std::endl;
      return -1;
    }

  cv::FileStorage fs(argv[1], cv::FileStorage::READ);
  if (!fs.isOpened())
    {
      std::cout << "Cannot open " << argv[1] << std::endl;
      return -1;
    }

  fs["CameraExtrinsicMat"] >> CameraExtrinsicMat;
  fs["CameraMat"] >> CameraMat;
  fs["DistCoeff"] >> DistCoeff;
  fs["ImageSize"] >> ImageSize;

  ros::Subscriber image_sub = n.subscribe("/image_raw", 10, image_raw_cb);

  cameraInfo_sender(CameraMat, DistCoeff, ImageSize);

  ros::spin();

  return 0;

}
