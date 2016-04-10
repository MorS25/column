#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Example Python node to listen on a specific topic.
"""
from __future__ import division
# Import required Python code.
import rospy
import time
import numpy as np
from mavros_msgs.msg import State
from geometry_msgs.msg import Pose
from std_msgs.msg import String
from geometry_msgs.msg import PoseStamped

def command_path_xy(start_setpoint, end_setpoint, speed_mps=1.0):
    """
    Setpoints as (x, y) tuples to be sent to /?_rel_setpoint parameter
    """
    path_length = np.sqrt((start_setpoint[0] - end_setpoint[0])**2 +
                          (start_setpoint[1] - end_setpoint[1])**2)
    duration_s = path_length / speed_mps

    # Create interpolated position arrays with 10 * duration(seconds) points
    x_array = np.linspace(start_setpoint[0], end_setpoint[0], duration_s*10.0);
    y_array = np.linspace(start_setpoint[1], end_setpoint[1], duration_s*10.0);
    
    for i in range(len(x_array)):
        # Apply setpoint parameters at 10 Hz
        
        # Home in on tag then land instead, if tag detected
        if rospy.get_param('/tag_detect') == 1:
            rospy.loginfo("Homing in on April Tag!!")
            while 1:
                new_rel_setpoint_x = rospy.get_param('/pose_last_tagupdate_x') + rospy.get_param('/filtered_tag_x')
                new_rel_setpoint_y = rospy.get_param('/pose_last_tagupdate_y') + rospy.get_param('/filtered_tag_y')
                new_rel_setpoint_yaw = rospy.get_param('/pose_last_tagupdate_yaw') + rospy.get_param('/filtered_tag_yaw')
                rospy.set_param('/x_rel_setpoint', new_rel_setpoint_x)
                rospy.set_param('/y_rel_setpoint', new_rel_setpoint_y)
                rospy.set_param('/yaw_rel_setpoint', new_rel_setpoint_yaw)
               # Give 2 seconds to stabilize
               # rospy.set_param('/x_rel_setpoint', rospy.get_param('/filtered_tag_x'))
               # rospy.set_param('/y_rel_setpoint', rospy.get_param('/filtered_tag_y'))
               # time.sleep(2) 
               # land_now()
        else:
            rospy.set_param('/x_rel_setpoint', float(x_array[i])) 
            rospy.set_param('/y_rel_setpoint', float(y_array[i]))
        time.sleep(0.1)

def land_now():
    rospy.loginfo("Landing Now")
    rospy.set_param('/land_now', 1)
    time.sleep(2.5)
    rospy.loginfo("Setting zero velocity target")
    rospy.set_param('/zero_vel', 1)
    rospy.spin() # Do nothing until node closes

def cone_search():
    '''
    Main function.
    '''
    while not rospy.is_shutdown(): 
        time.sleep(0.5)
        if rospy.get_param('/offboard') < 1:
            rospy.loginfo("Still Waiting for Offboard")
        else:
            break

    # Forward expanding cone search path (forward 2.4m, out +/- 0.9m)
    # X Y Z here is RIGHT FORWARDS UP    (mavros frame X, Y is negative)
    xy_setpoints = [( 0.0,  0.0),
                    ( 0.3,  0.5),
                    (-0.3,  0.5),
                    (-0.6,  1.0),
                    ( 0.6,  1.0),
                    ( 0.9,  1.5),
                    ( -0.9, 1.5),
                    ( -0.9, 2.0),
                    ( 0.9,  2.0)]

    # Visit each setpoint
    for i in range(len(xy_setpoints)-1):
        rospy.loginfo("Setting new waypoint: %f, %f", xy_setpoints[i+1][0],
                                                      xy_setpoints[i+1][1]) 
        command_path_xy(xy_setpoints[i], xy_setpoints[i+1], speed_mps=0.1)
        time.sleep(2)
    land_now()    

# Main function.
if __name__ == '__main__':
    # Initialize the node and name it.
    rospy.init_node('cone_script')
    # Go to the main loop.
    # /S# with correct custom mode we can wait until it changes
    # state_sub = rospy.Subscriber(mavros.get_topic('state'), State, state_cb)
    rospy.set_param('/yaw_rel_setpoint', 0)
    cone_search()