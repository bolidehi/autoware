/*
 *  Copyright (c) 2015, Nagoya University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of Autoware nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>
#include "opencv2/contrib/contrib.hpp"

#include "ros/ros.h"
#include <sensor_msgs/image_encodings.h>
#include "points2image/PointsImage.h"

#if 0
#include "dpm/ImageObjects.h"
#else
#include "car_detector/FusedObjects.h"
#endif
#include <vector>
#include <iostream>
#include <math.h>
#include <float.h>

using namespace std;
using namespace cv;

#define NO_DATA 0
static char window_name[] = "points_image_d_viewer";

static bool existImage = false;
static bool existPoints = false;
static sensor_msgs::Image image_msg;
static points2image::PointsImageConstPtr points_msg;
static cv::Mat colormap;

#if 0
static vector<Rect> cars;
static vector<Rect> peds;
#else
static car_detector::FusedObjects car_fused_objects;
static car_detector::FusedObjects pedestrian_fused_objects;
#endif

/* check whether floating value x is nearly 0 or not */
static inline bool isNearlyNODATA(float x)
{
  float abs_x  = (float)fabs(x);
  const int rangeScale = 100;
  return(abs_x < FLT_MIN*rangeScale);
}

static vector<Scalar> _colors;

#define	IMAGE_WIDTH	800
#define	IMAGE_HEIGHT	600

static const int OBJ_RECT_THICKNESS = 3;

static void drawRects(IplImage *Image,
                      int object_num,
                      std::vector<int> corner_point,
                      CvScalar color,
                      int threshold_height,
                      std::vector<float> distance)
{
  for(int i = 0; i < object_num; i++) {
    if (corner_point[1+i*4] > threshold_height && !isNearlyNODATA(distance.at(i))) {  // temporal way to avoid drawing detections in the sky
      CvPoint p1=cvPoint(corner_point[0+i*4], corner_point[1+i*4]);
      CvPoint p2=cvPoint(corner_point[0+i*4] + corner_point[2+i*4], corner_point[1+i*4] + corner_point[3+i*4]);
      cvRectangle(Image,p1,p2,color,OBJ_RECT_THICKNESS);
    }
  }
}

static void putDistance(IplImage *Image,
                        car_detector::FusedObjects objects,
                        int threshold_height,
                        const char* objectLabel)
{
  char distance_string[32];
  CvFont dfont;
  float hscale	    = 0.7f;
  float vscale	    = 0.7f;
  float italicscale = 0.0f;
  int	thickness   = 1;

  CvFont      dfont_label;
  float       hscale_label = 0.5f;
  float       vscale_label = 0.5f;
  CvSize      text_size;
  int         baseline     = 0;

  cvInitFont(&dfont_label, CV_FONT_HERSHEY_COMPLEX, hscale_label, vscale_label, italicscale, thickness, CV_AA);
  cvGetTextSize(objectLabel,
                &dfont_label,
                &text_size,
                &baseline);

  for (int i=0; i<objects.car_num; i++)
    {
      if (objects.corner_point[1+i*4] > threshold_height) // temporal way to avoid drawing detections in the sky
        {
          if (!isNearlyNODATA(objects.distance.at(i)))
            {

              /*put label */
              CvPoint labelOrg = cvPoint(objects.corner_point[0+i*4] - OBJ_RECT_THICKNESS,
                                         objects.corner_point[1+i*4] - baseline - OBJ_RECT_THICKNESS);

              cvRectangle(Image,
                          cvPoint(labelOrg.x + 0, labelOrg.y + baseline),
                          cvPoint(labelOrg.x + text_size.width, labelOrg.y - text_size.height),
                          CV_RGB(0, 0, 0), // label background is black
                          -1, 8, 0
                          );
              cvPutText(Image,
                        objectLabel,
                        labelOrg,
                        &dfont_label,
                        CV_RGB(255, 255, 255) // label text color is white
                        );

              /* put distance data */
              cvRectangle(Image,
                          cv::Point(objects.corner_point[0+i*4] + (objects.corner_point[2+i*4]/2) - (((int)log10(objects.distance.at(i)/100)+1) * 5 + 45),
                                    objects.corner_point[1+i*4] + objects.corner_point[3+i*4] + 5),
                          cv::Point(objects.corner_point[0+i*4] + (objects.corner_point[2+i*4]/2) + (((int)log10(objects.distance.at(i)/100)+1) * 8 + 38),
                                    objects.corner_point[1+i*4] + objects.corner_point[3+i*4] + 30),
                          cv::Scalar(255,255,255),
                          -1);

              cvInitFont (&dfont,
                          CV_FONT_HERSHEY_COMPLEX,
                          hscale,
                          vscale,
                          italicscale,
                          thickness,
                          CV_AA);

              sprintf(distance_string, "%.2f m", objects.distance.at(i) / 100); //unit of length is meter
              cvPutText(Image,
                        distance_string,
                        cvPoint(objects.corner_point[0+i*4] + (objects.corner_point[2+i*4]/2) - (((int)log10(objects.distance.at(i)/100)+1) * 5 + 40),
                                objects.corner_point[1+i*4] + objects.corner_point[3+i*4] + 25),
                        &dfont,
                        CV_RGB(255, 0, 0));
            }

        }
    }
}

void show(void)
{
  if(!existImage || !existPoints){
    return;
  }
  const auto& encoding = sensor_msgs::image_encodings::TYPE_8UC3;
  cv_bridge::CvImagePtr cv_image = cv_bridge::toCvCopy(image_msg, encoding);
  IplImage frame = cv_image->image;

  cv::Mat matImage(&frame, false);
  cv::cvtColor(matImage, matImage, CV_BGR2RGB);

  /* DRAW RECTANGLES of detected objects */
#if 0
  for(std::size_t i=0; i<cars.size();i++) {
      if(cars[i].y > matImage.rows*.3) { //temporal way to avoid drawing detections in the sky
          cvRectangle( &frame,
                       cvPoint(cars[i].x, cars[i].y),
                       cvPoint(cars[i].x+cars[i].width, cars[i].y+cars[i].height),
                       _colors[0], 3, 8,0 );
    }
  }
  for(std::size_t i=0; i<peds.size();i++) {
    if(peds[i].y > matImage.rows*.3) {
      cvRectangle( &frame,
                   cvPoint(peds[i].x, peds[i].y),
                   cvPoint(peds[i].x+peds[i].width, peds[i].y+peds[i].height),
                   _colors[1], 3, 8,0 );
    }
  }
#else
  drawRects(&frame,
            car_fused_objects.car_num,
            car_fused_objects.corner_point,
            cvScalar(255.0, 255.0, 0,0),
            matImage.rows*.25,
            car_fused_objects.distance);

  drawRects(&frame,
            pedestrian_fused_objects.car_num,
            pedestrian_fused_objects.corner_point,
            cvScalar(0.0, 255.0, 0,0),
            matImage.rows*.25,
            pedestrian_fused_objects.distance);
#endif
  /* PUT DISTANCE text on image */
  putDistance(&frame,
              car_fused_objects,
              matImage.rows*.25,
              "car");
  putDistance(&frame,
              pedestrian_fused_objects,
              matImage.rows*.25,
              "pedestrian");

  /* DRAW POINTS of lidar scanning */
  int w = matImage.size().width;
  int h = matImage.size().height;

  int n = w * h;
  float min_d = 1<<16, max_d = -1;
  //	int min_i = 1<<8, max_i = -1;
  for(int i=0; i<n; i++){
    int di = points_msg->distance[i];
    max_d = di > max_d ? di : max_d;
    min_d = di < min_d ? di : min_d;
    // int it = points_msg->intensity[i];
    // max_i = it > max_i ? it : max_i;
    // min_i = it < min_i ? it : min_i;
  }
  float wid_d = max_d - min_d;

  for(int y=0; y<h; y++){
    for(int x=0; x<w; x++){
      int j = y * w + x;
      double distance = points_msg->distance[j];
      if(distance == 0){
        continue;
      }
      int colorid= wid_d ? ( (distance - min_d) * 255 / wid_d ) : 128;
      cv::Vec3b color=colormap.at<cv::Vec3b>(colorid);
      int g = color[1];
      int b = color[2];
      int r = color[0];
      cvRectangle(&frame, cvPoint(x, y), cvPoint(x+1, y+1), CV_RGB(r, g, b));
    }
  }
  cvShowImage(window_name, &frame);
  cvWaitKey(2);
}

#if 0
static void car_updater_callback(dpm::ImageObjects image_objects_msg)
{
  int num = image_objects_msg.car_num;
  vector<int> points = image_objects_msg.corner_point;
  //points are X,Y,W,H and repeat for each instance
  cars.clear();

  for (int i=0; i<num;i++) {
    Rect tmp;
    tmp.x = points[i*4 + 0];
    tmp.y = points[i*4 + 1];
    tmp.width = points[i*4 + 2];
    tmp.height = points[i*4 + 3];
    cars.push_back(tmp);
  }
}
#else
static void car_updater_callback(const car_detector::FusedObjects& fused_car_msg)
{
  car_fused_objects = fused_car_msg;
  //  show();
}
#endif

#if 0
static void ped_updater_callback(dpm::ImageObjects image_objects_msg)
{
  int num = image_objects_msg.car_num;
  vector<int> points = image_objects_msg.corner_point;
  //points are X,Y,W,H and repeat for each instance
  peds.clear();

  for (int i=0; i<num;i++) {
    Rect tmp;
    tmp.x = points[i*4 + 0];
    tmp.y = points[i*4 + 1];
    tmp.width = points[i*4 + 2];
    tmp.height = points[i*4 + 3];
    peds.push_back(tmp);
  }
}
#else
static void ped_updater_callback(const car_detector::FusedObjects& fused_pds_msg)
{
  pedestrian_fused_objects = fused_pds_msg;
  //  show();
}
#endif

static void image_cb(const sensor_msgs::Image& msg)
{
  image_msg = msg;
  existImage = true;
  show();
}

static void points_cb(const points2image::PointsImageConstPtr& msg)
{
  points_msg = msg;
  existPoints = true;
  show();
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "points_image_d_viewer");
  ros::NodeHandle n;
  ros::NodeHandle private_nh("~");

  string image_node;
  string car_node;
  string pedestrian_node;
  string points_node;

  if (private_nh.getParam("image_node", image_node)) {
    ROS_INFO("Setting image node to %s", image_node.c_str());
  } else {
    ROS_INFO("No image node received, defaulting to image_raw, you can use _image_node:=YOUR_NODE");
    image_node = "/image_raw";
  }

  if (private_nh.getParam("car_node", car_node)) {
    ROS_INFO("Setting car positions node to %s", car_node.c_str());
  } else {
    ROS_INFO("No car positions node received, defaulting to car_pixel_xyz, you can use _car_node:=YOUR_TOPIC");
    car_node = "/car_pixel_xyz";
  }

  if (private_nh.getParam("pedestrian_node", pedestrian_node)) {
    ROS_INFO("Setting pedestrian positions node to %s", pedestrian_node.c_str());
  } else {
    ROS_INFO("No pedestrian positions node received, defaulting to pedestrian_pixel_xyz, you can use _pedestrian_node:=YOUR_TOPIC");
    pedestrian_node = "/pedestrian_pixel_xyz";
  }

  if (private_nh.getParam("points_node", points_node)) {
    ROS_INFO("Setting pedestrian positions node to %s", points_node.c_str());
  } else {
    ROS_INFO("No points node received, defaulting to points_image, you can use _points_node:=YOUR_TOPIC");
    points_node = "/points_image";
  }

  cv::generateColors(_colors, 25);

  ros::Subscriber scriber = n.subscribe(image_node, 1,
                                        image_cb);
  ros::Subscriber scriber_car = n.subscribe(car_node, 1,
                                            car_updater_callback);
  ros::Subscriber scriber_ped = n.subscribe(pedestrian_node, 1,
                                            ped_updater_callback);
  ros::Subscriber scriber_points = n.subscribe(points_node, 1,
                                               points_cb);

  cv::Mat grayscale(256,1,CV_8UC1);
  for(int i=0;i<256;i++) {
    grayscale.at<uchar>(i)=i;
  }
  cv::applyColorMap(grayscale,colormap,cv::COLORMAP_JET);

  ros::spin();
  return 0;
}
