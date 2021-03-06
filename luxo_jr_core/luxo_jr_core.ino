#ifndef LUXO_CORE_INO
#define LUXO_CORE_INO

#include "luxo_jr_core_config.h"

HardwareTimer Timer(TIMER_CH1);
RC100 remote_controller;
WheelDriver LuxoJrWheel;
LuxoJrController LuxoJrJoint;

int luxo_jr_dxl_present_pos[4] = {0, 0, 0, 0};
int luxo_jr_dxl_goal_pos[4] = {-REPEAT, -REPEAT, -REPEAT, -REPEAT}; //degree

float luxo_jr_dxl_present_rad[4] = {0.0, 0.0, 0.0, 0.0};
float luxo_jr_dxl_goal_rad[4] = {0.0, 0.0, 0.0, 0.0};

float joint_vel[4]= {0.0, 0.0, 0.0, 0.0};

float luxo_jr_linear_x = 0.0, luxo_jr_angular_z = 0.0,  const_cmd_vel = 0.0;
float mov_time = 1.5, ts = 0.008, luxo_jr_acc = 5.0, luxo_jr_max_vel = 5.0;
int mov_cnt = 0;
bool mov_stop = false;

void setup()
{
  Serial.begin(115200);
  while(!Serial);

  remote_controller.begin(1);

  LuxoJrWheel.init();
  LuxoJrJoint.init();
  timerInit();
}

void loop()
{
  receiveRemoteControl();

  // Serial.print("luxo_jr_linear_x : ");
  // Serial.print(luxo_jr_linear_x);
  // Serial.print(" luxo_jr_angular_z : ");
  // Serial.print(luxo_jr_angular_z);
  //
  // for (int id = 1; id < 2; id++)
  // {
  //   LuxoJrJoint.readPosition(id, &luxo_jr_present_pos[id-1]);
  //   Serial.print(" luxo_jr_present_pos : ");
  //   Serial.print(luxo_jr_present_pos[0]);
  //   Serial.print(" ");
  //   Serial.print(luxo_jr_present_pos[1]);
  //   Serial.print(" ");
  //   Serial.print(luxo_jr_present_pos[2]);
  //   Serial.print(" ");
  //   Serial.println(luxo_jr_present_pos[3]);
  // }
}

void timerInit()
{
  Timer.pause();
  Timer.setPeriod(CONTROL_PEROID);           // in microseconds
  Timer.attachInterrupt(handler_control);
  Timer.refresh();
  Timer.resume();
}

void handler_control(void)
{
  //controlMotorSpeed();

  if (mov_cnt < (mov_time/ts))
  {
    trapezoidalProfile(luxo_jr_acc, luxo_jr_max_vel, luxo_jr_dxl_goal_pos);
    mov_cnt++;
  }
  else
  {
    if (luxo_jr_dxl_goal_pos[0] == -REPEAT)
    {
      luxo_jr_dxl_goal_pos[0] = REPEAT; luxo_jr_dxl_goal_pos[1] = REPEAT;
      luxo_jr_dxl_goal_pos[2] = REPEAT; luxo_jr_dxl_goal_pos[3] = REPEAT;
    }
    else
    {
      luxo_jr_dxl_goal_pos[0] = -REPEAT; luxo_jr_dxl_goal_pos[1] = -REPEAT;
      luxo_jr_dxl_goal_pos[2] = -REPEAT; luxo_jr_dxl_goal_pos[3] = -REPEAT;
    }
    mov_cnt = 0;
  }
}

void receiveRemoteControl(void)
{
  int received_data = 0;

  if (remote_controller.available())
  {
    received_data = remote_controller.readData();

    if(received_data & RC100_BTN_U)
    {
      luxo_jr_linear_x  += VELOCITY_LINEAR_X;
    }
    else if(received_data & RC100_BTN_D)
    {
      luxo_jr_linear_x  -= VELOCITY_LINEAR_X;
    }
    else if(received_data & RC100_BTN_L)
    {
      luxo_jr_angular_z += VELOCITY_ANGULAR_Z;
    }
    else if(received_data & RC100_BTN_R)
    {
      luxo_jr_angular_z -= VELOCITY_ANGULAR_Z;
    }
    else if(received_data & RC100_BTN_1)
    {
      const_cmd_vel += VELOCITY_STEP;
    }
    else if(received_data & RC100_BTN_3)
    {
      const_cmd_vel -= VELOCITY_STEP;
    }
    else if(received_data & RC100_BTN_6)
    {
      luxo_jr_linear_x  = const_cmd_vel;
      luxo_jr_angular_z = 0.0;
    }
    else if(received_data & RC100_BTN_5)
    {
      luxo_jr_linear_x  = 0.0;
      luxo_jr_angular_z = 0.0;
    }
  }
}

void controlMotorSpeed(void)
{
  double wheel_speed_cmd[2];
  double lin_vel1;
  double lin_vel2;

  wheel_speed_cmd[LEFT]  = luxo_jr_linear_x - (luxo_jr_angular_z * WHEEL_SEPARATION / 2);
  wheel_speed_cmd[RIGHT] = luxo_jr_linear_x + (luxo_jr_angular_z * WHEEL_SEPARATION / 2);

  lin_vel1 = wheel_speed_cmd[LEFT] * VELOCITY_CONSTANT_VAULE;

  if (lin_vel1 > LIMIT_XM_MAX_VELOCITY)       lin_vel1 =  LIMIT_XM_MAX_VELOCITY;
  else if (lin_vel1 < -LIMIT_XM_MAX_VELOCITY) lin_vel1 = -LIMIT_XM_MAX_VELOCITY;

  lin_vel2 = wheel_speed_cmd[RIGHT] * VELOCITY_CONSTANT_VAULE;

  if (lin_vel2 > LIMIT_XM_MAX_VELOCITY)       lin_vel2 =  LIMIT_XM_MAX_VELOCITY;
  else if (lin_vel2 < -LIMIT_XM_MAX_VELOCITY) lin_vel2 = -LIMIT_XM_MAX_VELOCITY;

  LuxoJrWheel.speedControl((int64_t)lin_vel1, (int64_t)lin_vel2);
}

void trapezoidalProfile(float acc, float max_vel, int goal_pos[4])
{
  if (mov_cnt == 1)
  {
    for (int id = 1; id < MOTOR_NUM+1; id++)
    {
      LuxoJrJoint.readPosition(id, &luxo_jr_dxl_present_pos[id-1]);
    }
  }

  for (int id = 1; id < MOTOR_NUM+1; id++)
  {
    luxo_jr_dxl_present_rad[id-1] = LuxoJrJoint.convertValue2Radian(luxo_jr_dxl_present_pos[id-1]);
    luxo_jr_dxl_goal_rad[id-1] = goal_pos[id-1]*DEGREE2RADIAN;

    if (luxo_jr_dxl_goal_rad[id-1] > luxo_jr_dxl_present_rad[id-1])
    {
      joint_vel[id-1] = min(joint_vel[id-1] + (acc * ts), max_vel);
      joint_vel[id-1] = min(joint_vel[id-1], sqrt(2*acc*abs(luxo_jr_dxl_goal_rad[id-1] - luxo_jr_dxl_present_rad[id-1])));
      luxo_jr_dxl_present_rad[id-1] = luxo_jr_dxl_present_rad[id-1] + joint_vel[id-1] * ts;
      luxo_jr_dxl_present_pos[id-1] = LuxoJrJoint.convertRadian2Value(luxo_jr_dxl_present_rad[id-1]);

      Serial.println(joint_vel[id-1]);
      Serial.print(" ");
      //Serial.println(luxo_jr_dxl_present_pos[id-1]);
      //Serial.print(" ");

      LuxoJrJoint.positionControl(luxo_jr_dxl_present_pos);
    }
    else if (luxo_jr_dxl_goal_rad[id-1] < luxo_jr_dxl_present_rad[id-1])
    {
      joint_vel[id-1] = max(joint_vel[id-1] - (acc * ts), -max_vel);
      joint_vel[id-1] = max(joint_vel[id-1], -sqrt(2*acc*abs(luxo_jr_dxl_goal_rad[id-1] - luxo_jr_dxl_present_rad[id-1])));
      luxo_jr_dxl_present_rad[id-1] = luxo_jr_dxl_present_rad[id-1] + joint_vel[id-1] * ts;
      luxo_jr_dxl_present_pos[id-1] = LuxoJrJoint.convertRadian2Value(luxo_jr_dxl_present_rad[id-1]);

      Serial.println(joint_vel[id-1]);
      Serial.print(" ");
      //Serial.println(luxo_jr_dxl_present_pos[id-1]);
      //Serial.print(" ");

      LuxoJrJoint.positionControl(luxo_jr_dxl_present_pos);
    }
  }
}

#endif // LUXO_CORE_INO
