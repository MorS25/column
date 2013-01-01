/**
 * @file offb_node.cpp
 * @brief offboard example node, written with mavros version 0.14.2, px4 flight
 * stack and tested in Gazebo SITL
 */

#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/TwistStamped.h>
#include <mavros_msgs/CommandBool.h>
#include <mavros_msgs/SetMode.h>
#include <mavros_msgs/State.h>
#include <tf/transform_datatypes.h>

bool flag = false;

mavros_msgs::State current_state;
void state_cb(const mavros_msgs::State::ConstPtr& msg){
    current_state = *msg;
}

geometry_msgs::PoseStamped current_position;
void position_cb(const geometry_msgs::PoseStamped::ConstPtr& pose){
    current_position = *pose;
}

geometry_msgs::Pose tag_position_in_camera_frame;
geometry_msgs::Pose camera_position_in_tag_frame;
double current_position_at_last_tag_frame_x;
double current_position_at_last_tag_frame_y;
void tag_cb(const geometry_msgs::Pose::ConstPtr& pose){
    double roll, pitch, yaw;
    // Get the tag position in the camera frame
    tag_position_in_camera_frame = *pose;
    // Get the camera position in the camera frame
    //tf::Quaternion Q = tf::Quaternion(tag_position_in_camera_frame.orientation.x,
    //	tag_position_in_camera_frame.orientation.y, tag_position_in_camera_frame.orientation.z,
    //	tag_position_in_camera_frame.orientation.w);
    tf::Matrix3x3 R = tf::Matrix3x3(); // Get the rotation matrix
    yaw = tag_position_in_camera_frame.orientation.x;
    pitch = tag_position_in_camera_frame.orientation.y;
    roll = tag_position_in_camera_frame.orientation.z;
    //R.getRPY(roll,pitch,yaw);
    R.setEulerYPR(yaw, pitch, roll);
    R = R.inverse();
    //R.getRPY(roll,pitch,yaw);
    //ROS_INFO("Roll: %f, Pitch: %f, Yaw: %f",
    //	roll*180/3.1415926, pitch*180/3.1415926, yaw*180/3.1415926);
    camera_position_in_tag_frame.position.x = (R[0][0]*tag_position_in_camera_frame.position.x +
        R[0][1]*tag_position_in_camera_frame.position.y +
        R[0][2]*tag_position_in_camera_frame.position.z) * 0.0254;
    camera_position_in_tag_frame.position.y = (R[1][0]*tag_position_in_camera_frame.position.x + 
        R[1][1]*tag_position_in_camera_frame.position.y +
        R[1][2]*tag_position_in_camera_frame.position.z) * 0.0254;
    camera_position_in_tag_frame.position.z = (R[2][0]*tag_position_in_camera_frame.position.x +
        R[2][1]*tag_position_in_camera_frame.position.y +
        R[2][2]*tag_position_in_camera_frame.position.z) * 0.0254;
    camera_position_in_tag_frame.orientation.x = 0;
    camera_position_in_tag_frame.orientation.y = 0;
    camera_position_in_tag_frame.orientation.z = 0;
    camera_position_in_tag_frame.orientation.w = 0;
    current_position_at_last_tag_frame_x = current_position.pose.position.x;
    current_position_at_last_tag_frame_y = current_position.pose.position.y;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "offb_node");
    ros::NodeHandle nh;

    ros::Subscriber state_sub = nh.subscribe<mavros_msgs::State>
            ("mavros/state", 10, state_cb);
    ros::Subscriber tag_sub = nh.subscribe<geometry_msgs::Pose>
            ("april_pose", 10, tag_cb);
    ros::Publisher local_pos_pub = nh.advertise<geometry_msgs::PoseStamped>
            ("mavros/setpoint_position/local", 10);

    ros::Publisher set_vel_pub = nh.advertise<geometry_msgs::TwistStamped>
            ("mavros/setpoint_velocity/cmd_vel", 10);

    ros::ServiceClient arming_client = nh.serviceClient<mavros_msgs::CommandBool>
            ("mavros/cmd/arming");
    ros::ServiceClient set_mode_client = nh.serviceClient<mavros_msgs::SetMode>
            ("mavros/set_mode");
    ros::Subscriber local_pos_sub = nh.subscribe<geometry_msgs::PoseStamped>
            ("mavros/local_position/local", 10, position_cb);

    //the setpoint publishing rate MUST be faster than 2Hz
    ros::Rate rate(100.0);
    int publish_skip = 1*100; //Publish only every 2 seconds
    int publish_idx = 0;
    
    float avg_april_pose_x = 0.0; 
    float avg_april_pose_y = 0.0;

    // wait for FCU connection
    while(ros::ok() && current_state.connected){
        
        ros::spinOnce();
        rate.sleep();
    }


    geometry_msgs::TwistStamped twist_zero;
    twist_zero.header.stamp = ros::Time::now();
    twist_zero.header.frame_id = "fcu";
    twist_zero.twist.linear.x = 0.0;
    twist_zero.twist.linear.y = 0.0;
    twist_zero.twist.linear.z = 0.0;
    twist_zero.twist.angular.x = 0.0;
    twist_zero.twist.angular.y = 0.0;
    twist_zero.twist.angular.z = 0.0;

    geometry_msgs::TwistStamped twist_pub = twist_zero;

    //send a few setpoints before starting
    while(current_state.mode != "OFFBOARD" && ros::ok()){
        set_vel_pub.publish(twist_zero);
        ros::spinOnce();
        rate.sleep();
    }
    float kp = 1.0;

// Set reference / desired positions to current position ONCE when offboard enabled
    geometry_msgs::PoseStamped des_position = current_position;
    geometry_msgs::PoseStamped initial_position = current_position;

    ROS_INFO("Offboard mode enabled! ");

    ros::Time time_begin = ros::Time::now();
    while(ros::ok()){
      float time = (ros::Time::now()-time_begin).toSec();
      bool land_now = 0;
     //Set desired position sequence 
      if(time > 2){ //No changes for first 2 seconds
	if(time < 4){
          des_position.pose.position.x = initial_position.pose.position.x + 1.0;
	}
	else if(time < 8){
          des_position.pose.position.x = initial_position.pose.position.x + 1.0;
          des_position.pose.position.y = initial_position.pose.position.y + 1.0;
	}
	else if(time < 12){
          des_position.pose.position.x = initial_position.pose.position.x + 0.0;
          des_position.pose.position.y = initial_position.pose.position.y + 1.0;
	}
	else if(time < 16){
          des_position.pose.position.x = initial_position.pose.position.x + 0;
          des_position.pose.position.y = initial_position.pose.position.y + 0;
	}
        else{
	  land_now = 1;
	} 
       }
	//P velocity controller to des_position setpoint
      twist_pub = twist_zero;
      twist_pub.twist.linear.x = kp*(des_position.pose.position.x - current_position.pose.position.x);
      twist_pub.twist.linear.y = kp*(des_position.pose.position.y - current_position.pose.position.y);
      twist_pub.twist.linear.z = kp*(des_position.pose.position.z - current_position.pose.position.z);
      // Overwrite Z velocity if land_now == 1
      if(land_now){twist_pub.twist.linear.z = -1;}	

      set_vel_pub.publish(twist_pub);

	//Print info at 2 hz	
	if(publish_idx % (publish_skip/2) == 0){ 
        ROS_INFO("Time (s): %f", time);
        ROS_INFO("Cmd velocities:   vx:%f vy:%f vz:%f", twist_pub.twist.linear.x, twist_pub.twist.linear.y, twist_pub.twist.linear.z);
        ROS_INFO("Position Setpoint: x:%f  y:%f  z:%f", des_position.pose.position.x, des_position.pose.position.y, des_position.pose.position.z);
	}
	
        ros::spinOnce();
        rate.sleep();
    }

    return 0;
}


