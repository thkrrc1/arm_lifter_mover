#include <map>
#include "stroke_converter.h"

void seed_converter::ArmLifterMover::makeTables() {
    upper_csv_dir = ament_index_cpp::get_package_share_directory("arm_lifter_mover") + "/csv/typeG2_arm";
    lifter_csv_dir = ament_index_cpp::get_package_share_directory("arm_lifter_mover") + "/csv/typeG_lifter";

    if (makeTable(shoulder_p.table, upper_csv_dir + "/shoulder_p.csv"))
        makeInvTable(shoulder_p.inv_table, shoulder_p.table);
    if (makeTable(shoulder_r.table, upper_csv_dir + "/shoulder_r.csv"))
        makeInvTable(shoulder_r.inv_table, shoulder_r.table);
    if (makeTable(elbow_p.table, upper_csv_dir + "/elbow_p.csv"))
        makeInvTable(elbow_p.inv_table, elbow_p.table);
    if (makeTable(wrist_p.table, upper_csv_dir + "/wrist_p.csv"))
        makeInvTable(wrist_p.inv_table, wrist_p.table);
    if (makeTable(wrist_r.table, upper_csv_dir + "/wrist_r.csv"))
        makeInvTable(wrist_r.inv_table, wrist_r.table);
    if (makeTable(leg.table, lifter_csv_dir + "/leg.csv"))
        makeInvTable(leg.inv_table, leg.table);
}

int16_t seed_converter::ArmLifterMover::calcStroke1(double angle,double scale,double offset){
    static const double rad2Deg = 180.0 / M_PI;
    if (std::isnan(angle)) {
        return 0x7FFF;
    }

    return static_cast<int16_t>(scale * (rad2Deg * angle + offset));
}

int16_t seed_converter::ArmLifterMover::calcStroke2(double angle,double scale,int dir,double offset, const std::vector<seed_converter::StrokeMap> &_table){
    static const double rad2Deg = 180.0 / M_PI;
    if (std::isnan(angle)) {
        return 0x7FFF;
    }

    return static_cast<int16_t>(scale * setAngleToStroke(offset + dir * rad2Deg * angle, _table));
}


void seed_converter::ArmLifterMover::calcStroke3(double angle1,double angle2,double scale,int dir1,int dir2, const std::vector<seed_converter::StrokeMap> &_table1, const std::vector<seed_converter::StrokeMap> &_table2,bool is_pitch,int16_t& out1,int16_t& out2){
    static const double rad2Deg = 180.0 / M_PI;
    if (std::isnan(angle1) || std::isnan(angle2)) {
        out1 = 0x7FFF;
        out2 = 0x7FFF;
        return;
    }

    seed_converter::DiffJoint value = setDualAngleToStroke(dir2*rad2Deg * angle2, dir1*rad2Deg * angle1, _table2, _table1, is_pitch);
    out1 = static_cast<int16_t>(scale * value.one);
    out2 = static_cast<int16_t>(scale * value.two);

    return;
}


void seed_converter::ArmLifterMover::Angle2Stroke(std::vector<int16_t> &_strokes, const std::vector<double> &_angles) {
    static const float scale = 100.0;

    for(size_t idx = 0;idx < _strokes.size();++idx){
        _strokes[idx] = 0x7FFF;
    }

    _strokes[idx_shoulder_y] = calcStroke1(_angles[idx_shoulder_y],scale,0);
    _strokes[idx_wrist_y]    = calcStroke1(_angles[idx_wrist_y],scale,0);

    _strokes[idx_shoulder_p] = calcStroke2(_angles[idx_shoulder_p],scale,1,0, shoulder_p.table);
    _strokes[idx_elbow]      = calcStroke2(_angles[idx_elbow],scale,1,-90, elbow_p.table);
    _strokes[idx_thumb]      = calcStroke1(_angles[idx_thumb],-scale*0.18,-50);
   
    _strokes[idx_knee] = calcStroke2(_angles[idx_knee],scale,-1,0, leg.table);
    _strokes[idx_ankle] = calcStroke2(_angles[idx_ankle],scale,1,0, leg.table);

    calcStroke3(_angles[idx_wrist_p],_angles[idx_wrist_r],scale,1,-1,wrist_p.table, wrist_r.table, false,_strokes[idx_wrist_r],_strokes[idx_wrist_p]);
}

//無限回転ホイールの回転角度を求める(-180 ~ 180度)
int16_t seed_converter::ArmLifterMover::calcWheelAngDeg(int idx, int16_t raw_ang) {
    static constexpr int16_t max_ang = 13602; //最大値
    static constexpr int16_t min_ang = -13602; //最大値
    static constexpr int16_t max_ang_mod = max_ang % 360;
    static constexpr int16_t min_ang_mod = min_ang % 360;

    prev_wheel[idx] = cur_wheel[idx];
    cur_wheel[idx] = raw_ang;
    if (cur_wheel[idx] < -13000 && prev_wheel[idx] > 13000) {
        //順方向にオーバーフローした場合
        zero_ofst[idx] = (zero_ofst[idx] + max_ang_mod - min_ang_mod) % 360;
    } else if (prev_wheel[idx] < -13000 && cur_wheel[idx] > 13000) {
        //逆方向にオーバーフローした場合
        zero_ofst[idx] = (zero_ofst[idx] + min_ang_mod - max_ang_mod) % 360;
    }

    auto cur_ang = (cur_wheel[idx] + zero_ofst[idx])%360;
    if(cur_ang < 0){cur_ang += 360;}
    return (cur_ang - 180.);
}

void seed_converter::ArmLifterMover::Stroke2Angle(std::vector<double> &_angles, const std::vector<int16_t> &_strokes) {
    static const float deg2Rad = M_PI / 180.0;
    static const float scale_inv = 0.01;

    _angles[idx_shoulder_y] = deg2Rad * scale_inv * _strokes[idx_shoulder_y];
    _angles[idx_shoulder_p] = deg2Rad * setStrokeToAngle(scale_inv * _strokes[idx_shoulder_p], shoulder_p.inv_table);
    _angles[idx_elbow]      = (M_PI / 2) + deg2Rad * setStrokeToAngle(scale_inv * _strokes[idx_elbow], elbow_p.inv_table);
    _angles[idx_wrist_y]    = deg2Rad * scale_inv * _strokes[idx_wrist_y];
    _angles[idx_wrist_p]    =  deg2Rad * setStrokeToAngle(scale_inv * (_strokes[idx_wrist_r] + _strokes[idx_wrist_p]) * 0.5, wrist_p.inv_table);
    _angles[idx_wrist_r]    =  -deg2Rad * setStrokeToAngle(scale_inv * (_strokes[idx_wrist_r] - _strokes[idx_wrist_p]) * 0.5, wrist_r.inv_table);
    _angles[idx_thumb]      = -deg2Rad * (scale_inv * _strokes[idx_thumb] * 5.556 - 50.0);

    _angles[idx_knee] = -deg2Rad * setStrokeToAngle(scale_inv * _strokes[idx_knee], leg.inv_table);  // knee
    _angles[idx_ankle] = deg2Rad * setStrokeToAngle(scale_inv * _strokes[idx_ankle], leg.inv_table);  // ankle

    int16_t ang_fl = calcWheelAngDeg(0,_strokes[idx_wheel_front_left]);
    int16_t ang_fr = calcWheelAngDeg(1,_strokes[idx_wheel_front_right]);
    int16_t ang_rl = calcWheelAngDeg(2,_strokes[idx_wheel_rear_left]);
    int16_t ang_rr = calcWheelAngDeg(3,_strokes[idx_wheel_rear_right]);

    _angles[idx_wheel_front_left]  = deg2Rad * ang_fl;
    _angles[idx_wheel_front_right] = deg2Rad * ang_fr;
    _angles[idx_wheel_rear_left]   = deg2Rad * ang_rl;
    _angles[idx_wheel_rear_right]  = deg2Rad * ang_rr;
}

void seed_converter::ArmLifterMover::setJointNames(const std::vector<std::string> &names) {
    std::map<std::string, int> jnameIndex;
    jnameIndex.clear();
    for (size_t idx = 0; idx < names.size(); ++idx) {
        jnameIndex.emplace(names[idx], idx);
    }    

    idx_shoulder_p = jnameIndex["shoulder_p_joint"];
    idx_shoulder_y = jnameIndex["shoulder_y_joint"];
    idx_elbow = jnameIndex["elbow_joint"];
    idx_wrist_y = jnameIndex["wrist_y_joint"];
    idx_wrist_p = jnameIndex["wrist_p_joint"];
    idx_wrist_r = jnameIndex["wrist_r_joint"];    
    idx_thumb = jnameIndex["r_thumb_joint"];
    idx_knee = jnameIndex["knee_joint"];
    idx_ankle = jnameIndex["ankle_joint"];
    idx_wheel_front_left = jnameIndex["wheel_front_left"];
    idx_wheel_front_right = jnameIndex["wheel_front_right"];
    idx_wheel_rear_left = jnameIndex["wheel_rear_left"];
    idx_wheel_rear_right = jnameIndex["wheel_rear_right"];
}

void seed_converter::ArmLifterMover::calcActuatorVel(std::vector<int16_t> &actuator_vel, const std::vector<double> &joint_vel) {
    for (size_t idx = 0; idx < actuator_vel.size(); ++idx) {
        if (std::isnan(joint_vel[idx])) {
            actuator_vel[idx] = 0x7FFF;
        } else {
            actuator_vel[idx] = static_cast<int16_t>(joint_vel[idx] * 180.0 / M_PI);
        }
    }
}

void seed_converter::ArmLifterMover::calcJointVel(std::vector<double> &joint_vel,const std::vector<int16_t> &actuator_vel){
    for (size_t idx = 0; idx < joint_vel.size(); ++idx) {
        if (actuator_vel[idx] == 0x7FFF) {
            joint_vel[idx] = std::numeric_limits<double>::quiet_NaN();
        } else {
            joint_vel[idx] = static_cast<double>(actuator_vel[idx] * M_PI / 180.0);
        }
    }
}


#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(seed_converter::ArmLifterMover, seed_converter::StrokeConverter)
