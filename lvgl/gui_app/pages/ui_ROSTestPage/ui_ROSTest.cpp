#include "ui_ROSTest.h"
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"

///////////////////// VARIABLES ////////////////////


///////////////////// ANIMATIONS ////////////////////


///////////////////// FUNCTIONS ////////////////////


static void ui_event_back_btn(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        lv_lib_pm_OpenPrePage(&page_manager);
    }
}


class PublisherNode : public rclcpp::Node
{
    public:
        PublisherNode()
        : Node("topic_helloworld_pub") // ROS2节点父类初始化
        {
            // 创建发布者对象（消息类型、话题名、队列长度）
            publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/turtle1/cmd_vel", 10);
        }

        void publish_twist(float linear_x, float angular_z)
        {
            geometry_msgs::msg::Twist msg;
            msg.linear.x = linear_x;
            msg.linear.y = 0.0f;
            msg.linear.z = 0.0f;
            msg.angular.x = 0.0f;
            msg.angular.y = 0.0f;
            msg.angular.z = angular_z;
            publisher_->publish(msg);
        }

    private:
    	// 发布者指针
	rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
};

struct TwistCommand {
    float linear_x;
    float angular_z;
};

static std::shared_ptr<PublisherNode> g_turtle_pub_node;

static void ui_event_cmd_btn(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        TwistCommand * cmd = static_cast<TwistCommand *>(lv_event_get_user_data(e));
        if(cmd != nullptr && g_turtle_pub_node)
        {
            g_turtle_pub_node->publish_twist(cmd->linear_x, cmd->angular_z);
        }
    }
}

static lv_obj_t * ui_create_cmd_button(lv_obj_t * parent, const char * text, int x, int y, TwistCommand * cmd)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 90, 50);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(btn, ui_event_cmd_btn, LV_EVENT_ALL, cmd);
    return btn;
}



///////////////////// SCREEN init ////////////////////
// extern rclcpp::executors::SingleThreadedExecutor* g_executor;
void ui_ROSTest_init(void *arg)
{

    rclcpp::executors::SingleThreadedExecutor* executor = (rclcpp::executors::SingleThreadedExecutor*)arg;
    lv_obj_t * ROSTestPage = lv_obj_create(NULL);

    if(!g_turtle_pub_node)
    {
        g_turtle_pub_node = std::make_shared<PublisherNode>();
        if(executor)
        {
            executor->add_node(g_turtle_pub_node);
        }
    }

    // 创建返回按钮
    lv_obj_t * back_btn = lv_btn_create(ROSTestPage);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 10);
    lv_obj_set_size(back_btn, 75,40);
    lv_obj_t * back_btn_label = lv_label_create(back_btn);
    lv_label_set_text(back_btn_label,"Back");
    lv_obj_align(back_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(back_btn_label, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(back_btn, ui_event_back_btn, LV_EVENT_ALL, NULL);

    static TwistCommand cmd_forward = {1.0f, 0.0f};
    static TwistCommand cmd_backward = {-1.0f, 0.0f};
    static TwistCommand cmd_left = {0.0f, 1.0f};
    static TwistCommand cmd_right = {0.0f, -1.0f};
    static TwistCommand cmd_stop = {0.0f, 0.0f};

    ui_create_cmd_button(ROSTestPage, "Forward", 80, 80, &cmd_forward);
    ui_create_cmd_button(ROSTestPage, "Back", 80, 200, &cmd_backward);
    ui_create_cmd_button(ROSTestPage, "Left", 20, 140, &cmd_left);
    ui_create_cmd_button(ROSTestPage, "Right", 140, 140, &cmd_right);
    ui_create_cmd_button(ROSTestPage, "Stop", 80, 140, &cmd_stop);


    // load page
    lv_scr_load_anim(ROSTestPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_ROSTest_deinit()
{
    // deinit
    g_turtle_pub_node.reset();
}


void ui_ROSTest_button(lv_obj_t * ui_HomeScreen, lv_obj_t * ui_AppIconContainer, int x, int y, void (*button_cb)(lv_event_t *)){
    lv_obj_t * ui_DrawBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_DrawBtn, 70);
    lv_obj_set_height(ui_DrawBtn, 70);
    lv_obj_set_x(ui_DrawBtn, x);
    lv_obj_set_y(ui_DrawBtn, y);
    lv_obj_add_flag(ui_DrawBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_remove_flag(ui_DrawBtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_DrawBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_DrawBtn, lv_color_hex(0x86A8E5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_DrawBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_DrawBtn, &ui_img_paint60_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_DrawBtn, button_cb, LV_EVENT_CLICKED, (void *)"ROSTestPage");
}