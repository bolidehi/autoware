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

/*
 Localization and mapping program using Normal Distributions Transform

 Yuki KITSUKAWA
 */

/*
$: rostopic pub --once /output std_msgs/Float32 0.2
 */

#define DEBUG 0
// #define VIEW_TIME
#define OUTPUT // If you want to output "position_log.txt", "#define OUTPUT".
#define THRESHOLD 1.0
#define RADIUS 10.0

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float32.h>
#include <sensor_msgs/PointCloud2.h>
#include <velodyne_pointcloud/point_types.h>
#include <velodyne_pointcloud/rawdata.h>

#include <tf/transform_broadcaster.h>
#include <tf/transform_datatypes.h>

#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/registration/ndt.h>
#include <pcl/filters/approximate_voxel_grid.h>
#include <pcl/filters/voxel_grid.h>

#include <runtime_manager/ConfigNdt.h>

struct Position {
    double x;
    double y;
    double z;
    double roll;
    double pitch;
    double yaw;
};

// global variables
static Position previous_pos, guess_pos, current_pos, added_pos;

static double offset_x, offset_y, offset_z, offset_yaw; // current_pos - previous_pos

static pcl::PointCloud<pcl::PointXYZI> map;

static pcl::NormalDistributionsTransform<pcl::PointXYZI, pcl::PointXYZI> ndt;
// Default values
static int iter = 30; // Maximum iterations
static float ndt_res = 1.0; // Resolution
static double step_size = 0.1; // Step size
static double trans_eps = 0.01; // Transformation epsilon

// Leaf size of VoxelGrid filter.
static double voxel_leaf_size = 2.0;

static ros::Time callback_start, callback_end, t1_start, t1_end, t2_start, t2_end, t3_start, t3_end, t4_start, t4_end, t5_start, t5_end;
static ros::Duration d_callback, d1, d2, d3, d4, d5;

static ros::Publisher ndt_map_pub;
static ros::Publisher ndt_pose_pub;
static geometry_msgs::PoseStamped ndt_pose_msg;

static ros::Publisher ndt_stat_pub;
static std_msgs::Bool ndt_stat_msg;

static int count = 0;
static int initial_scan_loaded = 0;

static Eigen::Matrix4f gnss_transform = Eigen::Matrix4f::Identity();

static void output_callback(const std_msgs::Float32::ConstPtr& input)
{
  double voxel_leaf_size = input->data;

  pcl::PointCloud<pcl::PointXYZI>::Ptr map_ptr(new pcl::PointCloud<pcl::PointXYZI>(map));
  pcl::PointCloud<pcl::PointXYZI>::Ptr map_filtered(new pcl::PointCloud<pcl::PointXYZI>());
  map_ptr->header.frame_id = "map";
  map_filtered->header.frame_id = "map";

  // Apply voxelgrid filter
  pcl::VoxelGrid<pcl::PointXYZI> voxel_grid_filter;
  voxel_grid_filter.setLeafSize(voxel_leaf_size, voxel_leaf_size, voxel_leaf_size);
  voxel_grid_filter.setInputCloud(map_ptr);
  voxel_grid_filter.filter(*map_filtered);
  std::cout << "Original: " << map_ptr->points.size() << " points." << std::endl;
  std::cout << "Filtered: " << map_filtered->points.size() << " points." << std::endl;

  sensor_msgs::PointCloud2::Ptr map_msg_ptr(new sensor_msgs::PointCloud2);
  pcl::toROSMsg(*map_filtered, *map_msg_ptr);

  ndt_map_pub.publish(*map_msg_ptr);

  // Writing Point Cloud data to PCD file
  pcl::io::savePCDFileASCII("local_map.pcd", *map_filtered);
  std::cout << "Saved " << map_filtered->points.size() << " data points to local_map.pcd." << std::endl;

}

static void velodyne_callback(const pcl::PointCloud<velodyne_pointcloud::PointXYZIR>::ConstPtr& input)
{
  ros::Time scan_time;

  pcl::PointXYZI p; 
  pcl::PointCloud<pcl::PointXYZI> scan;
  pcl::PointCloud<pcl::PointXYZI>::Ptr filtered_scan_ptr (new pcl::PointCloud<pcl::PointXYZI>());
  pcl::PointCloud<pcl::PointXYZI>::Ptr transformed_scan_ptr (new pcl::PointCloud<pcl::PointXYZI>());
  tf::Quaternion q;
  Eigen::Matrix4f t(Eigen::Matrix4f::Identity());

  scan.header = input->header;
  scan_time.sec = scan.header.stamp / 1000000.0;
  scan_time.nsec = (scan.header.stamp - scan_time.sec * 1000000.0) * 1000.0;
    
//     /*
//       std::cout << "scan.header.stamp: " << scan.header.stamp << std::endl;
//       std::cout << "scan_time: " << scan_time << std::endl;
//       std::cout << "scan_time.sec: " << scan_time.sec << std::endl;
//       std::cout << "scan_time.nsec: " << scan_time.nsec << std::endl;
//     */
    
//     t1_start = ros::Time::now();

  for (pcl::PointCloud<velodyne_pointcloud::PointXYZIR>::const_iterator item = input->begin(); item != input->end(); item++) {
    p.x = (double) item->x;
    p.y = (double) item->y;
    p.z = (double) item->z;
    p.intensity = (float) item->intensity;

    double r = p.x * p.x + p.y * p.y;

    if(r >= RADIUS){
      scan.points.push_back(p);
    }
  }

  pcl::PointCloud<pcl::PointXYZI>::Ptr scan_ptr(new pcl::PointCloud<pcl::PointXYZI>(scan));

  // Add initial point cloud to velodyne_map
  if(initial_scan_loaded == 0){
    map += *scan_ptr;
    initial_scan_loaded = 1;
    std::cout << "initial_scan_loaded." << std::endl;
  }

  // Apply voxelgrid filter
  pcl::VoxelGrid<pcl::PointXYZI> voxel_grid_filter;
  voxel_grid_filter.setLeafSize(voxel_leaf_size, voxel_leaf_size, voxel_leaf_size);
  voxel_grid_filter.setInputCloud(scan_ptr);
  voxel_grid_filter.filter(*filtered_scan_ptr);

  // Matching with map
  ndt.setTransformationEpsilon(trans_eps);
  ndt.setStepSize(step_size);
  ndt.setResolution(ndt_res);
  ndt.setMaximumIterations(iter);

  ndt.setInputSource(filtered_scan_ptr);

  pcl::PointCloud<pcl::PointXYZI>::Ptr map_ptr(new pcl::PointCloud<pcl::PointXYZI>(map));

  map_ptr->header.frame_id = "map";
  // Setting point cloud to be aligned to.
  ndt.setInputTarget(map_ptr);

  tf::Matrix3x3 init_rotation;
    
  guess_pos.x = previous_pos.x + offset_x;
  guess_pos.y = previous_pos.y + offset_y;
  guess_pos.z = previous_pos.z + offset_z;
  guess_pos.roll = previous_pos.roll;
  guess_pos.pitch = previous_pos.pitch;
  guess_pos.yaw = previous_pos.yaw + offset_yaw;
  
  Eigen::AngleAxisf init_rotation_x(guess_pos.roll, Eigen::Vector3f::UnitX());
  Eigen::AngleAxisf init_rotation_y(guess_pos.pitch, Eigen::Vector3f::UnitY());
  Eigen::AngleAxisf init_rotation_z(guess_pos.yaw, Eigen::Vector3f::UnitZ());
  
  Eigen::Translation3f init_translation(guess_pos.x, guess_pos.y, guess_pos.z);
  
  Eigen::Matrix4f init_guess = (init_translation * init_rotation_z * init_rotation_y * init_rotation_x).matrix();
  
  t3_end = ros::Time::now();
  d3 = t3_end - t3_start;
  
  t4_start = ros::Time::now();
  //  pcl::PointCloud<pcl::PointXYZ>::Ptr output_cloud(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::PointCloud<pcl::PointXYZI>::Ptr output_cloud(new pcl::PointCloud<pcl::PointXYZI>);
  ndt.align(*output_cloud, init_guess);
  
  t = ndt.getFinalTransformation();

  //  pcl::transformPointCloud(*filtered_scan_ptr, *transformed_scan_ptr, t);
  pcl::transformPointCloud(*scan_ptr, *transformed_scan_ptr, t);

  tf::Matrix3x3 tf3d;
    
  tf3d.setValue(static_cast<double>(t(0, 0)), static_cast<double>(t(0, 1)), static_cast<double>(t(0, 2)),
		static_cast<double>(t(1, 0)), static_cast<double>(t(1, 1)), static_cast<double>(t(1, 2)),
		static_cast<double>(t(2, 0)), static_cast<double>(t(2, 1)), static_cast<double>(t(2, 2)));
    
    // Update current_pos.
  current_pos.x = t(0, 3);
  current_pos.y = t(1, 3);
  current_pos.z = t(2, 3);
  tf3d.getRPY(current_pos.roll, current_pos.pitch, current_pos.yaw, 1);

  // Calculate the offset (curren_pos - previous_pos)
  offset_x = current_pos.x - previous_pos.x;
  offset_y = current_pos.y - previous_pos.y;
  offset_z = current_pos.z - previous_pos.z;
  offset_yaw = current_pos.yaw - previous_pos.yaw;
  
  // Update position and posture. current_pos -> previous_pos
  previous_pos.x = current_pos.x;
  previous_pos.y = current_pos.y;
  previous_pos.z = current_pos.z;
  previous_pos.roll = current_pos.roll;
  previous_pos.pitch = current_pos.pitch;
  previous_pos.yaw = current_pos.yaw;

  // Calculate the offset between added_pos and current_pos
  double offset = sqrt(pow(current_pos.x-added_pos.x, 2.0) + pow(current_pos.y-added_pos.y, 2.0));
  std::cout << "offset: " << offset << std::endl;

  if(offset >= THRESHOLD){
    map += *transformed_scan_ptr;
    added_pos.x = current_pos.x;
    added_pos.y = current_pos.y;
    added_pos.z = current_pos.z;
    added_pos.roll = current_pos.roll;
    added_pos.pitch = current_pos.pitch;
    added_pos.yaw = current_pos.yaw;
    std::cout << "add velodyne_points to map" << std::endl;
  }

  sensor_msgs::PointCloud2::Ptr map_msg_ptr(new sensor_msgs::PointCloud2);
  pcl::toROSMsg(*map_ptr, *map_msg_ptr);

  ndt_map_pub.publish(*map_msg_ptr);

  q.setRPY(current_pos.roll, current_pos.pitch, current_pos.yaw);
  ndt_pose_msg.header.frame_id = "map";
  ndt_pose_msg.header.stamp = scan_time;
  ndt_pose_msg.pose.position.x = current_pos.x;
  ndt_pose_msg.pose.position.y = current_pos.y;
  ndt_pose_msg.pose.position.z = current_pos.z;
  ndt_pose_msg.pose.orientation.x =  q.x();
  ndt_pose_msg.pose.orientation.y =  q.y();
  ndt_pose_msg.pose.orientation.z =  q.z();
  ndt_pose_msg.pose.orientation.w =  q.w();
  
  ndt_pose_pub.publish(ndt_pose_msg);

  std::cout << "-----------------------------------------------------------------" << std::endl;
  std::cout << "count: " << count << std::endl;
  std::cout << "Sequence number: " << input->header.seq << std::endl;
  std::cout << "Number of scan points: " << scan_ptr->size() << " points." << std::endl;
  std::cout << "Number of filtered scan points: " << filtered_scan_ptr->size() << " points." << std::endl;
  std::cout << "transformed_scan_ptr: " << transformed_scan_ptr->points.size() << " points." << std::endl;
  std::cout << "map: " << map.points.size() << " points." << std::endl;
  std::cout << "NDT has converged: " << ndt.hasConverged() << std::endl;
  std::cout << "Fitness score: " << ndt.getFitnessScore() << std::endl;
  std::cout << "Number of iteration: " << ndt.getFinalNumIteration() << std::endl;
  std::cout << "(x,y,z,roll,pitch,yaw):" << std::endl;
  std::cout << "(" << current_pos.x << ", " << current_pos.y << ", " << current_pos.z << ", " << current_pos.roll << ", " << current_pos.pitch << ", " << current_pos.yaw << ")" << std::endl;
  std::cout << "Transformation Matrix:" << std::endl;
  std::cout << t << std::endl;
  std::cout << "-----------------------------------------------------------------" << std::endl;

  count++;
}

int main(int argc, char **argv)
{
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "NDT_SLAM program coded by Yuki KITSUKAWA" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    previous_pos.x = 0.0;
    previous_pos.y = 0.0;
    previous_pos.z = 0.0;
    previous_pos.roll = 0.0;
    previous_pos.pitch = 0.0;
    previous_pos.yaw = 0.0;

    current_pos.x = 0.0;
    current_pos.y = 0.0;
    current_pos.z = 0.0;
    current_pos.roll = 0.0;
    current_pos.pitch = 0.0;
    current_pos.yaw = 0.0;

    guess_pos.x = 0.0;
    guess_pos.y = 0.0;
    guess_pos.z = 0.0;
    guess_pos.roll = 0.0;
    guess_pos.pitch = 0.0;
    guess_pos.yaw = 0.0;

    added_pos.x = 0.0;
    added_pos.y = 0.0;
    added_pos.z = 0.0;
    added_pos.roll = 0.0;
    added_pos.pitch = 0.0;
    added_pos.yaw = 0.0;

    offset_x = 0.0;
    offset_y = 0.0;
    offset_z = 0.0;
    offset_yaw = 0.0;

    ros::init(argc, argv, "ndt_slam");
    ros::NodeHandle n;

    /*
    ndt.setTransformationEpsilon(trans_eps);
    ndt.setStepSize(step_size);
    ndt.setResolution(ndt_res);
    ndt.setMaximumIterations(iter);
    */

    ndt_map_pub = n.advertise<sensor_msgs::PointCloud2>("/ndt_map", 1000);
    ndt_pose_pub = n.advertise<geometry_msgs::PoseStamped>("/ndt_pose", 1000);

    // subscribing the velodyne data
    ros::Subscriber velodyne_sub = n.subscribe("velodyne_points", 1000, velodyne_callback);
    ros::Subscriber output_sub = n.subscribe("output", 1000, output_callback);

    ros::spin();

    return 0;
}
