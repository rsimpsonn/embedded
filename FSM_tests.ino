bool test_transition(
  state start_state, 
  state end_state, 
  int virt_position, 
  int exp_position,
  int time_pos, 
  int mils,
  int saved_clock_v,
  bool cal_switch_on,
  bool timezone_inc, 
  bool timezone_dec) {
    saved_clock = saved_clock_v;
    StepperVirtPosition = virt_position;
    state next_state = update_fsm(start_state, mils, cal_switch_on, timezone_inc, timezone_dec, time_pos);
    return next_state == end_state && StepperVirtPosition == exp_position;
  }

  typedef struct {
    state start_state;
    state end_state;
    int virt_position;
    int exp_position;
    int time_pos;
    int mils;
    int saved_clock;
    bool cal_switch_on;
    bool timezone_inc;
    bool timezone_dec; 
  } test;

  const test tests[18] = {
    {s_CALIBRATING, s_CALIBRATING, 0, 0, 0, 0, 0, false, false, false},
    {s_CALIBRATING, s_CALIBRATING, 0, 0, 1000, 0, 1, false, false, false},
    {s_CALIBRATING, s_STEP, 0, 0, 0, 0, 0, true, false, false},
    {s_CALIBRATING, s_STEP, 120, 120, 0, 1000, 0, true, false, false},
    {s_STEP, s_STEP, 0, 1, 2, 10, 10, false, false, false},
    {s_STEP, s_STEP, 70, 71, 2, 10, 10, false, false, false},
    {s_STEP, s_STEP, 3, 2, 1, 10, 10, false, false, false},
    {s_STEP, s_STEP, 50, 49, 2, 10, 10, false, false, false},
    {s_STEP, s_WAITING, 0, 0, 0, 10, 10, false, false, false},
    {s_STEP, s_WAITING, 100, 100, 100, 10, 10, false, false, false},
    {s_WAITING, s_WAITING, 0, 0, 0, 100, 0, false, false, false},
    {s_WAITING, s_WAITING, 0, 0, 0, 110000, 0, false, false, false},
    {s_WAITING, s_STEP, 0, 0, 0, 120000, 0, false, false, false},
    {s_WAITING, s_STEP, 0, 0, 0, 140000, 0, false, false, false},
    {s_WAITING, s_STEP, 0, 0, 0, 1000, 0, false, true, false},
    {s_WAITING, s_STEP, 0, 0, 0, 0, 0, false, true, false},
    {s_WAITING, s_STEP, 0, 0, 0, 1000, 0, false, false, true},
    {s_WAITING, s_STEP, 0, 0, 0, 0, 0, false, false, true},
  };

  bool test_all_tests() {
    Serial.println("FSM UNIT TESTS");
    for (int i = 0; i < 18; i++) {
      test cur_test = tests[i];
      Serial.print("Running test ");
      Serial.print(i);
      Serial.print(" : ");
      Serial.println(test_transition(
        cur_test.start_state, 
        cur_test.end_state, 
        cur_test.virt_position,
        cur_test.exp_position,
        cur_test.time_pos,
        cur_test.mils,
        cur_test.saved_clock,
        cur_test.cal_switch_on,
        cur_test.timezone_inc,
        cur_test.timezone_dec
        )
        );
    }
    Serial.println("FSM UNIT TESTS COMPLETE");
  }
