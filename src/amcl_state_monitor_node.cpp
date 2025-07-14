#include <memory>
#include <chrono>

#include "rclcpp/rclcpp.hpp"
#include "lifecycle_msgs/srv/get_state.hpp"

using namespace std::chrono_literals;

class AmclStateMonitor : public rclcpp::Node
{
public:
  AmclStateMonitor()
    : Node("amcl_state_monitor"), triggered_(false)
    {
        RCLCPP_INFO(this->get_logger(), "Amcl state monitor started");
        this->declare_parameter<std::string>("robot_pkg_path", "");
        this->declare_parameter<bool>("simulation", true);

        robot_pkg_path_ = this->get_parameter("robot_pkg_path").as_string();
        simulation_ = this->get_parameter("simulation").as_bool();
        
        state_client_ = this->create_client<lifecycle_msgs::srv::GetState>("/amcl/get_state");

        timer_ = this->create_wall_timer(1000ms, std::bind(&AmclStateMonitor::poll_amcl_state, this));
    }

private:
    void poll_amcl_state()
    {
        if (!state_client_->service_is_ready()) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
                           "Service /amcl/get_state not yet available");
            return;
        }

        auto request = std::make_shared<lifecycle_msgs::srv::GetState::Request>();

    auto future = state_client_->async_send_request(request,
      [this](rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedFuture future_response) {
        auto response = future_response.get();
        if (response->current_state.id == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
          RCLCPP_INFO(this->get_logger(), "AMCL is active. Publishing initial pose.");
          bringup_tools();
          timer_->cancel();
        } else {
          RCLCPP_INFO(this->get_logger(), "AMCL current state: %s", response->current_state.label.c_str());
        }
      });
  }
    void bringup_tools() {
        if (!triggered_) {
            triggered_ = true;
            RCLCPP_INFO(this->get_logger(), "/amcl/get_state become available");
            std::string tf_cmd =
                "ros2 run tf2_ros static_transform_publisher "
                "--x 0 --y 0 --z 0 "
                "--qx 0 --qy 0 --qz 0 --qw 1 "
                "--frame-id map --child-frame-id odom &";
            std::system(tf_cmd.c_str());
        }
    }

    rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedPtr state_client_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::string robot_pkg_path_;
    bool simulation_;
    bool triggered_;
};


int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<AmclStateMonitor>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
